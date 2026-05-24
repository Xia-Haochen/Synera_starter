#include "game.h"
#include "entity/unit.h"
#include "gui/griditem.h"
#include "gui/unititem.h"
#include "gui/equipslotitem.h"
#include "gui/equipitem.h"
#include <QGraphicsScene>
#include <QTimer>
#include <QtMath>
#include <algorithm>
#include <cstdlib>

namespace {
// 场景分层：格子 < 单位 < 拖拽中的单位。
constexpr qreal kZGrid = 0.0;
constexpr qreal kZUnit = 1.0;
constexpr qreal kZDraggingUnit = 2.0;

Equipment* createEquipmentFromType(EquipmentType type) {
    switch (type) {
        case EquipmentType::IronSword:   return new IronSword();
        case EquipmentType::Chainmail:   return new Chainmail();
        case EquipmentType::HasteGloves: return new HasteGloves();
        case EquipmentType::Sapphire:    return new Sapphire();
    }
    return nullptr;
}
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
    m_combatTimer->setInterval(500); // 500ms per unit action
    connect(m_combatTimer, &QTimer::timeout, this, &Game::combatTick);
}

Game::~Game()
{
    for (auto* eq : m_equipBar) delete eq;
    m_equipBar.clear();
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
    m_hasteSecondAction = false;
    m_combatTimer->setInterval(500);
    m_playerState.round = 1;
    m_playerState.hp = PlayerState().hp ; // 重置为初始满血
    m_playerState.gold = PlayerState().gold; // 重置金币为初始值
    m_playerState.level = PlayerState().level; // 重置等级为初始值
    m_playerState.boardCap = PlayerState().boardCap; // 重置人口上限为初始值

    // 彻底销毁当前所有残留的单位对象内存（不论敌我）
    qDeleteAll(m_units);
    m_units.clear();

    // 清空装备栏
    for (auto* eq : m_equipBar) delete eq;
    m_equipBar.clear();

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

    // 初始化商店
    m_shop.rollShop();

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

    bool isBonusAction = m_hasteSecondAction;
    if (m_hasteSecondAction) {
        m_hasteSecondAction = false;
        m_combatTimer->setInterval(500); // 恢复正常间隔
    } else {
        // 寻找下一个存活且在场上的单位
        for (int i = 0; i < totalUnits; ++i) {
            m_combatUnitIndex = (m_combatUnitIndex + 1) % totalUnits;
            Unit* u = m_units[m_combatUnitIndex];
            if (u && u->get_isAlive() && m_board.hasUnitAt(u->position())) {
                break;
            }
        }
    }

    if (m_combatUnitIndex >= 0 && m_combatUnitIndex < totalUnits) {
        Unit* u = m_units[m_combatUnitIndex];
        if (u && u->get_isAlive() && m_board.hasUnitAt(u->position())) {
            u->action(*this);

            // 急速手套：仅在非额外行动时触发双动
            if (!isBonusAction && u->has_haste_gloves()) {
                m_hasteSecondAction = true;
                m_combatTimer->setInterval(250);
            }

            // 高亮当前行动单位
            UnitItem* item = findUnitItem(u->id());
            if (item) item->setHighlighted(true);
            m_actingUnitId = u->id();
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
        m_shop.rollShop(); // 每回合自动刷新商店
        rollEquipmentDrop(); // 两次装备掉落判定
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
        if (m_playerState.round >= 8) {
            emit gameEnded(true);
        }
    } else {
        // 失败（或平局）
        m_playerState.gold += 10;
        m_playerState.hp -= enemyAliveCount;
        if (m_playerState.hp <= 0) {
            m_playerState.hp = 0;
            emit gameEnded(false);
        }
    }
}

void Game::resetUnitsToPrep()
{
    m_hasteSecondAction = false;
    m_combatTimer->setInterval(500);

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
    if (m_phase != Phase::Prep) return;

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
    if (!m_dragActive || m_phase != Phase::Prep) {
        return;
    }

    // 每次移动先清除旧高亮，再对候选目标格进行反馈。
    clearGridHighlights();

    // 检查是否移动到出售区域
    if (m_sellZoneItem) {
        QPointF diff = scenePos - m_sellZoneCenter;
        qreal dist = qSqrt(diff.x() * diff.x() + diff.y() * diff.y());
        if (dist < m_radius * 1.5) {
            m_sellZoneItem->setDropActive(true);
            return;
        }
    }

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
    if (!m_dragActive || m_phase != Phase::Prep) {
        m_dragActive = false; // abort any ongoing drag
        return;
    }

    clearGridHighlights();

    // 检查是否出售到出售区域
    if (m_sellZoneItem) {
        QPointF diff = scenePos - m_sellZoneCenter;
        qreal dist = qSqrt(diff.x() * diff.x() + diff.y() * diff.y());
        if (dist < m_radius * 1.5) {
            sellUnit(unitId, sourceGrid);
            UnitItem* item = findUnitItem(m_activeUnitId);
            if (item) {
                item->setZValue(kZUnit);
            }
            m_dragActive = false;
            m_activeUnitId = -1;
            m_sourceGrid = QPoint(-1, -1);
            syncFromBoard();
            return;
        }
    }

    // 落点合法则更新棋盘，否则保持原位并仅重置拖拽状态。
    const QPoint target = worldToGrid(scenePos);

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

    // 检查人口上限
    bool sourceIsBench = isBenchPos(source);
    bool targetIsBoard = m_board.isPlayerHalf(target);
    if (sourceIsBench && targetIsBoard) {
        Unit* targetUnit = getUnitAtPos(target);
        if (!targetUnit) {
            // 从备战区移动到场上空位，需要检查人口
            int boardCount = 0;
            for (Unit* u : m_units) {
                if (u->get_owner() == Owner::PlayerCtrl && m_board.isPlayerHalf(u->position())) {
                    boardCount++;
                }
            }
            if (boardCount >= m_playerState.boardCap) {
                return false;
            }
        }
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

void Game::buyFromShopSlot(int slotIndex)
{
    if (m_phase != Phase::Prep) return;
    if (slotIndex < 0 || slotIndex >= Shop::SHOP_SIZE) return;
    if (m_shop.isSlotEmpty(slotIndex)) return;
    if (m_playerState.gold < Shop::UNIT_COST) return;
    if (m_bench.getUnitCount() >= m_bench.getMaxBenchSize()) return;

    // 扣除金币
    m_playerState.gold -= Shop::UNIT_COST;

    // 根据商店槽位的职业创建单位
    JobType job = m_shop.getSlot(slotIndex);
    Unit* newUnit = nullptr;
    switch (job) {
        case JobType::Warrior:
            newUnit = new Warrior("战士", Owner::PlayerCtrl);
            break;
        case JobType::Mage:
            newUnit = new Mage("法师", Owner::PlayerCtrl);
            break;
        case JobType::Archer:
            newUnit = new Archer("弓手", Owner::PlayerCtrl);
            break;
    }

    if (!newUnit) return;

    m_units.append(newUnit);

    // 创建图元
    UnitItem* unitItem = new UnitItem(newUnit);
    unitItem->setZValue(kZUnit);
    m_scene->addItem(unitItem);
    m_unitItems.push_back(unitItem);
    m_unitItemById[newUnit->id()] = unitItem;

    connect(unitItem, &UnitItem::dragStarted,
            this, &Game::handleDragStarted);
    connect(unitItem, &UnitItem::dragMoved,
            this, &Game::handleDragMoved);
    connect(unitItem, &UnitItem::dragDropped,
            this, &Game::handleDropCommand);

    // 放入备战区
    m_bench.addUnit(newUnit);

    // 设置单位在备战区的位置（syncFromBoard 依赖 position 判断可见性）
    for (int i = 0; i < m_bench.getMaxBenchSize(); ++i) {
        if (m_bench.getUnits().value(i, nullptr) == newUnit) {
            newUnit->setPosition(QPoint(i, Board::ROWS + 1));
            break;
        }
    }

    // 标记商店槽位为空
    m_shop.clearSlot(slotIndex);

    checkAndMerge(newUnit);

    syncFromBoard();
    emit stateUpdated();
}

void Game::buyBoardCap()
{
    if (m_phase != Phase::Prep) return;
    static const int BOARD_CAP_COST = 3;
    if (m_playerState.gold < BOARD_CAP_COST) return;

    m_playerState.gold -= BOARD_CAP_COST;
    m_playerState.boardCap++;
    emit stateUpdated();
}

void Game::checkAndMerge(Unit* newUnit)
{
    if (!newUnit) return;
    int targetStar = newUnit->get_starLevel();
    if (targetStar >= 3) return; // 最高3星

    // 查找同一阵营、相同职业、相同星级的单位
    QList<Unit*> identicalUnits;
    for (Unit* u : m_units) {
        if (u->get_owner() == Owner::PlayerCtrl && 
            u->get_job() == newUnit->get_job() && 
            u->get_starLevel() == targetStar) {
            identicalUnits.append(u);
        }
    }

    if (identicalUnits.size() >= 3) {
        // 保留刚获得的单位，移除其他两个
        Unit* keepUnit = newUnit;
        QList<Unit*> removeUnits;
        int removedCount = 0;
        for (Unit* u : identicalUnits) {
            if (u != keepUnit && removedCount < 2) {
                removeUnits.append(u);
                removedCount++;
            }
        }

        if (removedCount == 2) {
            // 移除这两个单位
            for (Unit* u : removeUnits) {
                QPoint p = u->position();
                if (isBenchPos(p)) {
                    m_bench.removeUnit(u);
                } else {
                    m_board.removeUnit(u);
                }
                m_units.removeOne(u);
                if (m_unitItemById.count(u->id())) {
                    UnitItem* item = m_unitItemById[u->id()];
                    m_scene->removeItem(item);
                    m_unitItemById.erase(u->id());
                    m_unitItems.erase(std::remove(m_unitItems.begin(), m_unitItems.end(), item), m_unitItems.end());
                    delete item;
                }
                // 退还装备到装备栏
                std::vector<Equipment*> unitEqs = u->get_equipments();
                for (auto* eq : unitEqs) {
                    u->remove_equipment(eq);
                    if (m_equipBar.size() < 5) {
                        m_equipBar.push_back(eq);
                    } else {
                        delete eq;
                    }
                }
                delete u;
            }

            // 升星处理
            keepUnit->set_starLevel(targetStar + 1);
            // 基础属性翻倍，重新结算羁绊并恢复满血
            keepUnit->set_baseMaxHp(keepUnit->get_baseMaxHp() * 2);
            keepUnit->set_baseAtk(keepUnit->get_baseAtk() * 2);
            keepUnit->applyTraitEffects(); // 更新加成后的属性
            keepUnit->set_hp(keepUnit->get_maxHp());

            // 递归检查是否能继续合成（如3个2星合1个3星）
            checkAndMerge(keepUnit);
        }
    }
}

void Game::rollShop()
{
    if (m_phase != Phase::Prep) return;
    if (m_playerState.gold < Shop::REFRESH_COST) return;
    m_playerState.gold -= Shop::REFRESH_COST;
    m_shop.rollShop();
    emit stateUpdated();
}

void Game::sellUnit(int unitId, const QPoint& sourceGrid)
{
    Unit* unit = findUnitById(unitId);
    if (!unit || unit->get_owner() != Owner::PlayerCtrl) return;

    // 从棋盘或备战区移除
    if (m_board.isValidPosition(sourceGrid)) {
        if (m_board.getUnitAt(sourceGrid) != unit) return;
        m_board.removeUnit(unit);
    } else if (isBenchPos(sourceGrid)) {
        if (m_bench.getUnits().value(sourceGrid.x(), nullptr) != unit) return;
        m_bench.removeUnit(unit);
    } else {
        return;
    }

    // 移除图元
    UnitItem* item = findUnitItem(unitId);
    if (item) {
        m_scene->removeItem(item);
        m_unitItems.erase(std::remove(m_unitItems.begin(), m_unitItems.end(), item), m_unitItems.end());
        m_unitItemById.erase(unitId);
        delete item;
    }

    // 退还装备到装备栏
    std::vector<Equipment*> unitEqs = unit->get_equipments();
    for (auto* eq : unitEqs) {
        unit->remove_equipment(eq);
        if (m_equipBar.size() < 5) {
            m_equipBar.push_back(eq);
        } else {
            delete eq;
        }
    }

    // 移除单位
    m_units.removeOne(unit);
    delete unit;

    // 获得出售金币
    m_playerState.gold += Shop::SELL_PRICE;

    syncFromBoard();
    emit stateUpdated();
}

void Game::buildScene()
{
    // 重建场景时清空旧图元与映射，避免悬挂引用。
    m_scene->clear();
    m_gridItems.clear();
    m_unitItems.clear();
    m_unitItemById.clear();
    m_equipSlotItems.clear();
    m_equipUIItems.clear();

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

    // 创建出售区域（黄色六边形）- 放在备战区右侧
    {
        int sellRow = Board::ROWS + 1;
        int sellCol = Board::COLS;
        const QPolygonF sellPoly = cellHexPolygon(sellRow, sellCol);
        GridItem* sellItem = new GridItem(sellRow, sellCol, sellPoly);
        sellItem->setZValue(kZGrid);
        sellItem->setBaseColor(QColor(180, 180, 50));
        sellItem->setSellZone(true);
        m_scene->addItem(sellItem);
        m_gridItems.push_back(sellItem);
        m_sellZoneItem = sellItem;
        m_sellZoneCenter = gridToWorld(sellRow, sellCol);

        const QRectF sellBounds = sellItem->boundingRect();
        totalBounds = totalBounds.united(sellBounds);
    }

    // 创建装备栏槽位
    qreal startX = gridToWorld(Board::ROWS + 1, 0).x();
    qreal yBase = gridToWorld(Board::ROWS + 1, 0).y() + 80.0;
    for (int i = 0; i < 5; ++i) {
        QRectF rect(startX + i * 50, yBase, 40, 40);
        EquipSlotItem* slot = new EquipSlotItem(i, rect);
        m_scene->addItem(slot);
        m_equipSlotItems.push_back(slot);
        totalBounds = totalBounds.united(slot->boundingRect());
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
    if (m_phase == Phase::Prep) {
        updateTraits();
    }
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
        item->update();
    }

    // Sync equipItems — use deleteLater to avoid use-after-free
    // when syncFromBoard is called from within an EquipItem signal handler.
    for (auto* eqItem : m_equipUIItems) {
        m_scene->removeItem(eqItem);
        eqItem->deleteLater();
    }
    m_equipUIItems.clear();

    for (int i = 0; i < m_equipBar.size(); ++i) {
        Equipment* eq = m_equipBar[i];
        if (i < m_equipSlotItems.size()) {
            EquipItem* item = new EquipItem(eq);
            item->setSlotIndex(i);
            item->setPos(m_equipSlotItems[i]->pos() + QPointF(m_equipSlotItems[i]->boundingRect().topLeft()) + QPointF(20, 20));
            m_scene->addItem(item);
            m_equipUIItems.push_back(item);
            
            connect(item, &EquipItem::dragStarted, this, &Game::handleEquipDragStarted);
            connect(item, &EquipItem::dragMoved, this, &Game::handleEquipDragMoved);
            connect(item, &EquipItem::dragDropped, this, &Game::handleEquipDropCommand);
        }
    }

    for (Unit* u : m_units) {
        if (!u->get_isAlive()) continue;
        bool validOnBoard = m_board.isValidPosition(u->position()) && m_board.getUnitAt(u->position()) == u;
        bool validOnBench = isBenchPos(u->position()) && m_bench.getUnits().value(u->position().x(), nullptr) == u;
        if (!validOnBoard && !validOnBench) continue;
        
        auto eqs = u->get_equipments();
        for (int i = 0; i < eqs.size(); ++i) {
            EquipItem* item = new EquipItem(eqs[i]);
            item->setSlotIndex(-(u->id() * 10 + i + 1));
            UnitItem* uiItem = findUnitItem(u->id());
            if (uiItem) {
                item->setPos(uiItem->pos() + QPointF(-40, -10 + i * 20));
                m_scene->addItem(item);
                m_equipUIItems.push_back(item);
                
                connect(item, &EquipItem::dragStarted, this, &Game::handleEquipDragStarted);
                connect(item, &EquipItem::dragMoved, this, &Game::handleEquipDragMoved);
                connect(item, &EquipItem::dragDropped, this, &Game::handleEquipDropCommand);
            }
        }

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
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>

bool Game::saveToFile(const QString& path) {
    QJsonObject root;
    QJsonObject pState;
    pState["hp"] = m_playerState.hp;
    pState["gold"] = m_playerState.gold;
    pState["level"] = m_playerState.level;
    pState["boardCap"] = m_playerState.boardCap;
    pState["round"] = m_playerState.round;
    root["playerState"] = pState;
    root["phase"] = static_cast<int>(m_phase);

    QJsonArray unitsArr;
    for (Unit* u : m_units) {
        QJsonObject uObj;
        uObj["id"] = u->get_m_id();
        uObj["name"] = u->name();
        uObj["job"] = static_cast<int>(u->get_job());
        uObj["owner"] = static_cast<int>(u->get_owner());
        uObj["hp"] = u->get_hp();
        uObj["maxHp"] = u->get_maxHp();
        uObj["atk"] = u->get_atk();
        uObj["range"] = u->get_range();
        uObj["mana"] = u->get_mana();
        uObj["maxMana"] = u->get_maxMana();
        uObj["starLevel"] = u->get_starLevel();
        uObj["traitLevel"] = u->get_traitLevel();
        uObj["baseMaxHp"] = u->get_baseMaxHp();
        uObj["baseAtk"] = u->get_baseAtk();
        uObj["posX"] = u->position().x();
        uObj["posY"] = u->position().y();

        QJsonArray eqArr;
        for (auto* eq : u->get_equipments()) {
            eqArr.append(static_cast<int>(eq->getType()));
        }
        uObj["equipments"] = eqArr;

        unitsArr.append(uObj);
    }
    root["units"] = unitsArr;

    QJsonArray shopArr;
    for (int i=0; i<Shop::SHOP_SIZE; ++i) {
        QJsonObject slot;
        slot["empty"] = m_shop.isSlotEmpty(i);
        if (!m_shop.isSlotEmpty(i)) {
            slot["job"] = static_cast<int>(m_shop.getSlot(i));
        }
        shopArr.append(slot);
    }
    root["shop"] = shopArr;

    QJsonArray equipBarArr;
    for (auto* eq : m_equipBar) {
        equipBarArr.append(static_cast<int>(eq->getType()));
    }
    root["equipBar"] = equipBarArr;

    QJsonDocument doc(root);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(doc.toJson());
    return true;
}

bool Game::loadFromFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull()) return false;
    
    QJsonObject root = doc.object();
    
    m_board.clear();
    m_bench.clear();
    qDeleteAll(m_units);
    m_units.clear();
    for (auto* eq : m_equipBar) delete eq;
    m_equipBar.clear();
    m_preCombatPositions.clear();
    m_dragActive = false;
    m_activeUnitId = -1;
    m_sourceGrid = QPoint(-1, -1);
    if (m_combatTimer->isActive()) m_combatTimer->stop();

    QJsonObject pState = root["playerState"].toObject();
    m_playerState.hp = pState.contains("hp") ? pState["hp"].toInt() : 10;
    m_playerState.gold = pState.contains("gold") ? pState["gold"].toInt() : 0;
    m_playerState.level = pState.contains("level") ? pState["level"].toInt() : 1;
    m_playerState.boardCap = pState.contains("boardCap") ? pState["boardCap"].toInt() : PlayerState().boardCap;
    m_playerState.round = pState.contains("round") ? pState["round"].toInt() : 1;
    m_phase = static_cast<Phase>(root["phase"].toInt());

    QJsonArray shopArr = root["shop"].toArray();
    for (int i=0; i<Shop::SHOP_SIZE; ++i) {
        QJsonObject slot = shopArr[i].toObject();
        m_shop.clearSlot(i);
        if (!slot["empty"].toBool()) {
            m_shop.setSlot(i, static_cast<JobType>(slot["job"].toInt()));
        }
    }

    QJsonArray equipBarArr = root["equipBar"].toArray();
    for (int i = 0; i < equipBarArr.size(); ++i) {
        EquipmentType type = static_cast<EquipmentType>(equipBarArr[i].toInt());
        Equipment* eq = createEquipmentFromType(type);
        if (eq) m_equipBar.push_back(eq);
    }

    QJsonArray unitsArr = root["units"].toArray();
    int maxId = -1;
    for (int i=0; i<unitsArr.size(); ++i) {
        QJsonObject uObj = unitsArr[i].toObject();
        JobType job = static_cast<JobType>(uObj["job"].toInt());
        Owner owner = static_cast<Owner>(uObj["owner"].toInt());
        Unit* u = nullptr;
        if (job == JobType::Warrior) u = new Warrior(uObj["name"].toString(), owner);
        else if (job == JobType::Mage) u = new Mage(uObj["name"].toString(), owner);
        else u = new Archer(uObj["name"].toString(), owner);
        
        int loadedId = uObj["id"].toInt();
        u->set_m_id(loadedId);
        if (loadedId > maxId) maxId = loadedId;

        u->set_starLevel(uObj["starLevel"].toInt());
        if(uObj.contains("traitLevel")) u->set_traitLevel(uObj["traitLevel"].toInt());
        if(uObj.contains("baseMaxHp")) u->set_baseMaxHp(uObj["baseMaxHp"].toInt());
        if(uObj.contains("baseAtk")) u->set_baseAtk(uObj["baseAtk"].toInt());
        u->setPosition(uObj["posX"].toInt(), uObj["posY"].toInt());

        // Load equipment — add_equipment triggers applyTraitEffects
        QJsonArray eqArr = uObj["equipments"].toArray();
        for (int j = 0; j < eqArr.size(); ++j) {
            EquipmentType type = static_cast<EquipmentType>(eqArr[j].toInt());
            Equipment* eq = createEquipmentFromType(type);
            if (eq) u->add_equipment(eq);
        }
        // Ensure trait effects are applied even if unit has no equipment
        if (eqArr.empty()) {
            u->applyTraitEffects();
        }

        // Override current HP/Mana with saved values (may differ from max)
        u->set_hp(uObj["hp"].toInt());
        u->set_mana(uObj["mana"].toInt());

        m_units.append(u);
        
        QPoint pos = u->position();
        if (pos.y() == Board::ROWS + 1) { 
            m_bench.addUnit(u);
            // 修复：必须让单位的坐标与备战区真正的物理下标对齐，避免旧坐标残留导致与后续购买的单位坐标重叠
            for (int k = 0; k < m_bench.getMaxBenchSize(); ++k) {
                if (m_bench.getUnits()[k] == u) {
                    u->setPosition(QPoint(k, pos.y()));
                    break;
                }
            }
        } else {
            m_board.addUnit(u, pos);
        }
    }
    Unit::setNextId(maxId + 1);

    buildScene();
    syncFromBoard();
    m_combatUnitIndex = -1;
    m_actingUnitId = -1;
    
    emit stateUpdated();
    emit phaseChanged(m_phase);
    return true;
}

void Game::updateTraits() {

    int warriorCountPlay = 0, mageCountPlay = 0, archerCountPlay = 0;
    int warriorCountEnem = 0, mageCountEnem = 0, archerCountEnem = 0;

    for (Unit* u : m_units) {
        if (!u->get_isAlive() || !m_board.hasUnitAt(u->position())) continue;
        int stars = u->get_starLevel();
        if (u->get_owner() == Owner::PlayerCtrl) {
            if (u->get_job() == JobType::Warrior) warriorCountPlay += stars;
            else if (u->get_job() == JobType::Mage) mageCountPlay += stars;
            else if (u->get_job() == JobType::Archer) archerCountPlay += stars;
        } else {
            if (u->get_job() == JobType::Warrior) warriorCountEnem += stars;
            else if (u->get_job() == JobType::Mage) mageCountEnem += stars;
            else if (u->get_job() == JobType::Archer) archerCountEnem += stars;
        }
    }

    for (Unit* u : m_units) {
        int count = 0;
        if (u->get_owner() == Owner::PlayerCtrl) {
            if (u->get_job() == JobType::Warrior) count = warriorCountPlay;
            else if (u->get_job() == JobType::Mage) count = mageCountPlay;
            else if (u->get_job() == JobType::Archer) count = archerCountPlay;
        } else {
            if (u->get_job() == JobType::Warrior) count = warriorCountEnem;
            else if (u->get_job() == JobType::Mage) count = mageCountEnem;
            else if (u->get_job() == JobType::Archer) count = archerCountEnem;
        }

        int level = 0;
        if (u->get_job() == JobType::Warrior) {
            if (count >= 4) level = 2;
            else if (count >= 2) level = 1;
        } else if (u->get_job() == JobType::Mage) {
            if (count >= 3) level = 1;
        } else if (u->get_job() == JobType::Archer) {
            if (count >= 3) level = 1;
        }
        
        u->set_traitLevel(level);
        u->applyTraitEffects();
    }
}

void Game::rollEquipmentDrop()
{
    // 每个回合结束时进行两次装备判定，每次判定有50%概率会掉落一件随机装备到装备栏里
    for (int i = 0; i < 2; ++i) {
        if (m_equipBar.size() < 5) {
            int randVal = std::rand() % 100;
            if (randVal < 50) {
                EquipmentType type = static_cast<EquipmentType>(std::rand() % 4);
                Equipment* eq = nullptr;
                switch (type) {
                    case EquipmentType::IronSword: eq = new IronSword(); break;
                    case EquipmentType::Chainmail: eq = new Chainmail(); break;
                    case EquipmentType::HasteGloves: eq = new HasteGloves(); break;
                    case EquipmentType::Sapphire: eq = new Sapphire(); break;
                }
                if (eq) {
                    m_equipBar.push_back(eq);
                }
            }
        }
    }
}

void Game::handleEquipDragStarted(Equipment* eq, int sourceSlot, const QPointF& scenePos)
{
    if (m_phase != Phase::Prep) return;
    Q_UNUSED(eq);
    Q_UNUSED(sourceSlot);
    Q_UNUSED(scenePos);
    for (UnitItem* item : m_unitItems) {
        item->setHighlighted(false);
    }
    for (EquipSlotItem* slot : m_equipSlotItems) {
        slot->setDropActive(false);
    }
}

void Game::handleEquipDragMoved(Equipment* eq, int sourceSlot, const QPointF& scenePos)
{
    if (m_phase != Phase::Prep) return;
    Q_UNUSED(eq);
    Q_UNUSED(sourceSlot);
    for (UnitItem* item : m_unitItems) {
        item->setHighlighted(false);
    }
    for (EquipSlotItem* slot : m_equipSlotItems) {
        slot->setDropActive(false);
    }

    for (UnitItem* item : m_unitItems) {
        if (item->isVisible() && item->unit() &&
            item->unit()->get_owner() == Owner::PlayerCtrl &&
            item->sceneBoundingRect().contains(scenePos)) {
            item->setHighlighted(true);
            return;
        }
    }

    for (EquipSlotItem* slot : m_equipSlotItems) {
        if (slot->sceneBoundingRect().contains(scenePos)) {
            slot->setDropActive(true);
            return;
        }
    }
}

void Game::handleEquipDropCommand(Equipment* eq, int sourceSlot, const QPointF& scenePos)
{
    if (m_phase != Phase::Prep) {
        syncFromBoard();
        return;
    }
    Q_UNUSED(sourceSlot);
    // Clear drag highlights
    for (UnitItem* item : m_unitItems) {
        item->setHighlighted(false);
    }
    for (EquipSlotItem* slot : m_equipSlotItems) {
        slot->setDropActive(false);
    }

    // Find if dropped on a unit
    UnitItem* targetUnitItem = nullptr;
    for (UnitItem* item : m_unitItems) {
        if (item->isVisible() && item->sceneBoundingRect().contains(scenePos)) {
            targetUnitItem = item;
            break;
        }
    }

    // Find if dropped on an equip bar slot
    int targetBarSlot = -1;
    for (EquipSlotItem* slot : m_equipSlotItems) {
        if (slot->sceneBoundingRect().contains(scenePos)) {
            targetBarSlot = slot->slotIndex();
            break;
        }
    }

    Unit* sourceUnit = nullptr;
    for (Unit* u : m_units) {
        for (auto* unitEq : u->get_equipments()) {
            if (unitEq == eq) {
                sourceUnit = u;
                break;
            }
        }
        if (sourceUnit) break;
    }
    bool fromBar = false;
    int barIndex = -1;
    if (!sourceUnit) {
        auto it = std::find(m_equipBar.begin(), m_equipBar.end(), eq);
        if (it != m_equipBar.end()) {
            fromBar = true;
            barIndex = static_cast<int>(std::distance(m_equipBar.begin(), it));
        }
    }

    // Case 1: Drop on a player-controlled unit
    if (targetUnitItem && targetUnitItem->unit() && targetUnitItem->unit()->get_owner() == Owner::PlayerCtrl) {
        Unit* targetUnit = targetUnitItem->unit();
        if (sourceUnit && sourceUnit != targetUnit) {
            sourceUnit->remove_equipment(eq);
            if (!targetUnit->add_equipment(eq)) {
                auto targetEqs = targetUnit->get_equipments();
                if (!targetEqs.empty()) {
                    Equipment* swapEq = targetEqs[0];
                    targetUnit->remove_equipment(swapEq);
                    targetUnit->add_equipment(eq);
                    sourceUnit->add_equipment(swapEq);
                } else {
                    sourceUnit->add_equipment(eq);
                }
            }
        } else if (fromBar) {
            if (targetUnit->add_equipment(eq)) {
                m_equipBar.erase(std::find(m_equipBar.begin(), m_equipBar.end(), eq));
            } else {
                // 目标单位已满 — 交换：新装备穿上，旧装备退回装备栏
                auto targetEqs = targetUnit->get_equipments();
                if (!targetEqs.empty()) {
                    Equipment* swapEq = targetEqs[0];
                    targetUnit->remove_equipment(swapEq);
                    targetUnit->add_equipment(eq);
                    auto it = std::find(m_equipBar.begin(), m_equipBar.end(), eq);
                    if (it != m_equipBar.end()) {
                        *it = swapEq;
                    }
                }
            }
        }
        syncFromBoard();
        emit stateUpdated();
        return;
    }

    // Case 2: Drop from unit back to equip bar
    if (sourceUnit && targetBarSlot >= 0) {
        sourceUnit->remove_equipment(eq);
        if (static_cast<int>(m_equipBar.size()) < 5) {
            m_equipBar.push_back(eq);
        } else {
            // Bar full, try to swap with the item at target slot
            if (targetBarSlot < static_cast<int>(m_equipBar.size())) {
                Equipment* swapEq = m_equipBar[targetBarSlot];
                m_equipBar.erase(m_equipBar.begin() + targetBarSlot);
                m_equipBar.insert(m_equipBar.begin() + targetBarSlot, eq);
                sourceUnit->add_equipment(swapEq);
            } else {
                sourceUnit->add_equipment(eq); // revert
            }
        }
        syncFromBoard();
        emit stateUpdated();
        return;
    }

    // Case 3: Reorder within equip bar
    if (fromBar && targetBarSlot >= 0 && barIndex != targetBarSlot) {
        m_equipBar.erase(m_equipBar.begin() + barIndex);
        if (targetBarSlot > barIndex) targetBarSlot--;
        if (targetBarSlot < static_cast<int>(m_equipBar.size())) {
            m_equipBar.insert(m_equipBar.begin() + targetBarSlot, eq);
        } else {
            m_equipBar.push_back(eq);
        }
        syncFromBoard();
        emit stateUpdated();
        return;
    }

    // Case 4: Invalid drop — sync back to original position
    syncFromBoard();
    emit stateUpdated();
}