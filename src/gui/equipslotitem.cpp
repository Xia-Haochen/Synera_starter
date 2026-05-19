#include "equipslotitem.h"
#include <QGraphicsSceneHoverEvent>
#include <QPainter>

EquipSlotItem::EquipSlotItem(int slotIndex, const QRectF& rect, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_slotIndex(slotIndex)
    , m_rect(rect)
    , m_baseColor(QColor(40, 40, 50))
    , m_hoverActive(false)
    , m_dropActive(false)
    , m_pointerHover(false)
{
    setAcceptHoverEvents(true);
    setZValue(1.0); // Below equipment items and units
}

QRectF EquipSlotItem::boundingRect() const
{
    // Adding slight padding for bounding rect for drawing border
    return m_rect.adjusted(-2.0, -2.0, 2.0, 2.0);
}

void EquipSlotItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    QColor fill = m_baseColor;
    QColor border = QColor(60, 60, 80);

    if (m_dropActive) {
        fill = QColor(110, 170, 110);
        border = QColor(100, 255, 100);
    } else if (m_hoverActive || m_pointerHover) {
        fill = m_baseColor.lighter(130);
    }

    painter->setPen(QPen(border, 2));
    painter->setBrush(fill);
    painter->drawRect(m_rect);

    if (m_hoverActive || m_pointerHover) {
        painter->setPen(QPen(QColor(220, 220, 220), 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(m_rect);
    }
}

void EquipSlotItem::setBaseColor(const QColor& color)
{
    m_baseColor = color;
    update();
}

void EquipSlotItem::setHoverActive(bool active)
{
    if (m_hoverActive == active) {
        return;
    }
    m_hoverActive = active;
    update();
}

void EquipSlotItem::setDropActive(bool active)
{
    if (m_dropActive == active) {
        return;
    }
    m_dropActive = active;
    update();
}

void EquipSlotItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    Q_UNUSED(event);
    if (!m_pointerHover) {
        m_pointerHover = true;
        update();
    }
}

void EquipSlotItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    Q_UNUSED(event);
    if (m_pointerHover) {
        m_pointerHover = false;
        update();
    }
}
