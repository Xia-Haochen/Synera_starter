#ifndef UNIT_H
#define UNIT_H

#include <QPoint>
#include <QString>
// TODO[T3-5]: 添加装备(Equipment)类前置声明或枚举，PA要求支持玩家给单位穿戴装备（如铁剑、锁子甲等）

// TODO[T2-2]: 定义战斗状态机枚举，例如 enum class State { Idle, Moving, Attacking, Casting, Dead };

class Game; // 前置声明，避免循环依赖

struct UnitStats {
    int hp = 300, maxHp = 300;
    int atk = 35, range = 1;
    int mana = 0, maxMana = 60;
    int attackIntervalFrames = 60;
    int moveIntervalFrames = 20;
};

enum class Owner { PlayerCtrl, EnemyCtrl };
enum class UnitState { Idle, Moving, Attacking, Casting, Dead };
enum class Trait { Warrior, Mage, Ranger, Guardian, Assassin, Priest };
enum class EquipType { Sword, ChainVest, Gloves, Crystal };

struct Role_Template
{
    int HP;
    int ATK;
    int Range;
    int Max_Mana;
    // TODO[T4-1]: 可以在这里添加更多基础属性，如攻击速度、移动速度等，以便在构造函数中统一初始化
};

enum class JobType // C++ 强类型枚举（枚举类）
{
    Warrior,  // 战士
    Mage,     // 法师
    Archer    // 射手
};

template <JobType T>
class RoleTemplate
{
public:
    Role_Template attr;

    // 构造函数：根据枚举初始化对应职业数值
    RoleTemplate()
    {
        if constexpr (T == JobType::Warrior)
        {
            attr = {1200, 250, 1, 80};
        }
        else if constexpr (T == JobType::Mage)
        {
            attr = {600, 300, 5, 40};
        }
        else if constexpr (T == JobType::Archer)
        {
            attr = {800, 200, 3, 50};
        }
    }
};

int distance(const QPoint& a, const QPoint& b);

class Unit
{
public:
    explicit Unit(const QString& name = QString("Unit"));
    explicit Unit(const QString& name, Role_Template t);
    Unit(const Unit& other);
    // finish TODO[T0-2]: 将析构函数改为虚析构函数 virtual ~Unit() = default; 以支持多态衍生类的安全释放
    virtual ~Unit() = default;

    int id() const { return m_id; }
    QString name() const { return m_name; }
    QPoint position() const { return m_position; }

    void setName(const QString& name) { m_name = name; }
    void setPosition(const QPoint& pos) { m_position = pos; }

    // getters/setters for HP/ATK/Range/Mana等属性
    int get_hp() const { return HP; }
    int get_maxHp() const { return Max_HP; }
    int get_atk() const { return ATK; }
    int get_range() const { return Range; }
    int get_mana() const { return Mana; }
    int get_maxMana() const { return Max_Mana; }
    bool get_isAlive() const { return is_alive; }
    bool get_isDizzy() const { return is_Dizzy; }
    bool get_hasTarget() const { return has_target; }
    Unit* get_Target() const { return target; }
    Owner get_owner() const { return owner; }
    int get_starLevel() const { return Star_Level; }
    int get_m_id() const { return m_id; }
    JobType get_job() const { return m_job; }

    void set_hp(int hp) { HP = hp; }
    void set_atk(int atk) { ATK = atk; }
    void set_range(int range) { Range = range; }
    void set_mana(int mana) { Mana = mana; }
    void set_maxMana(int maxMana) { Max_Mana = maxMana; }
    void set_isAlive(bool alive) { is_alive = alive; }
    void set_isDizzy(bool dizzy) { is_Dizzy = dizzy; }
    void set_hasTarget(bool hasTarget) { has_target = hasTarget; }
    void set_Target(Unit* targetUnit) { target = targetUnit; }
    void set_owner(Owner owner) { this->owner = owner; }
    void set_starLevel(int starLevel) { Star_Level = starLevel; }
    void setPosition(int x, int y) { m_position = QPoint(x, y); }
    void set_m_id(int id) { m_id = id; }

    void find_target(Game& game);
    // TODO[T2-2]: 实现每帧更新的虚拟函数，基于状态机的帧逻辑更新，如 virtual void action() = 0; 或提供默认实现
    virtual void action(Game& game);
    // finish: TODO[T2-5]: 实现虚函数 virtual void attack(Unit* target); 计算普攻伤害、回蓝逻辑（如每次普攻回10点蓝）
    virtual void attack(Unit* target);
    // TODO[T0-2/T2-5]: 实现纯虚函数 virtual void castSkill(Game& game) = 0; 让3-5个派生的英雄类各自实现不同的技能逻辑(眩晕/AOE/回血等)
    virtual void castSkill(Game& game) = 0;
    // finish: TODO[T2-5]: 实现虚拟函数 virtual void takeDamage(int amount); 计算受击扣血后判断是否进入Dead状态并触发死亡结算
    virtual void takeDamage(int amount);

protected:
    Owner owner; //单位归属
    JobType m_job; //单位职业类型
    
private:
    static int s_nextId;

    int m_id;
    QString m_name;
    QPoint m_position;

    int HP; //生命值
    int Max_HP; //最大生命值
    int ATK; //攻击力
    int Range; //攻击范围
    int Max_Mana; //最大法力值
    int Mana; //当前法力值
    bool is_alive; //单位是否存活
    bool is_Dizzy; //是否处于眩晕状态
    bool has_target; //是否有攻击目标
    Unit* target;

    // TODO[T2-5]: 增加战斗属性扩展：Attack_Speed(攻击速度，以帧为单位计量)、Move_Speed(移动速度)
    // TODO[T3-3]: 增加羁绊属性标签 traits (例如使用 std::vector<QString> 或自定义 Enum)，同种羁绊达成触发Buff
    // TODO[T3-4]: 增加星级(Star_Level)属性支持3合1进阶机制（1星合成保留原位变2星），以及记录星阶属性翻倍数据
    int Star_Level; //单位星级，1-3星，3星后不再进阶
    // TODO[T2-2/T2-3]: 增加一个成员维护当前状态机的状态 state，以及寻找的目标敌方单位指针 Unit* current_target;
    // TODO[T3-5]: 增加装备槽数据结构(如 std::vector<Equipment*> eq)，PA要求1星最多1件，2星可选扩展2件
};

class Warrior : public Unit
{
public:
    explicit Warrior(const QString& name, Owner Camp);
    void castSkill(Game& game);
};

class Mage : public Unit
{
public:
    explicit Mage(const QString& name, Owner Camp);
    void castSkill(Game& game);
};

class Archer : public Unit
{
public:
    explicit Archer(const QString& name, Owner Camp);
    void castSkill(Game& game);
};

#endif // UNIT_H
