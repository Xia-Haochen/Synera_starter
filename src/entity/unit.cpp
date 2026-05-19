#include <cmath>
#include "unit.h"
#include "core/game.h"

int Unit::s_nextId = 0;

int distance(const QPoint& a, const QPoint& b) // 计算六边形格子距离，基于 cube 坐标系的距离公式，适配 even-r
{
    int qa = a.x() - (a.y() + (a.y() & 1)) / 2;
    int ra = a.y();
    int sa = -qa - ra;

    int qb = b.x() - (b.y() + (b.y() & 1)) / 2;
    int rb = b.y();
    int sb = -qb - rb;

    return std::max({std::abs(qa - qb), std::abs(ra - rb), std::abs(sa - sb)});
}

Unit::Unit(const QString& name)
    : m_id(s_nextId++)
    , m_name(name)
    , m_position(0, 0)
    , HP(100000)
    , Max_HP(100000)
    , ATK(100)
    , Max_Mana(100)
    , Mana(0)
    , Range(1)
    , is_alive(true)
    , is_Dizzy(false)
    , has_target(false)
    , target(nullptr)
    , owner(Owner::PlayerCtrl)
    , Star_Level(1)
    , Trait_Level(0)
    , Base_Max_HP(100000)
    , Base_ATK(100)
    , Base_Range(1)
    , Base_Max_Mana(100)
    // finish: TODO[T0-1/T1-4]: 在这里通过初始化列表或内部代码，给 HP/ATK/Max_Mana 等战斗属性赋初值
    // finish: TODO[T0-1]: 初始化 owner 阵营标识、是否存活 is_alive=true、法力值 Mana=0、以及默认星阶 1
    // TODO[T2-2]: 将当前动作状态设置为 Idle（空闲不找目标）
{
    // finish: TODO[T0-2]: 各个派生继承类（如战士、法师等）将会调用带有不同基础面板的构造函数，可以考虑重新设计一个接受基础模板数据的构造或添加Init方法
}

Unit::Unit(const QString& name, Role_Template t)
    : m_id(s_nextId++)
    , m_name(name)
    , m_position(0, 0)
    , HP(t.HP)
    , Max_HP(t.HP)
    , ATK(t.ATK)
    , Max_Mana(t.Max_Mana)
    , Mana(0) // 初始蓝量为0
    , Range(t.Range)
    , is_alive(true)
    , is_Dizzy(false)
    , has_target(false)
    , target(nullptr)
    , owner(Owner::PlayerCtrl)
    , Star_Level(1)
    , Trait_Level(0)
    , Base_Max_HP(t.HP)
    , Base_ATK(t.ATK)
    , Base_Range(t.Range)
    , Base_Max_Mana(t.Max_Mana)
{
}

Unit::Unit(const Unit& other)
    : m_id(s_nextId++)
    , m_name(other.m_name)
    , m_position(other.m_position)
    , HP(other.HP)
    , Max_HP(other.Max_HP)
    , ATK(other.ATK)
    , Max_Mana(other.Max_Mana)
    , Mana(other.Mana)
    , Range(other.Range)
    , is_alive(other.is_alive)
    , is_Dizzy(other.is_Dizzy)
    , has_target(other.has_target)
    , target(other.target) // Warning：浅复制指针，后续可能需要改
    , owner(other.owner)
    , Star_Level(other.Star_Level)
    , Trait_Level(other.Trait_Level)
    , Base_Max_HP(other.Base_Max_HP)
    , Base_ATK(other.Base_ATK)
    , Base_Range(other.Base_Range)
    , Base_Max_Mana(other.Base_Max_Mana)
{
    // TODO[T3-4]: 复制构造函数，确保在合成进阶（3合1）时正确复制单位属性和状态
}

Unit::~Unit()
{
    for (auto* eq : m_equipments) delete eq;
    m_equipments.clear();
}

void Unit::find_target(Game& game)
{
    // TODO[T2-2]: 实现寻找目标的逻辑，通常是扫描敌方单位列表，找到距离最近且在攻击范围内的单位，并设置 has_target=true 和 target 指针
    int minDist = std::numeric_limits<int>::max(); // 初始化为最大值以便后续比较
    Unit *potentialTarget = nullptr;
    for(auto& unit : game.getUnitList()) {
        if(unit->get_owner() != owner && unit->get_isAlive()) {
            int dist = distance(m_position, unit->position());
            if(dist < minDist) {
                minDist = dist;
                potentialTarget = unit;
            }
        }
    }
    if(potentialTarget) {
        has_target = true;
        target = potentialTarget;
    } else {
        has_target = false;
        target = nullptr;
    }
}

void Unit::action(Game& game)
{
    if(!is_alive) {
        return;
    }
    if(is_Dizzy) {
        is_Dizzy = false;
        return;
    }
    if(!has_target || !target || !target->get_isAlive()) {
        find_target(game);
    }
    if(has_target && target && target->get_isAlive()) {
        int dist = distance(m_position, target->position());
        if (dist <= Range) {
            if(Mana >= Max_Mana) {
                castSkill(game);
                Mana = 0;
            } else {
                attack(target);
            }
        } else {
            QPoint nextStep = game.get_board().findPath(m_position, target->position());
            if (nextStep != m_position) {
                game.get_board().moveUnit(m_position, nextStep);
            }
        }
    }
}

void Unit::attack(Unit* targetUnit)
{
    // finish: TODO[T2-5]: 实现攻击逻辑，计算伤害并应用到目标单位
    targetUnit->takeDamage(ATK);
    if(!targetUnit->get_isAlive()) {
        has_target = false; // 目标死了，清除攻击目标
        target = nullptr;
    }
    Mana = (Mana + 10) < Max_Mana ? (Mana + 10) : Max_Mana; // 每次普攻回10点蓝，且不超过最大蓝量
}

void Unit::takeDamage(int amount)
{
    // finish: TODO[T2-5]: 实现受击逻辑，降低当前 HP，若 HP<=0 标为死亡，清空站位
    if(!is_alive) {
        return;
    }
    HP -= amount;
    if(HP <= 0) {
        HP = 0;
        is_alive = false;
        // 清空站位的操作交由 Game 统一在结算或者检测死亡时 m_board.removeUnit 去处理。
    }
}

void Unit::applyTraitEffects()
{
    // 如果没有羁绊，恢复基础属性
    int oldMaxHp = Max_HP;
    
    if (m_job == JobType::Warrior) {
        if (Trait_Level == 1) {
            Max_HP = Base_Max_HP * 1.5;
        } else if (Trait_Level >= 2) {
            Max_HP = Base_Max_HP * 2.0;
        } else {
            Max_HP = Base_Max_HP;
        }
        ATK = Base_ATK; // 战士不加攻击
        Range = Base_Range;
        Max_Mana = Base_Max_Mana;
    } else if (m_job == JobType::Archer) {
        if (Trait_Level >= 1) {
            ATK = Base_ATK * 1.5;
        } else {
            ATK = Base_ATK;
        }
        Max_HP = Base_Max_HP; // 弓手不加血量
        Range = Base_Range;
        Max_Mana = Base_Max_Mana;
    } else if (m_job == JobType::Mage) {
        Max_HP = Base_Max_HP;
        ATK = Base_ATK;
        Range = Base_Range;
        Max_Mana = Base_Max_Mana;
    }
    
    // 如果最大生命值发生变化，按比例调整当前血量
    if (oldMaxHp > 0 && Max_HP != oldMaxHp) {
        double ratio = static_cast<double>(HP) / oldMaxHp;
        HP = Max_HP * ratio;
        if (HP <= 0 && is_alive) HP = 1;
    }

    // Apply equipment effects after base and trait
    for (auto* eq : m_equipments) {
        eq->applyEffect(this);
    }
}

bool Unit::add_equipment(Equipment* eq) {
    if (m_equipments.size() >= 1) {
        return false;
    }
    m_equipments.push_back(eq);
    applyTraitEffects(); // Re-evaluate total stats
    return true;
}

void Unit::remove_equipment(Equipment* eq) {
    auto it = std::find(m_equipments.begin(), m_equipments.end(), eq);
    if (it != m_equipments.end()) {
        m_equipments.erase(it);
        applyTraitEffects();
    }
}

bool Unit::has_haste_gloves() const {
    for (auto* eq : m_equipments) {
        if (eq->getType() == EquipmentType::HasteGloves) return true;
    }
    return false;
}

// finish: TODO[T2-5]: 实现 attack(Unit* target) 虚函数，加入攻速（每 x 帧一次）逻辑，每次普攻计算伤害并令自身每次普攻回复10点蓝

// finish: TODO[T2-5]: 实现 takeDamage(int amount) 虚函数，收到伤害降低当前 HP，若 HP<=0 标为死亡，清空站位

// finish: TODO[T3-4/T3-5]: 析构时清理装备资源，合并/出售时退还装备到装备栏

Warrior::Warrior(const QString& name, Owner Camp)
    : Unit(name, RoleTemplate<JobType::Warrior>().attr)
{
    m_job = JobType::Warrior;
    owner = Camp;
}

void Warrior::castSkill(Game& game)
{
    // finish: TODO[T0-2/T2-5]: 实现战士技能逻辑（如眩晕敌人1秒）
    get_Target()->set_isDizzy(true);
    set_atk(get_atk() * 1.2); // 眩晕后攻击力提升20%（示例效果）
    attack(get_Target()); // 眩晕后执行一次攻击
    return;
}

Mage::Mage(const QString& name, Owner Camp)
    : Unit(name, RoleTemplate<JobType::Mage>().attr)
{
    m_job = JobType::Mage;
    owner = Camp;
}

void Mage::castSkill(Game& game)
{
    // finish: TODO[T0-2/T2-5]: 实现法师技能逻辑（如释放火球术）
    if (!get_Target()) return; // 防御性判断

    QPoint centerPos = get_Target()->position();

    // even-r 六边形偏移：偶数行和奇数行的6个邻居方向不同
    int offsets[6][2];
    if (centerPos.y() % 2 == 0) {
        offsets[0][0] = -1; offsets[0][1] = -1;
        offsets[1][0] =  0; offsets[1][1] = -1;
        offsets[2][0] = -1; offsets[2][1] =  0;
        offsets[3][0] =  1; offsets[3][1] =  0;
        offsets[4][0] = -1; offsets[4][1] =  1;
        offsets[5][0] =  0; offsets[5][1] =  1;
    } else {
        offsets[0][0] =  0; offsets[0][1] = -1;
        offsets[1][0] =  1; offsets[1][1] = -1;
        offsets[2][0] = -1; offsets[2][1] =  0;
        offsets[3][0] =  1; offsets[3][1] =  0;
        offsets[4][0] =  0; offsets[4][1] =  1;
        offsets[5][0] =  1; offsets[5][1] =  1;
    }

    // 灼烧目标本身
    Unit* centerUnit = game.get_board().getUnitAt(centerPos);
    if (centerUnit && centerUnit->get_owner() != owner) {
        centerUnit->takeDamage(get_atk());
        if (get_traitLevel() >= 1) centerUnit->set_isDizzy(true);
    }

    // 灼烧周围6个方向的敌人
    for (int i = 0; i < 6; ++i) {
        QPoint targetPos = centerPos + QPoint(offsets[i][0], offsets[i][1]);
        Unit* targetUnit = game.get_board().getUnitAt(targetPos);
        if (targetUnit && targetUnit->get_owner() != owner) {
            targetUnit->takeDamage(get_atk());
            if (get_traitLevel() >= 1) targetUnit->set_isDizzy(true);
        }
    }
    return;
}

Archer::Archer(const QString& name, Owner Camp)
    : Unit(name, RoleTemplate<JobType::Archer>().attr)
{
    m_job = JobType::Archer;
    owner = Camp;
}

void Archer::castSkill(Game& game)
{
    // finish: TODO[T0-2/T2-5]: 实现射手技能逻辑（如对目标进行斩杀）
    for(int i = 0; i < 2; i++) {
        if(get_Target()) { // 修复：必须确保目标还存活且非空才能继续攻击
            attack(get_Target()); // 先攻击目标2次
        }
    }
    if(get_Target() && get_Target()->get_hp() <= get_atk()) { // 如果目标血量低于攻击力，执行斩杀
        get_Target()->takeDamage(get_Target()->get_hp()); // 直接将目标血量降为0
        set_hasTarget(false); // 斩杀后目标死了，清除攻击目标
        set_Target(nullptr);
    }
    return;
}