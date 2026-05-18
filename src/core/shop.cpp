#include "shop.h"
#include <cstdlib>

Shop::Shop(QObject* parent)
    : QObject(parent)
    , m_slots(SHOP_SIZE)
{
}

void Shop::rollShop()
{
    for (int i = 0; i < SHOP_SIZE; ++i) {
        m_slots[i].occupied = true;
        int r = rand() % 3;
        m_slots[i].job = static_cast<JobType>(r);
    }
    emit shopChanged();
}

void Shop::clearSlot(int index)
{
    if (index >= 0 && index < SHOP_SIZE) {
        m_slots[index].occupied = false;
        emit shopChanged();
    }
}

void Shop::setSlot(int index, JobType job)
{
    if (index >= 0 && index < SHOP_SIZE) {
        m_slots[index].occupied = true;
        m_slots[index].job = job;
        emit shopChanged();
    }
}

JobType Shop::getSlot(int index) const
{
    return m_slots[index].job;
}

bool Shop::isSlotEmpty(int index) const
{
    return !m_slots[index].occupied;
}
