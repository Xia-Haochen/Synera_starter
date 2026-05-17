#ifndef GUI_ITEMS_UNITITEM_H
#define GUI_ITEMS_UNITITEM_H

#include <QGraphicsObject>
#include <QPoint>
#include <QPixmap>

class Unit;

// UnitItem 是 Unit 的可视化图元，负责：
// - 绘制单位贴图/占位图
// - 处理鼠标拖拽并发出逻辑事件信号
class UnitItem : public QGraphicsObject
{
    Q_OBJECT

public:
    explicit UnitItem(Unit* unit, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    Unit* unit() const { return m_unit; }
    int unitId() const;

    // 由 Game 在同步阶段写入当前网格坐标。
    void setGridPos(const QPoint& gridPos);
    QPoint gridPos() const { return m_gridPos; }

    void setHighlighted(bool highlighted) { m_highlighted = highlighted; update(); }

signals:
    // 拖拽生命周期事件，交由 Game 统一处理规则。
    void dragStarted(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);
    void dragMoved(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);
    void dragDropped(int unitId, const QPoint& sourceGrid, const QPointF& scenePos);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    // 贴图懒加载：首帧绘制时尝试读取资源。
    void ensureSpriteLoaded() const;
    QString spriteRelativePathForUnit() const;

    Unit* m_unit;
    QPoint m_gridPos;
    bool m_dragging;
    mutable QPixmap m_sprite;
    mutable bool m_spriteTried;
    bool m_highlighted = false;
};

#endif // GUI_ITEMS_UNITITEM_H
