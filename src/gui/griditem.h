#ifndef GUI_ITEMS_GRIDITEM_H
#define GUI_ITEMS_GRIDITEM_H

#include <QGraphicsObject>
#include <QPolygonF>

class QGraphicsSceneHoverEvent;

// GridItem 表示单个六边形格子图元，负责：
// - 绘制基础底色
// - 响应 hover / 可落子高亮
class GridItem : public QGraphicsObject
{
    Q_OBJECT

public:
    GridItem(int row, int col, const QPolygonF& polygon, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    int row() const { return m_row; }
    int col() const { return m_col; }
    QPoint gridPos() const;

    // 三种视觉状态：底色、悬浮高亮、可落子高亮。
    void setBaseColor(const QColor& color);
    void setHoverActive(bool active);
    void setDropActive(bool active);

    // 标记为出售区域（显示 "SELL" 文本）
    void setSellZone(bool isSellZone);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    // 网格逻辑坐标（row/col）与绘制几何。
    int m_row;
    int m_col;
    QPolygonF m_polygon;
    QRectF m_bounds;
    QColor m_baseColor;
    bool m_hoverActive;
    bool m_dropActive;
    bool m_pointerHover;
    bool m_isSellZone;
};

#endif // GUI_ITEMS_GRIDITEM_H
