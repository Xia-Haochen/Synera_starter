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
    m_combatTimer->setInterval(500); // 每个单位行动间隔 500ms
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
    // 清除上一个行动单位的高亮显示
    if (m_actingUnitId != -1) {
        UnitItem* prev = findUnitItem(m_actingUnitId);
        if (prev) prev->setHighlighted(false);
        m_actingUnitId = -1;
    }

    // ========== 决定本轮哪个单位行动 ==========
    // 战斗采用"轮流制"：每 tick 让一个单位行动一次
    // m_combatUnitIndex 循环递增遍历所有单位
    // 如果上一 tick 因为急速手套触发了额外行动（isBonusAction），
    // 则本轮继续让同一个单位再行动一次，不切换到下一个单位
    int totalUnits = m_units.size();

    bool isBonusAction = m_hasteSecondAction;
    if (m_hasteSecondAction) {
        // 本次是额外行动，用完就关闭标记，恢复正常间隔
        m_hasteSecondAction = false;
        m_combatTimer->setInterval(500);
    } else {
        // 找下一个存活并且在棋盘上的单位
        for (int i = 0; i < totalUnits; ++i) {
            m_combatUnitIndex = (m_combatUnitIndex + 1) % totalUnits;
            Unit* u = m_units[m_combatUnitIndex];
            if (u && u->get_isAlive() && m_board.hasUnitAt(u->position())) {
                break;
            }
        }
    }

    // 让选中的单位执行一次 action（移动 / 攻击 / 施法）
    if (m_combatUnitIndex >= 0 && m_combatUnitIndex < totalUnits) {
        Unit* u = m_units[m_combatUnitIndex];
        if (u && u->get_isAlive() && m_board.hasUnitAt(u->position())) {
            u->action(*this);

            // 如果这个单位有急速手套，且当前不是额外行动，就触发第二次行动
            if (!isBonusAction && u->has_haste_gloves()) {
                m_hasteSecondAction = true;
                m_combatTimer->setInterval(250); // 额外行动的间隔缩短一半
            }

            // 高亮当前行动的单位
            UnitItem* item = findUnitItem(u->id());
            if (item) item->setHighlighted(true);
            m_actingUnitId = u->id();
        }
    }

    // 清理战斗中的死亡单位（从棋盘移除，不占用格子）
    cleanupDeadUnits();
    syncFromBoard();

    // ========== 检测战斗是否结束 ==========
    // 某一方全部死亡则战斗结束，进入 resolve 阶段
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
        advancePhase(); // 进入 Resolve 阶段
    }
}

// ========== 阶段轮转核心逻辑 ==========
// 整个游戏的回合流程是：Prep（准备）→ Combat（战斗）→ Resolve（结算）→ Prep（下一轮准备）
// 每次调用 advancePhase() 就前进一个阶段
void Game::advancePhase()
{
    if (m_phase == Phase::Prep) {
        // 【准备 → 战斗】
        // 玩家点击"Start Battle"后触发，停止布阵，开始自动战斗
        m_phase = Phase::Combat;
        m_combatUnitIndex = -1;
        m_actingUnitId = -1;
        // 记录所有我方单位的战斗前位置，结算后要复位到这里
        m_preCombatPositions.clear();
        for (Unit* u : m_units) {
            if (u->get_owner() == Owner::PlayerCtrl && m_board.hasUnitAt(u->position())) {
                m_preCombatPositions[u] = u->position();
            }
        }
        emit phaseChanged(m_phase);
        m_combatTimer->start(); // 启动战斗时钟，开始 combatTick 循环

    } else if (m_phase == Phase::Combat) {
        // 【战斗 → 结算】
        // 当 combatTick 检测到一方全部阵亡时，通过 advancePhase() 进入这里
        m_phase = Phase::Resolve;
        resolveCombat(); // 计算胜负、发金币、扣血
        emit phaseChanged(m_phase);
        emit stateUpdated();
        // 结算画面停留 2 秒后自动进入下一轮准备
        QTimer::singleShot(2000, this, [this](){ advancePhase(); });

    } else if (m_phase == Phase::Resolve) {
        // 【结算 → 下一轮准备】
        m_phase = Phase::Prep;
        m_playerState.round++;       // 轮数 +1
        resetUnitsToPrep();          // 我方单位复位到战斗前位置，恢复满血
        spawnEnemyWave();            // 根据新的 round 生成敌人
        m_shop.rollShop();           // 自动刷新商店
        rollEquipmentDrop();         // 两次 50% 概率的装备掉落判定
        syncFromBoard();             // 场景同步
        emit phaseChanged(m_phase);
        emit stateUpdated();
    }
}

// 战斗结算：统计双方存活数量，根据胜负发放金币和扣血
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
        // 胜利：所有敌人被消灭，奖励 15 金币
        m_playerState.gold += 15;
        // 第 8 轮胜利时触发通关
        if (m_playerState.round >= 8) {
            emit gameEnded(true);
        }
    } else {
        // 失败（或平局）：场上还有剩余敌人，每个存活敌人扣 1 滴血，奖励 10 金币
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
        m_dragActive = false; // 终止本次拖拽
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

// 生成敌方单位：第 1 轮 2 个，之后每轮 +2，最多 8 个
void Game::spawnEnemyWave()
{
    int enemyCount = std::min(8, m_playerState.round * 2);

    for (int i = 0; i < enemyCount; ++i) {
        Unit* enemy = nullptr;
        int type = rand() % 3;
        if (type == 0) enemy = new Warrior("Enemy Warrior", Owner::EnemyCtrl);
        else if (type == 1) enemy = new Archer("Enemy Archer", Owner::EnemyCtrl);
        else enemy = new Mage("Enemy Mage", Owner::EnemyCtrl);

        m_units.append(enemy);

        // 创建图元并关联拖拽信号（敌人虽然不可拖动，但为了统一处理也创建了图元）
        UnitItem* unitItem = new UnitItem(enemy);
        unitItem->setZValue(kZUnit);
        m_scene->addItem(unitItem);
        m_unitItems.push_back(unitItem);
        m_unitItemById[enemy->id()] = unitItem;

        connect(unitItem, &UnitItem::dragStarted, this, &Game::handleDragStarted);
        connect(unitItem, &UnitItem::dragMoved, this, &Game::handleDragMoved);
        connect(unitItem, &UnitItem::dragDropped, this, &Game::handleDropCommand);

        // 在敌方半场（行 0~3）找一个空位随机摆放
        int r, c;
        do {
            r = rand() % (Board::ROWS / 2);
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

    // ========== 人口上限检查 ==========
    // 只有从备战区往场上（玩家半场）拖放空位时才触发人口检查
    // 如果目标是场上已有单位的格子（互换），则不需要检查人口
    // 因为互换不会增加场上单位数量
    bool sourceIsBench = isBenchPos(source);
    bool targetIsBoard = m_board.isPlayerHalf(target);
    if (sourceIsBench && targetIsBoard) {
        Unit* targetUnit = getUnitAtPos(target);
        if (!targetUnit) {
            // 统计当前已经在场上的我方单位数量
            int boardCount = 0;
            for (Unit* u : m_units) {
                if (u->get_owner() == Owner::PlayerCtrl && m_board.isPlayerHalf(u->position())) {
                    boardCount++;
                }
            }
            // 如果场上单位数 ≥ 人口上限，禁止上场
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

    // ========== 执行落子 ==========
    // 根据 source 和 target 的归属判断四种情况：
    // 1. board→board：棋盘内部移动（可能互换位置）
    // 2. bench→bench：备战区内部换位
    // 3. bench→board：备战区上阵到棋盘（受人口上限限制）
    // 4. board→bench：棋盘下阵到备战区
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

    // 刷新所有单位的 position 字段以保持同步
    // 因为互换位置涉及两个单位，逐个刷新比追踪交换更简单可靠
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

// 从商店购买单位：消耗 10 金币，创建对应职业的单位并放入备战区
void Game::buyFromShopSlot(int slotIndex)
{
    // ========== 前置条件检查 ==========
    if (m_phase != Phase::Prep) return;               // 只能在准备阶段购买
    if (slotIndex < 0 || slotIndex >= Shop::SHOP_SIZE) return;
    if (m_shop.isSlotEmpty(slotIndex)) return;         // 已售罄
    if (m_playerState.gold < Shop::UNIT_COST) return;  // 金币不够
    if (m_bench.getUnitCount() >= m_bench.getMaxBenchSize()) return; // 备战区满了

    // 扣钱
    m_playerState.gold -= Shop::UNIT_COST;

    // 根据商店显示的职业创建对应的单位
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

    // 加入全局单位列表
    m_units.append(newUnit);

    // 为这个新单位创建场景图元并连接拖拽信号
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

    // 放入备战区（找第一个空位）
    m_bench.addUnit(newUnit);

    // 设置单位在备战区的格子位置，后续 syncFromBoard 依赖这个来判断显示位置
    for (int i = 0; i < m_bench.getMaxBenchSize(); ++i) {
        if (m_bench.getUnits().value(i, nullptr) == newUnit) {
            newUnit->setPosition(QPoint(i, Board::ROWS + 1));
            break;
        }
    }

    // 商店槽位置为已售
    m_shop.clearSlot(slotIndex);

    // 检查是否可以触发 3 合 1 升星
    checkAndMerge(newUnit);

    syncFromBoard();
    emit stateUpdated();
}

// 购买人口上限：花费 3 金币，使人口上限 +1
// 这个操作没有次数限制，不会卖空
void Game::buyBoardCap()
{
    if (m_phase != Phase::Prep) return;         // 只能在准备阶段购买
    static const int BOARD_CAP_COST = 3;         // 人口上限价格
    if (m_playerState.gold < BOARD_CAP_COST) return; // 钱不够

    m_playerState.gold -= BOARD_CAP_COST;
    m_playerState.boardCap++;                    // 人口上限永久 +1
    emit stateUpdated();
}

// ========== 3 合 1 升星系统 ==========
// 自走棋核心机制：每获得一个新单位时，检查场上是否有 3 个同职业、同星级的我方单位
// 如果有，则移除其中 2 个，保留的 1 个升 1 星，基础属性翻倍
// 升星后递归检查是否可以继续合成（例如 3 个 2 星合成 1 个 3 星）
void Game::checkAndMerge(Unit* newUnit)
{
    if (!newUnit) return;
    int targetStar = newUnit->get_starLevel();
    if (targetStar >= 3) return; // 最高 3 星，不能再升

    // 查找所有跟我方、同职业、同星级的单位（包含 newUnit 自身）
    QList<Unit*> identicalUnits;
    for (Unit* u : m_units) {
        if (u->get_owner() == Owner::PlayerCtrl &&
            u->get_job() == newUnit->get_job() &&
            u->get_starLevel() == targetStar) {
            identicalUnits.append(u);
        }
    }

    // 如果凑够了 3 个，触发升星
    if (identicalUnits.size() >= 3) {
        // 策略：保留刚买/刚合成的这个单位（keepUnit），移除另外 2 个
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
            // ===== 移除 2 个被融合的单位 =====
            for (Unit* u : removeUnits) {
                // 从棋盘或备战区移除占位
                QPoint p = u->position();
                if (isBenchPos(p)) {
                    m_bench.removeUnit(u);
                } else {
                    m_board.removeUnit(u);
                }
                m_units.removeOne(u);

                // 移除场景图元
                if (m_unitItemById.count(u->id())) {
                    UnitItem* item = m_unitItemById[u->id()];
                    m_scene->removeItem(item);
                    m_unitItemById.erase(u->id());
                    m_unitItems.erase(std::remove(m_unitItems.begin(), m_unitItems.end(), item), m_unitItems.end());
                    delete item;
                }

                // 身上的装备退还到装备栏，如果装备栏满了就销毁
                std::vector<Equipment*> unitEqs = u->get_equipments();
                for (auto* eq : unitEqs) {
                    u->remove_equipment(eq);
                    if (m_equipBar.size() < 5) {
                        m_equipBar.push_back(eq);
                    } else {
                        delete eq;
                    }
                }
                delete u; // 销毁被融合的单位
            }

            // ===== 升星保留的单位 =====
            keepUnit->set_starLevel(targetStar + 1);    // 星级 +1
            keepUnit->set_baseMaxHp(keepUnit->get_baseMaxHp() * 2); // 基础血量翻倍
            keepUnit->set_baseAtk(keepUnit->get_baseAtk() * 2);     // 基础攻击翻倍
            keepUnit->applyTraitEffects();  // 重新计算羁绊加成和装备加成
            keepUnit->set_hp(keepUnit->get_maxHp()); // 升星后回满血

            // 递归检查：如果现在是 2 星，看看能不能继续合成 3 星
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

// 出售单位：从棋盘/备战区移除，退还装备，获得 5 金币
void Game::sellUnit(int unitId, const QPoint& sourceGrid)
{
    Unit* unit = findUnitById(unitId);
    if (!unit || unit->get_owner() != Owner::PlayerCtrl) return;

    // 从棋盘或备战区移除占位
    if (m_board.isValidPosition(sourceGrid)) {
        if (m_board.getUnitAt(sourceGrid) != unit) return;
        m_board.removeUnit(unit);
    } else if (isBenchPos(sourceGrid)) {
        if (m_bench.getUnits().value(sourceGrid.x(), nullptr) != unit) return;
        m_bench.removeUnit(unit);
    } else {
        return;
    }

    // 移除对应的场景图元
    UnitItem* item = findUnitItem(unitId);
    if (item) {
        m_scene->removeItem(item);
        m_unitItems.erase(std::remove(m_unitItems.begin(), m_unitItems.end(), item), m_unitItems.end());
        m_unitItemById.erase(unitId);
        delete item;
    }

    // 单位身上的装备退还到装备栏，装备栏满则销毁
    std::vector<Equipment*> unitEqs = unit->get_equipments();
    for (auto* eq : unitEqs) {
        unit->remove_equipment(eq);
        if (m_equipBar.size() < 5) {
            m_equipBar.push_back(eq);
        } else {
            delete eq;
        }
    }

    // 删除单位对象
    m_units.removeOne(unit);
    delete unit;

    // 获得出售金币（固定 5 金）
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

    // 重新创建装备栏图元
    // 使用 deleteLater 而非立即 delete，避免在 EquipItem 信号处理中调用
    // syncFromBoard 时出现 use-after-free
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

// ========== 六边形网格坐标系统 ==========
// 使用 even-r 偏移坐标系（偶数行右移半格），这是六边形棋盘常用的对齐方式
// gridToWorld：将逻辑坐标 (row, col) 转换为场景像素坐标
// worldToGrid：将场景像素坐标反向查找最近的逻辑坐标
// cellHexPolygon：根据逻辑坐标生成正六边形的 6 个顶点

QPointF Game::gridToWorld(int row, int col) const
{
    // 偶数行水平偏移 half-cell，形成错列六边形布局
    const qreal colSpacing = m_radius * qSqrt(3.0); // 六边形水平间距 = 半径 * √3
    const qreal xOffset = (row % 2 == 0) ? colSpacing * 0.5 : 0.0;
    const qreal x = xOffset + col * colSpacing;
    const qreal y = row * m_rowSpacing; // 垂直间距固定
    return QPointF(x, y);
}

QPoint Game::worldToGrid(const QPointF& world) const
{
    // 遍历所有格子（包括备战区 m_rows + 1），找到距离最近的格子中心
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
    // 从中心点 + 半径，以 60° 为步长计算六边形 6 个顶点
    // 起始角度 -90°（朝上），顺时针旋转
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

// ========== 存档：将游戏状态写入 JSON 文件 ==========
// 保存内容包括：
// - playerState：金币、血量、等级、人口上限、轮数
// - units：所有单位的完整属性（职业、星级、装备、坐标等）
// - shop：商店 5 个槽位的职业
// - equipBar：装备栏中的装备
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

// ========== 读档：从 JSON 文件恢复游戏状态 ==========
// 流程：清空当前所有状态 → 读取 playerState → 恢复商店 → 恢复装备栏
// → 重建所有单位并放回棋盘/备战区 → 重建场景图元
bool Game::loadFromFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull()) return false;
    
    QJsonObject root = doc.object();

    // ===== 清空当前所有状态 =====
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

    // ===== 恢复玩家状态（血量、金币、人口上限等） =====
    // 如果存档中缺少某个字段则使用默认值
    QJsonObject pState = root["playerState"].toObject();
    m_playerState.hp = pState.contains("hp") ? pState["hp"].toInt() : 10;
    m_playerState.gold = pState.contains("gold") ? pState["gold"].toInt() : 0;
    m_playerState.level = pState.contains("level") ? pState["level"].toInt() : 1;
    m_playerState.boardCap = pState.contains("boardCap") ? pState["boardCap"].toInt() : PlayerState().boardCap;
    m_playerState.round = pState.contains("round") ? pState["round"].toInt() : 1;
    m_phase = static_cast<Phase>(root["phase"].toInt());

    // ===== 恢复商店 =====
    QJsonArray shopArr = root["shop"].toArray();
    for (int i=0; i<Shop::SHOP_SIZE; ++i) {
        QJsonObject slot = shopArr[i].toObject();
        m_shop.clearSlot(i);
        if (!slot["empty"].toBool()) {
            m_shop.setSlot(i, static_cast<JobType>(slot["job"].toInt()));
        }
    }

    // ===== 恢复装备栏 =====
    QJsonArray equipBarArr = root["equipBar"].toArray();
    for (int i = 0; i < equipBarArr.size(); ++i) {
        EquipmentType type = static_cast<EquipmentType>(equipBarArr[i].toInt());
        Equipment* eq = createEquipmentFromType(type);
        if (eq) m_equipBar.push_back(eq);
    }

    // ===== 重建所有单位 =====
    // 遍历存档中的单位数组，逐个恢复：
    // 1. 根据职业创建对应类型的单位对象
    // 2. 恢复存档时保存的 ID（用于保留拖拽信号绑定关系）
    // 3. 恢复星级、羁绊、基础属性
    // 4. 恢复装备（add_equipment 会自动触发属性重算）
    // 5. 根据坐标放回棋盘或备战区
    QJsonArray unitsArr = root["units"].toArray();
    int maxId = -1;
    for (int i=0; i<unitsArr.size(); ++i) {
        QJsonObject uObj = unitsArr[i].toObject();
        JobType job = static_cast<JobType>(uObj["job"].toInt());
        Owner owner = static_cast<Owner>(uObj["owner"].toInt());

        // 根据存档中的职业创建单位
        Unit* u = nullptr;
        if (job == JobType::Warrior) u = new Warrior(uObj["name"].toString(), owner);
        else if (job == JobType::Mage) u = new Mage(uObj["name"].toString(), owner);
        else u = new Archer(uObj["name"].toString(), owner);

        // 恢复 ID（必须在添加到 m_units 之前设置）
        int loadedId = uObj["id"].toInt();
        u->set_m_id(loadedId);
        if (loadedId > maxId) maxId = loadedId;

        // 恢复星级和属性
        u->set_starLevel(uObj["starLevel"].toInt());
        if(uObj.contains("traitLevel")) u->set_traitLevel(uObj["traitLevel"].toInt());
        if(uObj.contains("baseMaxHp")) u->set_baseMaxHp(uObj["baseMaxHp"].toInt());
        if(uObj.contains("baseAtk")) u->set_baseAtk(uObj["baseAtk"].toInt());
        u->setPosition(uObj["posX"].toInt(), uObj["posY"].toInt());

        // 恢复装备（add_equipment 内部会调用 applyTraitEffects 重新计算属性）
        QJsonArray eqArr = uObj["equipments"].toArray();
        for (int j = 0; j < eqArr.size(); ++j) {
            EquipmentType type = static_cast<EquipmentType>(eqArr[j].toInt());
            Equipment* eq = createEquipmentFromType(type);
            if (eq) u->add_equipment(eq);
        }
        // 如果没有装备，也要手动触发属性重算（确保羁绊效果生效）
        if (eqArr.empty()) {
            u->applyTraitEffects();
        }

        // 恢复当前血量和蓝量（可能与最大值不同）
        u->set_hp(uObj["hp"].toInt());
        u->set_mana(uObj["mana"].toInt());

        m_units.append(u);

        // 根据坐标决定放回棋盘还是备战区
        QPoint pos = u->position();
        if (pos.y() == Board::ROWS + 1) {
            m_bench.addUnit(u);
            // 将坐标纠正为备战区的实际下标，防止旧坐标导致后续购买的坐标冲突
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
    // 恢复全局单位 ID 计数器，确保新创建的 ID 不冲突
    Unit::setNextId(maxId + 1);

    buildScene();
    syncFromBoard();
    m_combatUnitIndex = -1;
    m_actingUnitId = -1;
    
    emit stateUpdated();
    emit phaseChanged(m_phase);
    return true;
}

// ========== 羁绊效果计算 ==========
// 统计场上双方各职业的单位星级总和，根据数量决定羁绊等级：
// - 战士：2 个→等级1(1.5倍血量)，4 个→等级2(2倍血量)
// - 法师：3 个→等级1(技能附带眩晕)
// - 弓手：3 个→等级1(1.5倍攻击)
// 敌我双方各自独立计算
void Game::updateTraits() {

    int warriorCountPlay = 0, mageCountPlay = 0, archerCountPlay = 0;
    int warriorCountEnem = 0, mageCountEnem = 0, archerCountEnem = 0;

    // 第一遍遍历：统计双方各职业的星级总和
    for (Unit* u : m_units) {
        if (!u->get_isAlive() || !m_board.hasUnitAt(u->position())) continue;
        int stars = u->get_starLevel(); // 星级越高贡献越多
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

    // 第二遍遍历：根据统计结果给每个单位设置羁绊等级
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

        // 根据累计的星级数量判断羁绊等级
        int level = 0;
        if (u->get_job() == JobType::Warrior) {
            if (count >= 4) level = 2;    // 4 星以上→2级羁绊
            else if (count >= 2) level = 1; // 2 星以上→1级羁绊
        } else if (u->get_job() == JobType::Mage) {
            if (count >= 3) level = 1;    // 3 星以上→1级羁绊
        } else if (u->get_job() == JobType::Archer) {
            if (count >= 3) level = 1;    // 3 星以上→1级羁绊
        }

        u->set_traitLevel(level);
        u->applyTraitEffects(); // 将羁绊等级生效到单位属性上
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
    // 清除拖拽高亮
    for (UnitItem* item : m_unitItems) {
        item->setHighlighted(false);
    }
    for (EquipSlotItem* slot : m_equipSlotItems) {
        slot->setDropActive(false);
    }

    // 判断拖拽落点是否在一个单位上
    UnitItem* targetUnitItem = nullptr;
    for (UnitItem* item : m_unitItems) {
        if (item->isVisible() && item->sceneBoundingRect().contains(scenePos)) {
            targetUnitItem = item;
            break;
        }
    }

    // 判断拖拽落点是否在装备栏的某个槽位上
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

    // 情况 1：装备拖拽到我方单位身上 → 穿戴装备（如果目标已满则交换）
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

    // 情况 2：从单位身上拖回装备栏 → 卸下装备
    if (sourceUnit && targetBarSlot >= 0) {
        sourceUnit->remove_equipment(eq);
        if (static_cast<int>(m_equipBar.size()) < 5) {
            m_equipBar.push_back(eq);
        } else {
            // 装备栏满了，尝试与目标槽位交换
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

    // 情况 3：装备栏内部拖动 → 重新排序
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

    // 情况 4：无效拖拽（没放到任何有效位置）→ 回到原位
    syncFromBoard();
    emit stateUpdated();
}