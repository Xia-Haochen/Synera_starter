#ifndef UNIT_H
#define UNIT_H

#include <QPoint>
#include <QString>

class Game;

#include "core/equipment.h"
#include <vector>

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
            attr = {1800, 350, 1, 30};
        }
        else if constexpr (T == JobType::Mage)
        {
            attr = {900, 300, 3, 50};
        }
        else if constexpr (T == JobType::Archer)
        {
            attr = {1200, 250, 2, 40};
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
    virtual ~Unit();

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

    int get_traitLevel() const { return Trait_Level; }
    int get_baseMaxHp() const { return Base_Max_HP; }
    int get_baseAtk() const { return Base_ATK; }
    int get_baseRange() const { return Base_Range; }
    int get_baseMaxMana() const { return Base_Max_Mana; }
    
    // Equipment related
    const std::vector<Equipment*>& get_equipments() const { return m_equipments; }
    bool add_equipment(Equipment* eq);
    void remove_equipment(Equipment* eq);
    bool has_haste_gloves() const;

    void set_traitLevel(int level) { Trait_Level = level; }
    void set_baseMaxHp(int hp) { Base_Max_HP = hp; }
    void set_baseAtk(int atk) { Base_ATK = atk; }
    void set_baseRange(int range) { Base_Range = range; }
    void set_baseMaxMana(int mana) { Base_Max_Mana = mana; }
    void applyTraitEffects();

    void set_hp(int hp) { HP = hp; }
    void set_maxHp(int maxHp) { Max_HP = maxHp; }
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
    
    static void setNextId(int id) { s_nextId = id; }

    void find_target(Game& game);
    // 每帧更新的战斗逻辑：移动 / 攻击 / 施法
    virtual void action(Game& game);
    // 普攻，附带 10 点回蓝
    virtual void attack(Unit* target);
    // 技能（纯虚函数，由派生类各自实现）
    virtual void castSkill(Game& game) = 0;
    // 受击扣血，HP ≤ 0 时标记死亡
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

    // 星级属性（1-3星，3合1进阶）
    int Star_Level;
    int Trait_Level;
    // 无羁绊加成时的基础属性
    int Base_Max_HP;
    int Base_ATK;
    int Base_Range;
    int Base_Max_Mana;
    std::vector<Equipment*> m_equipments;
    // TODO[T2-5]: 增加战斗属性扩展：Attack_Speed、Move_Speed
    // TODO[T3-3]: 增加羁绊属性标签 traits，同种羁绊触发 Buff
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
