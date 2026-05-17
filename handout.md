# Synera PA Handout（阶段0~阶段3）

> 基于以下材料整理：`PA说明文档_mupdf.md`、`PA阶段TODO与功能规划.md`、`README.md`、当前 `src/` 全部代码。  
> 本 handout 仅覆盖到阶段三（不包含阶段四扩展）。

---

## 0. 你现在的项目状态（快速定位）

当前仓库已经有：

1. 可运行的 Qt6 图形框架（`GameWindow + QGraphicsScene`）。
2. 8x8 棋盘占位（`Board`）和基础拖拽链路（`UnitItem -> Game -> Board -> sync`）。
3. `Unit` 还是最小演示实体，尚未完成 PA 要求的战斗属性、多态、状态机、羁绊、升星、装备、经济、存档。

结论：**GUI/拖拽脚手架可复用，核心逻辑需系统重构。**

---

## 1. TODO 编号规则与代码映射

为了让你“在 handout 和代码里秒对齐”，我把代码 TODO 标注成：

- `T0-x`：阶段0
- `T1-x`：阶段1
- `T2-x`：阶段2
- `T3-x`：阶段3

### 1.1 当前代码中的 TODO 编号位置

| 编号 | 当前代码位置（示例） | 说明 |
|---|---|---|
| T0-1 | `src/entity/unit.cpp`（构造函数初始化注释） | 统一核心属性初始化 |
| T0-2 | `src/entity/unit.h/.cpp` | Unit 多态体系改造 |
| T0-3 | `src/core/game.h` | 主循环/阶段驱动入口 |
| T1-1 | `src/core/game.h` | Bench 与 Board 双向规则 |
| T1-2 | `src/core/game.h` | 非法拖拽处理策略 |
| T1-3 | `src/core/game.h` | 血条/蓝条/属性展示同步 |
| T1-4 | `src/core/game.h` + `src/entity/unit.cpp` | Player 与敌人生成入口 |
| T2-1 | `src/core/game.h` | 准备-战斗-结算循环 |
| T2-2 | `src/entity/unit.h/.cpp` | FSM（Idle/Moving/...） |
| T2-3 | `src/core/board.h` + `src/entity/unit.h` | 索敌与平局规则 |
| T2-4 | `src/core/board.h` | 寻路与碰撞 |
| T2-5 | `src/entity/unit.h/.cpp` | 普攻/回蓝/技能/受击 |
| T2-6 | `src/core/game.h` | 胜负与关卡推进 |
| T3-1 | `src/core/game.h` | 金币商店系统 |
| T3-2 | `src/core/game.h` | 人口系统 |
| T3-3 | `src/entity/unit.h` | 羁绊标签与效果承载 |
| T3-4 | `src/core/board.h` + `src/entity/unit.h/.cpp` | 3合1升星 |
| T3-5 | `src/entity/unit.h/.cpp` | 装备系统 |
| T3-6 | `src/core/game.h` | 存档读档接口 |
| T3-7 | `src/core/game.h` | 阶段三 UI 总展示 |

---

## 2. 先定统一数据模型（强烈建议）

在动手每个 TODO 前，先把核心类型补齐，否则后面会反复返工。

```cpp
enum class Owner { PlayerCtrl, EnemyCtrl };
enum class Phase { Prep, Combat, Resolve };
enum class UnitState { Idle, Moving, Attacking, Casting, Dead };
enum class Trait { Warrior, Mage, Ranger, Guardian, Assassin, Priest };
enum class EquipType { Sword, ChainVest, Gloves, Crystal };
```

建议新增核心结构（可放 `src/core` / `src/entity`）：

```cpp
struct UnitStats {
    int hp = 300, maxHp = 300;
    int atk = 35, range = 1;
    int mana = 0, maxMana = 60;
    int attackIntervalFrames = 60;
    int moveIntervalFrames = 20;
};

struct PlayerState {
    int hp = 100;
    int gold = 10;
    int level = 1;
    int boardCap = 3;   // 人口上限
    int round = 1;
};
```

---

## 3. 分 TODO 详细实现指引（阶段0~3）

## T0-1 统一核心数据模型

**作用**：避免“字段散落 + 逻辑互相耦合”，后续战斗、商店、存档都围绕统一状态。  
**推荐数据结构**：

- `GameState`：全局状态聚合（玩家、阶段、轮次、随机种子）。
- `UnitStats`：单位数值集中。
- `UnitTemplate`：英雄模板表（名字、基础属性、羁绊、技能类型）。

**接口建议**：

```cpp
struct GameState { PlayerState player; Phase phase; int round; };
const UnitTemplate& getTemplateByName(const QString& name);
```

**落地方法**：

1. 先把 `Unit` 的裸字段收拢到 `UnitStats`。
2. `Game` 内新增 `GameState m_state`，不要再散用局部变量记阶段/轮次。
3. 敌人生成、商店刷新都用 `UnitTemplate`，避免硬编码数值。

---

## T0-2 重构 Unit 体系（多态）

**作用**：满足 OOP 评分项，并让技能实现真正可扩展。  
**推荐结构**：

- `class Unit`（抽象基类）
- `class WarriorUnit / MageUnit / RangerUnit ...`（3~5个派生）

**接口建议**：

```cpp
class Unit {
public:
    virtual ~Unit() = default;
    virtual void update(class CombatContext& ctx) = 0;
    virtual void castSkill(class CombatContext& ctx) = 0;
    virtual void attack(Unit* target, CombatContext& ctx);
    virtual void takeDamage(int amount, Unit* source, CombatContext& ctx);
};
```

**算法/实现建议**：

- 普攻逻辑放基类（通用）。
- 技能逻辑放派生类（差异点）。
- 战斗中只持有 `Unit*` 或 `std::unique_ptr<Unit>`，不写 `if(name=="法师")`。

---

## T0-3 事件与更新主循环

**作用**：把游戏从“拖拽 demo”变成“可推进回合的游戏”。  
**推荐结构**：Qt `QTimer` 以 60FPS 驱动。

```cpp
void Game::startLoop() {
    connect(&m_timer, &QTimer::timeout, this, &Game::onTick);
    m_timer.start(16); // ~60fps
}
```

**关键点**：

1. `Prep`：允许拖拽、购买、刷新、升人口。
2. `Combat`：锁定拖拽，只运行单位 `update()`。
3. `Resolve`：结算胜负、发金币、准备下一轮。

---

## T1-1 Bench 模块

**作用**：阶段一必做，商店购买后的单位容器。  
**推荐数据结构**：固定长度数组最稳。

```cpp
class Bench {
public:
    static constexpr int kSize = 8;
    std::array<Unit*, kSize> slots{};
    int firstEmpty() const;
    bool add(Unit* u);
    Unit* removeAt(int idx);
};
```

**落地方法**：

1. `Game` 新增 `Bench m_bench`。
2. 拖拽来源增加“Bench槽位”和“Board格子”两种。
3. 保证 `Board` 与 `Bench` 不会同时持有同一单位。

---

## T1-2 非法拖拽处理（交换或回弹）

**作用**：符合 PA 交互验收。  
**推荐策略**：优先“回弹”，实现简单且不易出错。

**接口建议**：

```cpp
enum class DropResult { Applied, Reverted, Swapped };
DropResult Game::tryApplyDrop(const DragContext& ctx, const DropTarget& target);
```

**落地方法**：

- 先做回弹：不合法则不改数据，`syncFromBoardAndBench()` 即回原位。
- 再加交换（可选）：目标占用时交换两单位位置。

---

## T1-3 单位属性展示（血条/蓝条/面板）

**作用**：阶段一 GUI Checklist。  
**推荐做法**：

1. 在 `UnitItem::paint()` 画两条细矩形（红血蓝蓝）。
2. 面板可先做“选中单位后右侧文本”。

```cpp
qreal hpRatio = qBound(0.0, unit->hp() * 1.0 / unit->maxHp(), 1.0);
qreal manaRatio = qBound(0.0, unit->mana() * 1.0 / unit->maxMana(), 1.0);
```

---

## T1-4 玩家实体与敌方轮次生成

**作用**：接入后续 PvE 战斗。  
**推荐结构**：

- `PlayerState`（hp/gold/level/cap）
- `EnemyWaveConfig`（每轮生成哪些敌人）

```cpp
struct EnemySpawn { QString unitName; QPoint pos; int star; };
std::vector<EnemySpawn> getWaveConfig(int round);
```

---

## T2-1 三阶段循环

**作用**：阶段二主骨架。  
**落地顺序**：

1. `enterPrep()`：倒计时 + 允许经营。
2. `enterCombat()`：spawn敌人 + 锁交互。
3. `enterResolve()`：判定 + 发奖励 + 清场。

---

## T2-2 单位状态机（FSM）

**作用**：减少战斗 bug（状态唯一、切换明确）。  
**状态转移建议**：

- `Idle -> Moving`：有目标但不在攻击范围
- `Idle -> Attacking`：有目标且在范围内
- `Attacking -> Casting`：Mana 满
- `* -> Dead`：HP<=0

```cpp
switch (m_state) {
case UnitState::Idle:      updateIdle(ctx); break;
case UnitState::Moving:    updateMoving(ctx); break;
case UnitState::Attacking: updateAttacking(ctx); break;
case UnitState::Casting:   updateCasting(ctx); break;
case UnitState::Dead:      break;
}
```

---

## T2-3 索敌与平局规则

**作用**：让战斗结果可复现、可解释。  
**排序键（按 PA）**：

1. 欧氏距离最小
2. 距离相同：优先生命值高
3. 再同：从左到右（x 小优先）
4. 再同：从下到上（y 大优先，按当前坐标系）

```cpp
auto better = [&](Unit* a, Unit* b){ /* 按以上多关键字比较 */ };
```

---

## T2-4 寻路与碰撞

**作用**：近战绕行、防穿模、防重叠。  
**推荐算法**：

- 基础版本：BFS（无权图，网格最短路）。
- 提升版：A*（启发函数用曼哈顿/欧氏）。

**建议实现**：

```cpp
std::vector<QPoint> Board::findPath(const QPoint& start, const QPoint& goal,
                                    const std::function<bool(QPoint)>& passable);
```

**关键细节**：

1. 目标格被占用时，改为“寻到目标周围可攻击邻格”。
2. 每 N 帧重算一次路径（re-path）避免目标移动导致路径失效。
3. 每帧移动前做“目标格仍空闲”检查，失败则等待或重算。

---

## T2-5 普攻/回蓝/技能释放

**作用**：阶段二核心闭环。  
**推荐机制**：

- 攻击冷却：`attackCdFrames` 计时至0才能普攻。
- 普攻后：目标扣血 + 自己 `mana += 10`。
- 满蓝：进入 `Casting`，施法后清零或减蓝。

```cpp
void Unit::attack(Unit* target, CombatContext& ctx) {
    if (!target || !target->isAlive()) return;
    target->takeDamage(stats().atk, this, ctx);
    m_stats.mana = std::min(m_stats.maxMana, m_stats.mana + 10);
}
```

技能多态建议（3~5 英雄）：

1. 战士：单体高伤 + 短暂眩晕（机制类）。
2. 法师：直线/范围 AOE（伤害类）。
3. 牧师：友军小范围回血（功能类）。

---

## T2-6 胜负与关卡推进

**作用**：从“战斗动画”变成“游戏流程”。  
**规则建议**：

- 敌方全灭：胜利，+高金币，概率掉装备。
- 我方全灭：失败，玩家扣血，+低金币。
- 玩家HP<=0：游戏失败结束。
- `round++` 后按关卡配置增强敌方。

---

## T3-1 金币与商店（5招募位）

**作用**：阶段三运营核心。  
**推荐结构**：

```cpp
struct ShopSlot { QString templateId; int cost; bool bought = false; };
std::array<ShopSlot, 5> m_shop;
```

**接口建议**：

```cpp
void rollShop(bool isFree);
bool buyFromShop(int idx);
bool refreshShop(); // 扣金币后 roll
```

**算法建议**：

- 维护英雄池权重表（按费用/星级概率）。
- `std::discrete_distribution` 做随机抽样。

---

## T3-2 人口系统

**作用**：形成“上阵取舍”策略。  
**核心约束**：`boardPlayerUnitCount <= boardCap`。

```cpp
bool Game::canDeployFromBench() const {
    return currentPlayerBoardCount() < m_state.player.boardCap;
}
```

升级建议：花费递增（如 4/8/12/16...），每次 `cap + 1`。

---

## T3-3 羁绊系统（4~6种）

**作用**：阵容构筑深度。  
**推荐结构**：

```cpp
struct SynergyRule {
    Trait trait;
    std::vector<int> thresholds; // 如 {2,4}
    std::function<void(BattleSide&)> apply; // 光环/机制
};
```

最低达标建议：

1. 光环类1：战士 2/4 -> 护甲/生命加成
2. 光环类2：守护 2/3 -> 全队减伤
3. 机制类：法师 3 -> 技能伤害倍率提升

---

## T3-4 升星系统（3合1）

**作用**：成长线 + 运营目标。  
**推荐数据结构**：

- 索引键：`(name, star, owner)`
- 容器：`std::unordered_map<Key, std::vector<Unit*>>`

**触发时机**：购买成功后、准备阶段阵容变更后都检查。  
**保位规则**：保留“最新获得第3个”的位置（PA 要求）。

```cpp
void Game::tryMerge3(Unit* newlyObtained) {
    auto group = collectSameNameSameStar(newlyObtained);
    if (group.size() < 3) return;
    // remove 2, upgrade 1
}
```

---

## T3-5 装备系统（至少4件）

**作用**：战斗前运营增强。  
**基础装备（建议按 PA）**：

1. 铁剑：ATK +15
2. 锁子甲：HP +150
3. 急速手套：攻速 +20%
4. 蓝水晶：MaxMana -30

**推荐结构**：

```cpp
struct Equipment { EquipType type; QString name; };
std::vector<Equipment> m_equipBag;
```

单位装备槽：

- 1星：最多1件
- 2星：可扩展到2件（可选）

**实现建议**：装备效果统一通过“属性重算”应用，不直接多处叠加字段。

---

## T3-6 存档读档（序列化/反序列化）

**作用**：阶段三硬验收点。  
**推荐格式**：JSON（Qt 原生好用）。

```cpp
QJsonObject Game::toJson() const;
bool Game::fromJson(const QJsonObject& obj);
bool Game::saveToFile(const QString& path);
bool Game::loadFromFile(const QString& path);
```

必须存的状态：

- 玩家状态（hp/gold/level/cap/round/phase）
- 棋盘单位（位置、星级、HP、Mana、owner、traits、装备）
- Bench 单位
- Shop 当前5格
- 装备栏

---

## T3-7 阶段三 UI 补完

**作用**：完整演示路径。  
**至少可见**：

1. 玩家 HP / 金币 / 人口（已上阵/上限）
2. 商店5格 + 刷新按钮
3. 羁绊激活列表（当前阈值）
4. 单位星级与装备图标
5. 当前轮次与阶段（Prep/Combat/Resolve）

建议拆小组件：`HudWidget / ShopWidget / SynergyPanel`，不要全部塞 `GameWindow`。

---

## 4. 推荐实现顺序（按最稳妥依赖）

1. `T0-1 -> T0-2 -> T1-1 -> T1-2 -> T1-4`
2. `T0-3 -> T2-1 -> T2-2 -> T2-3 -> T2-4 -> T2-5 -> T2-6`
3. `T3-1 -> T3-2 -> T3-4 -> T3-5 -> T3-3 -> T3-6 -> T3-7`

说明：

- 羁绊（T3-3）依赖单位标签、上阵统计，放在商店/人口/升星后更稳。
- 存档（T3-6）建议在主要系统跑通后做，不然 schema 会反复改。

---

## 5. 关键坑位清单（高频踩坑）

1. **单位所有权重复**：同一 `Unit*` 同时在 Board 和 Bench（必须禁止）。
2. **状态机穿透**：`Dead` 状态还在攻击/回蓝（需全局短路）。
3. **路径过期**：目标移动后继续沿旧路径走（加 re-path）。
4. **羁绊重复叠加**：每帧直接叠数值导致爆炸（必须“先重置再按激活重算”）。
5. **存档漏字段**：读档后 UI 状态和逻辑状态不同步（load 后统一 `syncAllUI()`）。

---

## 6. 提交前最小自测清单（阶段三）

1. 能完整跑一轮：准备 -> 战斗 -> 结算 -> 下一轮。
2. 商店购买到 Bench，Bench 可上阵且受人口限制。
3. 至少 3 个英雄技能多态可触发。
4. 至少 4 种装备可掉落并穿戴生效。
5. 3合1升星自动触发且保位正确。
6. 存档后退出重开，读档恢复一致。

---

## 7. 你可以直接照抄起步的接口草案（建议）

```cpp
// core/game.h
class Game : public QObject {
    Q_OBJECT
public:
    void onTick();                     // T0-3/T2-1
    void advancePhase();               // T2-1/T2-6
    bool buyFromShop(int slot);        // T3-1
    bool refreshShop();                // T3-1
    bool upgradePopulation();          // T3-2
    bool saveToFile(const QString&);   // T3-6
    bool loadFromFile(const QString&); // T3-6
};

// entity/unit.h
class Unit {
public:
    virtual ~Unit() = default;
    virtual void update(CombatContext&) = 0;  // T2-2
    virtual void castSkill(CombatContext&) = 0; // T2-5
    virtual void attack(Unit*, CombatContext&); // T2-5
};
```

这套接口与当前 starter 的 `Game/Board/Unit` 结构兼容，迁移成本最低。

