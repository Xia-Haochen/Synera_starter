#ifndef CORE_SHOP_H
#define CORE_SHOP_H

#include <QObject>
#include <QVector>
#include "entity/unit.h"

class Game;
class Bench;

class Shop : public QObject
{
    Q_OBJECT

public:
    explicit Shop(QObject* parent = nullptr);

    static const int SHOP_SIZE = 3;
    static const int UNIT_COST = 10;
    static const int SELL_PRICE = 5;
    static const int REFRESH_COST = 3;

    void rollShop();
    void clearSlot(int index);

    JobType getSlot(int index) const;
    bool isSlotEmpty(int index) const;

signals:
    void shopChanged();

private:
    struct ShopSlot {
        bool occupied = false;
        JobType job = JobType::Warrior;
    };

    QVector<ShopSlot> m_slots;
};

#endif // CORE_SHOP_H
