#ifndef BENCH_H
#define BENCH_H

#include <QVector>
#include <QPoint>
#include "entity/unit.h"

class Board;

class Bench
{
    // finish: TODO[T1-1]: 实现备战区数据结构，支持单位上阵/下阵逻辑（建议使用 QVector<Unit*> 来存储备战区单位列表，并提供 addUnit/removeUnit/getUnits 等接口）
    const int MAX_BENCH_SIZE = 8; // PA中备战区最大容纳单位数量
    int Bench_unit_count; // 当前备战区单位数量
    QVector<Unit*> m_units;

public:
    Bench() : Bench_unit_count(0), m_units(MAX_BENCH_SIZE, nullptr) {}
    ~Bench() = default;

    int getUnitCount() const;
    int getMaxBenchSize() const;

    void clear();

    void addUnit(Unit* unit);
    void removeUnit(Unit* unit);
    QVector<Unit*> getUnits() const;

    // finish: TODO[T1-1]: 实现备战区与棋盘之间的单位移动接口（如 moveToBoard(Board& board, int benchIndex, const QPoint& targetPos) 和 moveFromBoard(Board& board, const QPoint& sourcePos, int benchIndex)）
    void moveToBoard(Board& board, int benchIndex, const QPoint& targetPos);
    void moveFromBoard(Board& board, const QPoint& sourcePos, int benchIndex);
    
    // 增加备战区内部的自由拖动（互换）
    void moveBenchToBench(int fromIndex, int toIndex);
};

#endif // BENCH_H
