#include "game.h"
#include "entity/unit.h"
#include "gui/griditem.h"
#include "gui/unititem.h"
#include <QGraphicsScene>
#include <QTimer>
#include <QtMath>
#include <algorithm>
#include <cstdlib>

namespace {
// 场景分层：格子 < 单位 < 拖拽中的单位。
// constexpr：编译时常量，确保在编译阶段就确定数值，避免运行时开销。
// qreal：Qt定义的浮点类型，通常是 double，提供平台无关的精度保证。
// Z值用于控制图元的渲染层级，数值越大越靠前显示。
constexpr qreal kZGrid = 0.0;
constexpr qreal kZUnit = 1.0;
constexpr qreal kZDraggingUnit = 2.0;
}

Game::Game(QObject* parent)
    : QObject(parent)
    , m_scene(new QGraphicsScene(this))
    , m_dragActive(false)
    , m_activeUnitId(-1)
    , m_sourceGrid(-1, -1)
    , m_rows(Board::ROWS)
    , m_cols(Board::COLS)
    , m_radius(46.0)
    , m_rowSpacing(69.0)
    , m_combatTimer(new QTimer(this))
{
    m_combatTimer->setInterval(1000); // 300ms per unit action
    connect(m_combatTimer, &QTimer::timeout, this, &Game::combatTick);
}

Game::~Game()
{
    qDeleteAll(m_units);
    m_units.clear();
}

void Game::initialize()
{
    reset();
}

void Game::reset()
{
    // 停止可能正在进行的战斗
    if (m_combatTimer->isActive()) {
        m_combatTimer->stop();
    }

    // 彻底清空棋盘和备战区逻辑
    m_board.clear();
    m_bench.clear();

    // 完全重置状态为重新开始游戏的状态
    m_phase = Phase::Prep;
    m_combatUnitIndex = -1;
    m_actingUnitId = -1;
    m_playerState.round = 1;
    m_playerState.hp = 10; // 重置为初始满血
    m_playerState.gold = 0; // 重置金币为 0
    m_playerState.level = 1;
    m_playerState.boardCap = 7;

    // 彻底销毁当前所有残留的单位对象内存（不论敌我）
    qDeleteAll(m_units);
    m_units.clear();

    // 重新生成开局的原始3个我方单位
    createStarterUnitsIfNeeded();

    // 重建场景图元：这会销毁所有处于图像容器的旧单位和网格，并绘制新的干净UI
    buildScene();

    // 仅将最开始的3个玩家单位按固定占位进行开场布阵
    const QPoint initialPositions[] = {
        QPoint(0, 7),
        QPoint(1, 7),
        QPoint(2, 7)
    };

    for (int i = 0; i < m_units.size() && i < 3; ++i) {
        Unit* u = m_units[i];
        m_board.addUnit(u, initialPositions[i]);
        u->setPosition(initialPositions[i]);
    }

    // 重新生成第一波敌人以匹配 round = 1 的状况
    spawnEnemyWave();

    // 强制界面的所有状态UI和棋盘实体位置更新一致
    syncFromBoard();
    emit stateUpdated();
    emit phaseChanged(m_phase);
}

void Game::combatTick()
{
    // 清除上一个行动单位的高亮
    if (m_actingUnitId != -1) {
        UnitItem* prev = findUnitItem(m_actingUnitId);
        if (prev) prev->setHighlighted(false);
        m_actingUnitId = -1;
    }

    // 每 tick 只让一个单位行动，轮流循环
    int totalUnits = m_units.size();
    for (int i = 0; i < totalUnits; ++i) {
        m_combatUnitIndex = (m_combatUnitIndex + 1) % totalUnits;
        Unit* u = m_units[m_combatUnitIndex];
        if (u->get_isAlive() && m_board.hasUnitAt(u->position())) {
            u->action(*this);

            // 高亮当前行动单位
            UnitItem* item = findUnitItem(u->id());
            if (item) item->setHighlighted(true);
            m_actingUnitId = u->id();
            break;
        }
    }

    cleanupDeadUnits();
    syncFromBoard();

    // 判断是否有一方全部阵亡
    bool enemyAlive = false;
    bool playerAlive = false;
    for (Unit* u : m_units) {
        if (u->get_isAlive() && m_board.hasUnitAt(u->position())) {
            if (u->get_owner() == Owner::PlayerCtrl) playerAlive = true;
            if (u->get_owner() == Owner::EnemyCtrl) enemyAlive = true;
        }
    }

    if (!enemyAlive || !playerAlive) {
        m_combatTimer->stop();
        // 清除高亮
        if (m_actingUnitId != -1) {
            UnitItem* prev = findUnitItem(m_actingUnitId);
            if (prev) prev->setHighlighted(false);
            m_actingUnitId = -1;
        }
        advancePhase();
    }
}

void Game::advancePhase()
{
    if (m_phase == Phase::Prep) {
        m_phase = Phase::Combat;
        // 重置战斗轮转索引
        m_combatUnitIndex = -1;
        m_actingUnitId = -1;
        // 记录战斗前的位置
        m_preCombatPositions.clear();
        for (Unit* u : m_units) {
            if (u->get_owner() == Owner::PlayerCtrl && m_board.hasUnitAt(u->position())) {
                m_preCombatPositions[u] = u->position();
            }
        }
        emit phaseChanged(m_phase);
        m_combatTimer->start();
    } else if (m_phase == Phase::Combat) {
        m_phase = Phase::Resolve;
        resolveCombat();
        emit phaseChanged(m_phase);
        emit stateUpdated();
        QTimer::singleShot(2000, this, [this](){ advancePhase(); });
    } else if (m_phase == Phase::Resolve) {
        m_phase = Phase::Prep;
        m_playerState.round++;
        resetUnitsToPrep();
        spawnEnemyWave(); // 刷新下一轮的敌人
        syncFromBoard(); // 将新生成的敌方单位图元同步到正确棋盘位置
        emit phaseChanged(m_phase);
        emit stateUpdated();
    }
}

void Game::resolveCombat()
{
    int enemyAliveCount = 0;
    int playerAliveCount = 0;
    
    for (Unit* u : m_units) {
        if (u->get_isAlive()) {
            if (u->get_owner() == Owner::EnemyCtrl) {
                enemyAliveCount++;
            } else if (u->get_owner() == Owner::PlayerCtrl && m_board.hasUnitAt(u->position())) {
                playerAliveCount++;
            }
        }
    }

    if (enemyAliveCount == 0) {
        // 胜利
        m_playerState.gold += 15;
    } else {
        // 失败（或平局）
        m_playerState.gold += 10;
        m_playerState.hp -= enemyAliveCount;
        if (m_playerState.hp < 0) {
            m_playerState.hp = 0;
        }
    }
}

void Game::resetUnitsToPrep()
{
    // 将所有我方上阵单位恢复到战斗前的位置，并重置状态
    m_board.clear(); // 清空棋盘重新摆放

    for (int i = 0; i < m_units.size(); ) {
        Unit* u = m_units[i];
        if (!u) {
            m_units.removeAt(i);
            continue;
        }

        if (u->get_owner() == Owner::EnemyCtrl) {
            // 敌人单位在准备阶段会被彻底清空
            auto it = m_unitItemById.find(u->id());
            if (it != m_unitItemById.end()) {
                m_scene->removeItem(it->second);
                m_unitItems.erase(std::remove(m_unitItems.begin(), m_unitItems.end(), it->second), m_unitItems.end());
                delete it->second;
                m_unitItemById.erase(it);
            }
            delete u;
            m_units.removeAt(i);
        } else {
            // 重置我方单位属性
            u->set_isAlive(true);
            u->set_hp(u->get_maxHp());
            u->set_mana(0);
            u->set_isDizzy(false);
            u->set_hasTarget(false);
            u->set_Target(nullptr);
            
            if (m_preCombatPositions.contains(u)) {
                // 如果参战了，放回原处。addUnit 操作会自动覆盖并统一 u->position()
                m_board.addUnit(u, m_preCombatPositions[u]);
            }
            ++i;
        }
    }
    
    syncFromBoard();
}

void Game::handleDragStarted(int unitId, const QPoint& sourceGrid, const QPointF&)
{
    // 记录拖拽源信息，并提升当前单位层级避免被遮挡。
    m_dragActive = true;
    m_activeUnitId = unitId;
    m_sourceGrid = sourceGrid;

    UnitItem* item = findUnitItem(unitId);
    if (item) {
        item->setZValue(kZDraggingUnit);
    }
}

void Game::handleDragMoved(int unitId, const QPoint&, const QPointF& scenePos)
{
    if (!m_dragActive) {
        return;
    }

    // 每次移动先清除旧高亮，再对候选目标格进行反馈。
    clearGridHighlights();

    const QPoint target = worldToGrid(scenePos);
    GridItem* targetItem = findGridItem(target);
    if (!targetItem) {
        return;
    }

    targetItem->setHoverActive(true);

    if (canApplyDrop(unitId, m_sourceGrid, target)) {
        targetItem->setDropActive(true);
    }
}

void Game::handleDropCommand(int unitId, const QPoint& sourceGrid, const QPointF& scenePos)
{
    if (!m_dragActive) {
        return;
    }

    // 落点合法则更新棋盘，否则保持原位并仅重置拖拽状态。
    const QPoint target = worldToGrid(scenePos);

    clearGridHighlights();
    if (canApplyDrop(unitId, sourceGrid, target)) {
        applyDrop(unitId, target);
    }

    UnitItem* item = findUnitItem(m_activeUnitId);
    if (item) {
        item->setZValue(kZUnit);
    }

    m_dragActive = false;
    m_activeUnitId = -1;
    m_sourceGrid = QPoint(-1, -1);

    syncFromBoard();
}

void Game::createStarterUnitsIfNeeded()
{
    if (!m_units.isEmpty()) {
        return;
    }

    // 当前 starter 仅放置 3 个示例单位，后续可替换为配置化生成。
    m_units.append(new Warrior("战士", Owner::PlayerCtrl));
    m_units.append(new Archer("弓手", Owner::PlayerCtrl));
    m_units.append(new Mage("法师", Owner::PlayerCtrl));
}

void Game::spawnEnemyWave()
{
    // Start with 2 enemies, +2 each round, up to 8 max.
    int enemyCount = std::min(8, m_playerState.round * 2);

    for (int i = 0; i < enemyCount; ++i) {
        Unit* enemy = nullptr;
        int type = rand() % 3;
        if (type == 0) enemy = new Warrior("Enemy Warrior", Owner::EnemyCtrl);
        else if (type == 1) enemy = new Archer("Enemy Archer", Owner::EnemyCtrl);
        else enemy = new Mage("Enemy Mage", Owner::EnemyCtrl);

        m_units.append(enemy);

        // 创建图元并关联信号
        UnitItem* unitItem = new UnitItem(enemy);
        unitItem->setZValue(kZUnit);
        m_scene->addItem(unitItem);
        m_unitItems.push_back(unitItem);
        m_unitItemById[enemy->id()] = unitItem;

        connect(unitItem, &UnitItem::dragStarted, this, &Game::handleDragStarted);
        connect(unitItem, &UnitItem::dragMoved, this, &Game::handleDragMoved);
        connect(unitItem, &UnitItem::dragDropped, this, &Game::handleDropCommand);

        // Randomize placement on enemy half of the board (rows 0 to 3)
        int r, c;
        do {
            r = rand() % (Board::ROWS / 2); // 在敌方半场随机选择行
            c = rand() % Board::COLS;
        } while (m_board.hasUnitAt(QPoint(c, r)));
        
        m_board.addUnit(enemy, QPoint(c, r));
    }
}

Unit* Game::findUnitById(int unitId) const
{
    for (Unit* unit : m_units) {
        if (unit && unit->id() == unitId) {
            return unit;
        }
    }
    return nullptr;
}

GridItem* Game::findGridItem(const QPoint& gridPos) const
{
    for (GridItem* item : m_gridItems) {
        if (item && item->gridPos() == gridPos) {
            return item;
        }
    }
    return nullptr;
}

bool Game::isBenchPos(const QPoint& pos) const 
{
    return pos.y() == Board::ROWS + 1 && pos.x() >= 0 && pos.x() < m_bench.getMaxBenchSize();
}

Unit* Game::getUnitAtPos(const QPoint& pos) const
{
    if (m_board.isValidPosition(pos)) {
        return m_board.getUnitAt(pos);
    } else if (isBenchPos(pos)) {
        return m_bench.getUnits().value(pos.x(), nullptr);
    }
    return nullptr;
}

UnitItem* Game::findUnitItem(int unitId) const
{
    auto it = m_unitItemById.find(unitId);
    if (it == m_unitItemById.end()) {
        return nullptr;
    }
    return it->second;
}

void Game::clearGridHighlights()
{
    for (GridItem* item : m_gridItems) {
        if (!item) {
            continue;
        }
        item->setHoverActive(false);
        item->setDropActive(false);
    }
}

void Game::cleanupDeadUnits()
{
    // 遍历所有单位，如果已经死亡且还在棋盘上，就从棋盘占位上移除
    for (Unit* u : m_units) {
        if (!u) continue;
        if (!u->get_isAlive()) {
            if (m_board.hasUnitAt(u->position()) && m_board.getUnitAt(u->position()) == u) {
                m_board.removeUnit(u);
            }
        }
    }
    syncFromBoard(); // 同步 UI 显示（这会将死亡单位隐藏起来）
}

bool Game::canApplyDrop(int unitId, const QPoint& source, const QPoint& target) const
{
    Unit* unit = findUnitById(unitId);
    if (!unit) {
        return false;
    }

    bool sourceValid = m_board.isValidPosition(source) || isBenchPos(source);
    bool targetValid = m_board.isValidPosition(target) || isBenchPos(target);

    if (!sourceValid || !targetValid || source == target) {
        return false;
    }

    // 仅允许在玩家半场或备战区内部调整站位。
    bool sourcePlayer = isBenchPos(source) || m_board.isPlayerHalf(source);
    bool targetPlayer = isBenchPos(target) || m_board.isPlayerHalf(target);

    if (!sourcePlayer || !targetPlayer) {
        return false;
    }

    return getUnitAtPos(source) == unit;
}

void Game::applyDrop(int unitId, const QPoint& target)
{
    Unit* unit = findUnitById(unitId);
    if (!unit) {
        return;
    }

    // 根据 source 和 target 判断是 board-board, bench-bench, board-bench, 还是 bench-board
    bool sourceIsBoard = m_board.isValidPosition(m_sourceGrid);
    bool targetIsBoard = m_board.isValidPosition(target);

    if (sourceIsBoard && targetIsBoard) {
        m_board.moveUnit(m_sourceGrid, target);
    } else if (!sourceIsBoard && !targetIsBoard) {
        m_bench.moveBenchToBench(m_sourceGrid.x(), target.x());
    } else if (!sourceIsBoard && targetIsBoard) {
        m_bench.moveToBoard(m_board, m_sourceGrid.x(), target);
    } else if (sourceIsBoard && !targetIsBoard) {
        m_bench.moveFromBoard(m_board, m_sourceGrid, target.x());
    }

    // 确保被挪动（互换）的棋子更新 position 字段
    // 因为涉及可能两单位互换位置，我们这里简单粗暴刷新整个棋盘和备战区单位的位置信息
    for (int r = 0; r < Board::ROWS; ++r) {
        for (int c = 0; c < Board::COLS; ++c) {
            Unit* u = m_board.getUnitAt(QPoint(c, r));
            if (u) {
                u->setPosition(QPoint(c, r));
            }
        }
    }
    for (int i = 0; i < m_bench.getMaxBenchSize(); ++i) {
        Unit* u = m_bench.getUnits().value(i, nullptr);
        if (u) {
            u->setPosition(QPoint(i, Board::ROWS + 1));
        }
    }
}

void Game::buildScene()
{
    // 重建场景时清空旧图元与映射，避免悬挂引用。
    m_scene->clear();
    m_gridItems.clear();
    m_unitItems.clear();
    m_unitItemById.clear();

    QRectF totalBounds;
    bool first = true;
    // 创建棋盘格图元。
    for (int row = 0; row < Board::ROWS; ++row) {
        for (int col = 0; col < Board::COLS; ++col) {
            const QPolygonF poly = cellHexPolygon(row, col);
            GridItem* gridItem = new GridItem(row, col, poly);
            gridItem->setZValue(kZGrid);
            gridItem->setBaseColor(row < Board::ROWS / 2 ? QColor(80, 60, 60) : QColor(60, 60, 80));

            m_scene->addItem(gridItem);
            m_gridItems.push_back(gridItem);

            const QRectF bounds = gridItem->boundingRect();
            totalBounds = first ? bounds : totalBounds.united(bounds);
            first = false;
        }
    }

    // 创建备战区图元 (row = Board::ROWS + 1, col = 0..7)
    for (int col = 0; col < m_bench.getMaxBenchSize(); ++col) {
        const QPolygonF poly = cellHexPolygon(Board::ROWS + 1, col);
        GridItem* gridItem = new GridItem(Board::ROWS + 1, col, poly);
        gridItem->setZValue(kZGrid);
        gridItem->setBaseColor(QColor(50, 60, 50));

        m_scene->addItem(gridItem);
        m_gridItems.push_back(gridItem);

        const QRectF bounds = gridItem->boundingRect();
        totalBounds = totalBounds.united(bounds);
    }

    // 创建单位图元并接入拖拽信号。
    for (Unit* unit : m_units) {
        UnitItem* unitItem = new UnitItem(unit);
        unitItem->setZValue(kZUnit);
        m_scene->addItem(unitItem);
        m_unitItems.push_back(unitItem);
        m_unitItemById[unit->id()] = unitItem;

        connect(unitItem, &UnitItem::dragStarted,
                this, &Game::handleDragStarted);
        connect(unitItem, &UnitItem::dragMoved,
                this, &Game::handleDragMoved);
        connect(unitItem, &UnitItem::dragDropped,
                this, &Game::handleDropCommand);
    }

    m_scene->setSceneRect(totalBounds.adjusted(-40, -40, 40, 40));
}

void Game::syncFromBoard()
{
    // 统一回写单位可见性与坐标。
    clearGridHighlights();

    for (UnitItem* item : m_unitItems) {
        if (!item || !item->unit()) {
            continue;
        }

        const QPoint pos = item->unit()->position();
        bool validOnBoard = m_board.isValidPosition(pos) && m_board.getUnitAt(pos) == item->unit();
        bool validOnBench = isBenchPos(pos) && m_bench.getUnits().value(pos.x(), nullptr) == item->unit();
        
        if (!validOnBoard && !validOnBench) {
            item->setVisible(false);
            continue;
        }

        item->setVisible(true);
        item->setGridPos(pos);
        item->setPos(gridToWorld(pos.y(), pos.x()));
        item->setZValue(kZUnit);
        
        // finish: TODO[T1-3]: 同步单位血条/蓝条/属性面板到图元。
        item->update(); // 通知图元重绘以更新血条蓝条
    }
}

QPointF Game::gridToWorld(int row, int col) const
{
    // 偶数行水平偏移 half-cell，形成错列六边形布局。
    const qreal colSpacing = m_radius * qSqrt(3.0);
    const qreal xOffset = (row % 2 == 0) ? colSpacing * 0.5 : 0.0;
    const qreal x = xOffset + col * colSpacing;
    const qreal y = row * m_rowSpacing;
    return QPointF(x, y);
}

QPoint Game::worldToGrid(const QPointF& world) const
{
    // 通过最近中心点匹配将场景坐标反解为网格坐标。包括行数等于 m_rows + 1 的备战区。
    QPoint best(-1, -1);
    qreal bestDist = 1e18;

    for (int row = 0; row <= m_rows + 1; ++row) {
        for (int col = 0; col < m_cols; ++col) {
            const QPointF center = gridToWorld(row, col);
            const qreal dx = world.x() - center.x();
            const qreal dy = world.y() - center.y();
            const qreal d2 = dx * dx + dy * dy;
            if (d2 < bestDist) {
                bestDist = d2;
                best = QPoint(col, row);
            }
        }
    }

    return best;
}

QPolygonF Game::cellHexPolygon(int row, int col) const
{
    // 基于中心点+半径生成正六边形顶点。
    const QPointF center = gridToWorld(row, col);
    QPolygonF poly;
    poly.reserve(6);

    for (int i = 0; i < 6; ++i) {
        const qreal angleDeg = 60.0 * i - 90.0;
        const qreal angleRad = qDegreesToRadians(angleDeg);
        poly.append(QPointF(
            center.x() + m_radius * qCos(angleRad),
            center.y() + m_radius * qSin(angleRad)
        ));
    }

    return poly;
}
