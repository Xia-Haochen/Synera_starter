#include "bench.h"
#include "board.h"

int Bench::getUnitCount() const
{
    return m_unitCount;
}

int Bench::getMaxBenchSize() const
{
    return MAX_BENCH_SIZE;
}

void Bench::clear()
{
    m_units.fill(nullptr);
    m_unitCount = 0;
}

void Bench::addUnit(Unit* unit) 
{ 
    for(int i = 0; i < MAX_BENCH_SIZE; ++i) {
        if(!m_units[i]) {
            m_units[i] = unit;
            m_unitCount++; // 更新当前备战区单位数量
            break;
        }
    }
}

void Bench::removeUnit(Unit* unit) 
{ 
    for(int i = 0; i < MAX_BENCH_SIZE; ++i) {
        if(m_units[i] == unit) {
            m_units[i] = nullptr;
            m_unitCount--; // 更新当前备战区单位数量
            break;
        }
    }
}

QVector<Unit*> Bench::getUnits() const 
{ 
    return m_units; 
}

void Bench::moveToBoard(Board& board, int benchIndex, const QPoint& targetPos)
{
    if (benchIndex < 0 || benchIndex >= MAX_BENCH_SIZE || !board.isValidPosition(targetPos)) {
        return;
    }

    Unit* benchUnit = m_units[benchIndex];
    Unit* boardUnit = board.getUnitAt(targetPos);

    if (!benchUnit && !boardUnit) return;

    // 先从各自的容器中移除
    if (benchUnit) {
        m_units[benchIndex] = nullptr;
        m_unitCount--;
    }
    if (boardUnit) {
        board.removeUnit(boardUnit);
    }

    // 将棋子放置到目标位置（若已有棋子则实现互换）
    if (benchUnit) {
        board.addUnit(benchUnit, targetPos);
    }
    if (boardUnit) {
        m_units[benchIndex] = boardUnit;
        m_unitCount++;
    }
}

void Bench::moveFromBoard(Board& board, const QPoint& sourcePos, int benchIndex)
{
    // 这个操作在逻辑上与moveToBoard的互换是完全等价的，直接复用互换逻辑
    moveToBoard(board, benchIndex, sourcePos);
}

void Bench::moveBenchToBench(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= MAX_BENCH_SIZE || toIndex < 0 || toIndex >= MAX_BENCH_SIZE) return;
    if (fromIndex == toIndex) return;

    Unit* temp = m_units[fromIndex];
    m_units[fromIndex] = m_units[toIndex];
    m_units[toIndex] = temp;
}
