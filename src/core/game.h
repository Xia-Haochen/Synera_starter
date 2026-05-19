#ifndef CORE_GAME_H
#define CORE_GAME_H

#include <QObject>
#include <QList>
#include <QPoint>
#include <QPointF>
#include <QPolygonF>
#include <unordered_map>
#include <vector>
#include "board.h"
#include "Bench.h"
#include "shop.h"

class Unit;
class QGraphicsScene;
class GridItem;
class UnitItem;

enum class Phase { Prep, Combat, Resolve };

struct PlayerState {
    int hp = 10;
    int gold = 100;
    int level = 1;
    int boardCap = 7;   // 人口上限
    int round = 1;
};

// Game 负责连接棋盘数据(Board)与图形场景(QGraphicsScene)：
// 1) 初始化单位与场景
// 2) 处理拖拽输入并执行落子规则
// 3) 将逻辑状态同步到 UI 图元
class Game : public QObject
{
    Q_OBJECT

public:
    explicit Game(QObject* parent = nullptr);
    ~Game();

    // 创建初始单位、构建场景并完成首轮同步。
    void initialize();
    // 将棋盘重置到开局摆放状态。
    void reset();

    QGraphicsScene* scene() const { return m_scene; }

    // UnitItem 拖拽生命周期事件入口（由信号槽触发）。
    void handleDragStarted(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);
    void handleDragMoved(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);
    void handleDropCommand(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);

    // 增加回合主循环驱动（Prep -> Combat -> Resolve）、胜负结算与轮次推进入口
    void advancePhase();
    void resolveCombat();
    void resetUnitsToPrep();
    void cleanupDeadUnits(); // 清理场上阵亡单位

    Phase getPhase() const { return m_phase; }
    PlayerState& getPlayerState() { return m_playerState; }

    const QList<Unit*>& getUnitList() const { return m_units; }
    Board& get_board() { return m_board; }

    void spawnEnemyWave();

    // 商店与出售
    Shop& getShop() { return m_shop; }
    void buyFromShopSlot(int slotIndex);
    void rollShop();

    void checkAndMerge(Unit* newUnit);
    void updateTraits();

signals:
    void phaseChanged(Phase newPhase);
    void stateUpdated();
    void gameEnded(bool isVictory);

    // TODO[T3-1/T3-2]: 增加商店与经济入口（rollShop()/buyUnit()/refreshShop()/upgradePopulation()）。
public:
    bool saveToFile(const QString& path);
    bool loadFromFile(const QString& path);
signals:
    // TODO[T3-7]: 增加阶段三整合UI同步接口（syncHud()/syncShop()/syncSynergyPanel()）。

private:
    // 初始化与查询辅助。
    void createStarterUnitsIfNeeded();
    Unit* findUnitById(int unitId) const;
    GridItem* findGridItem(const QPoint& gridPos) const;
    UnitItem* findUnitItem(int unitId) const;
    void clearGridHighlights();

    // 落子合法性判断与执行。
    bool canApplyDrop(int unitId, const QPoint& source, const QPoint& target) const;
    void applyDrop(int unitId, const QPoint& target);
    // 出售单位
    void sellUnit(int unitId, const QPoint& sourceGrid);
    // finish: TODO[T1-2]: 非法拖拽处理策略（交换/回弹）统一收口函数，避免散落在UI事件中。
    // finish: TODO[T1-1]: 棋盘与备战区双向上阵/下阵规则（建议拆分 canMoveBenchToBoard/canMoveBoardToBench）。

    // 场景构建与逻辑-渲染同步。
    void buildScene();
    void syncFromBoard();
    // finish: TODO[T1-3]: 同步单位血条/蓝条/属性面板到图元。
    
    // 定时器触发的每一次战斗逻辑循环
    void combatTick();

    // 六边形网格坐标变换与几何生成。
    QPointF gridToWorld(int row, int col) const;
    QPoint worldToGrid(const QPointF& world) const;
    QPolygonF cellHexPolygon(int row, int col) const;

    bool isBenchPos(const QPoint& pos) const;

    Unit* getUnitAtPos(const QPoint& pos) const;

    Board m_board;
    Bench m_bench;
    Shop m_shop;
    QList<Unit*> m_units;

    QGraphicsScene* m_scene;
    std::vector<GridItem*> m_gridItems;
    std::vector<UnitItem*> m_unitItems;

    // 当前拖拽上下文。
    bool m_dragActive;
    int m_activeUnitId;
    QPoint m_sourceGrid;
    std::unordered_map<int, UnitItem*> m_unitItemById;

    // 棋盘几何参数（用于坐标换算与绘制）。
    int m_rows;
    int m_cols;
    qreal m_radius;
    qreal m_rowSpacing;

    Phase m_phase = Phase::Prep;
    PlayerState m_playerState;
    QHash<Unit*, QPoint> m_preCombatPositions;
    class QTimer* m_combatTimer;
    int m_combatUnitIndex = -1;   // 当前轮到哪个单位行动
    int m_actingUnitId = -1;      // 正在行动的单位 ID（用于高亮）

    // 出售区域
    GridItem* m_sellZoneItem = nullptr;
    QPointF m_sellZoneCenter;
};

#endif // CORE_GAME_H
