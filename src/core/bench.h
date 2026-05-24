#ifndef BENCH_H
#define BENCH_H

#include <QVector>
#include <QPoint>
#include "entity/unit.h"

class Board;

class Bench
{
public:
    // 备战区最大容纳单位数量
    static constexpr int MAX_BENCH_SIZE = 8;

    Bench() : m_unitCount(0), m_units(MAX_BENCH_SIZE, nullptr) {}
    ~Bench() = default;

    int getUnitCount() const;
    int getMaxBenchSize() const;

    void clear();

    // --- 单位管理 ---
    void addUnit(Unit* unit);
    void removeUnit(Unit* unit);
    QVector<Unit*> getUnits() const;

    // --- 棋盘与备战区之间的移动 ---
    void moveToBoard(Board& board, int benchIndex, const QPoint& targetPos);
    void moveFromBoard(Board& board, const QPoint& sourcePos, int benchIndex);

    // --- 备战区内部换位 ---
    void moveBenchToBench(int fromIndex, int toIndex);

private:
    int m_unitCount = 0;
    QVector<Unit*> m_units;
};

#endif // BENCH_H
