#ifndef BOARD_H
#define BOARD_H

#include <QHash>
#include <QPoint>
#include <QVector>
#include "entity/unit.h"

// Board 表示 M x N 的主棋盘网格，分为玩家半场（下半区）与敌人半场（上半区）。
class Board
{
public:
    static constexpr int ROWS = 8;
    static constexpr int COLS = 8;

    Board();
    ~Board() = default;

    // --- 单位占位管理 ---
    void addUnit(Unit* unit, const QPoint& pos);
    void removeUnit(Unit* unit);
    void moveUnit(const QPoint& fromPos, const QPoint& toPos);
    void clear();

    // --- 坐标查询 ---
    Unit* getUnitAt(const QPoint& pos) const;
    bool hasUnitAt(const QPoint& pos) const;
    bool isValidPosition(const QPoint& pos) const;
    bool isPlayerHalf(const QPoint& pos) const;

    // --- 寻路（BFS） ---
    QPoint findPath(const QPoint& startPos, const QPoint& targetPos) const;

private:
    // 将 QPoint 映射为一维数组索引：y * COLS + x
    int indexOf(const QPoint& pos) const;

    QVector<Unit*> m_cells;              // 一维数组，大小 ROWS * COLS
    QHash<Unit*, QPoint> m_unitToPosition; // 单位 → 坐标 反查
};

#endif // BOARD_H
