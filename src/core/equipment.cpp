#include "equipment.h"
#include "../entity/unit.h"

Equipment::Equipment(EquipmentType type) : m_type(type) {}

void IronSword::applyEffect(Unit* unit) const {
    unit->set_atk(unit->get_atk() + 100);
}

void Chainmail::applyEffect(Unit* unit) const {
    unit->set_maxHp(unit->get_maxHp() + 200);
    unit->set_hp(unit->get_hp() + 200);
}

void HasteGloves::applyEffect(Unit* unit) const {
    // Action speed increased by 100%, meaning 2 actions per interval.
    // Handled in Unit/Game logic by checking if unit has HasteGloves.
}

void Sapphire::applyEffect(Unit* unit) const {
    unit->set_maxMana(unit->get_maxMana() - 10);
    if(unit->get_mana() > unit->get_maxMana()) {
        unit->set_mana(unit->get_maxMana());
    }
}