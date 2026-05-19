#include "equipitem.h"
#include "core/equipment.h"
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QCursor>

EquipItem::EquipItem(Equipment* equipment, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_equipment(equipment)
    , m_slotIndex(-1)
    , m_dragging(false)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setZValue(3.0); // Make sure it's above normal board layers
}

QRectF EquipItem::boundingRect() const
{
    return QRectF(-20, -20, 40, 40);
}

void EquipItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing);

    QColor baseColor(80, 80, 80);
    QString shortName = "???";

    if (m_equipment) {
        shortName = m_equipment->getName().left(4);
        switch (m_equipment->getType()) {
        case EquipmentType::IronSword:
            baseColor = QColor(160, 160, 160);
            break;
        case EquipmentType::Chainmail:
            baseColor = QColor(120, 130, 140);
            break;
        case EquipmentType::HasteGloves:
            baseColor = QColor(200, 160, 80);
            break;
        case EquipmentType::Sapphire:
            baseColor = QColor(80, 120, 220);
            break;
        }
    }

    // Draw equipment item background
    painter->setPen(QPen(Qt::black, 2));
    painter->setBrush(baseColor);
    painter->drawRoundedRect(QRectF(-18, -18, 36, 36), 4, 4);

    // Draw localized / short name
    painter->setPen(Qt::white);
    QFont font = painter->font();
    font.setPointSize(8);
    font.setBold(true);
    painter->setFont(font);
    painter->drawText(boundingRect(), Qt::AlignCenter, shortName);
}

void EquipItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        setCursor(Qt::ClosedHandCursor);
        setZValue(100.0); // Bring to very front when dragging
        emit dragStarted(m_equipment, m_slotIndex, event->scenePos());
        event->accept();
    } else {
        QGraphicsObject::mousePressEvent(event);
    }
}

void EquipItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_dragging) {
        setPos(event->scenePos());
        emit dragMoved(m_equipment, m_slotIndex, event->scenePos());
        event->accept();
    } else {
        QGraphicsObject::mouseMoveEvent(event);
    }
}

void EquipItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
        setZValue(3.0); // Reset Z-value back to normal
        emit dragDropped(m_equipment, m_slotIndex, event->scenePos());
        event->accept();
    } else {
        QGraphicsObject::mouseReleaseEvent(event);
    }
}
