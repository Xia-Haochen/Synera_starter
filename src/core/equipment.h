#ifndef EQUIPMENT_H
#define EQUIPMENT_H

#include <QString>

class Unit;

enum class EquipmentType {
    IronSword,
    Chainmail,
    HasteGloves,
    Sapphire
};

class Equipment {
public:
    Equipment(EquipmentType type);
    virtual ~Equipment() = default;

    EquipmentType getType() const { return m_type; }
    QString getName() const { return m_name; }

    virtual void applyEffect(Unit* unit) const = 0;

protected:
    EquipmentType m_type;
    QString m_name;
};

class IronSword : public Equipment {
public:
    IronSword() : Equipment(EquipmentType::IronSword) { m_name = "铁剑"; }
    void applyEffect(Unit* unit) const override;
};

class Chainmail : public Equipment {
public:
    Chainmail() : Equipment(EquipmentType::Chainmail) { m_name = "护甲"; }
    void applyEffect(Unit* unit) const override;
};

class HasteGloves : public Equipment {
public:
    HasteGloves() : Equipment(EquipmentType::HasteGloves) { m_name = "手套"; }
    void applyEffect(Unit* unit) const override;
};

class Sapphire : public Equipment {
public:
    Sapphire() : Equipment(EquipmentType::Sapphire) { m_name = "水晶"; }
    void applyEffect(Unit* unit) const override;
};

#endif // EQUIPMENT_H