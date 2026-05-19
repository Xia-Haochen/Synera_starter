#ifndef GUI_ITEMS_EQUIPITEM_H
#define GUI_ITEMS_EQUIPITEM_H

#include <QGraphicsObject>
#include <QPointF>

class Equipment;

class EquipItem : public QGraphicsObject
{
    Q_OBJECT

public:
    explicit EquipItem(Equipment* equipment, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    Equipment* equipment() const { return m_equipment; }
    
    void setSlotIndex(int index) { m_slotIndex = index; }
    int slotIndex() const { return m_slotIndex; }

signals:
    void dragStarted(Equipment* equipment, int sourceSlot, const QPointF& scenePos);
    void dragMoved(Equipment* equipment, int sourceSlot, const QPointF& scenePos);
    void dragDropped(Equipment* equipment, int sourceSlot, const QPointF& scenePos);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    Equipment* m_equipment;
    int m_slotIndex;
    bool m_dragging;
};

#endif // GUI_ITEMS_EQUIPITEM_H
