#ifndef GUI_ITEMS_EQUIPSLOTITEM_H
#define GUI_ITEMS_EQUIPSLOTITEM_H

#include <QGraphicsObject>
#include <QRectF>

class QGraphicsSceneHoverEvent;

class EquipSlotItem : public QGraphicsObject
{
    Q_OBJECT

public:
    EquipSlotItem(int slotIndex, const QRectF& rect, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    int slotIndex() const { return m_slotIndex; }
    
    void setBaseColor(const QColor& color);
    void setHoverActive(bool active);
    void setDropActive(bool active);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    int m_slotIndex;
    QRectF m_rect;
    QColor m_baseColor;
    bool m_hoverActive;
    bool m_dropActive;
    bool m_pointerHover;
};

#endif // GUI_ITEMS_EQUIPSLOTITEM_H
