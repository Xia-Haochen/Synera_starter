#include "board.h"
#include <queue>
#include <vector>

Board::Board()
    : m_cells(ROWS * COLS, nullptr)
{}


// 放置单位：通过 indexOf 转为一维下标，做非空、越界和重叠检查。
void Board::addUnit(Unit* unit, const QPoint& pos)
{
    const int idx = indexOf(pos);
    if (!unit || idx < 0 || m_cells[idx]) {
        return;
    }
    m_cells[idx] = unit;
    m_unitToPosition[unit] = pos;
    unit->setPosition(pos);
}

// 移除单位：从网格和反查表中删除记录。
void Board::removeUnit(Unit* unit)
{
    if (!unit || !m_unitToPosition.contains(unit)) {
        return;
    }
    const int idx = indexOf(m_unitToPosition.value(unit));
    if (idx >= 0) {
        m_cells[idx] = nullptr;
    }
    m_unitToPosition.remove(unit);
}


// 棋盘内部移动：目标有单位时互换位置。
void Board::moveUnit(const QPoint& fromPos, const QPoint& toPos)
{
    if (!isValidPosition(fromPos) || !isValidPosition(toPos)) return;
    if (fromPos == toPos) return;

    Unit* unitFrom = getUnitAt(fromPos);
    Unit* unitTo = getUnitAt(toPos);
    if (!unitFrom && !unitTo) return;

    if (unitFrom) removeUnit(unitFrom);
    if (unitTo) removeUnit(unitTo);
    if (unitFrom) addUnit(unitFrom, toPos);
    if (unitTo) addUnit(unitTo, fromPos);
}


Unit* Board::getUnitAt(const QPoint& pos) const
{
    const int idx = indexOf(pos);
    return idx < 0 ? nullptr : m_cells[idx];
}

bool Board::hasUnitAt(const QPoint& pos) const
{
    return getUnitAt(pos) != nullptr;
}


bool Board::isValidPosition(const QPoint& pos) const
{
    return pos.x() >= 0 && pos.x() < COLS && pos.y() >= 0 && pos.y() < ROWS;
}

// y >= ROWS/2 为玩家半场（下半区），y < ROWS/2 为敌方半场（上半区）。
bool Board::isPlayerHalf(const QPoint& pos) const
{
    return pos.y() >= ROWS / 2 && pos.y() < ROWS;
}


void Board::clear()
{
    std::fill(m_cells.begin(), m_cells.end(), nullptr);
    m_unitToPosition.clear();
}


int Board::indexOf(const QPoint& pos) const
{
    if (!isValidPosition(pos)) return -1;
    return pos.y() * COLS + pos.x();
}

// QHash 需要全局 qHash 支持 QPoint。
inline uint qHash(const QPoint& key, uint seed)
{
    return qHash(key.x() * 1000 + key.y(), seed);
}


// BFS 寻路：返回前往 targetPos 的下一步坐标，无法到达或已相邻时返回 startPos。
QPoint Board::findPath(const QPoint& startPos, const QPoint& targetPos) const
{
    if (startPos == targetPos) return startPos;
    if (!isValidPosition(startPos) || !isValidPosition(targetPos)) return startPos;

    std::queue<QPoint> q;
    QHash<QPoint, QPoint> parent;
    q.push(startPos);
    parent[startPos] = startPos;

    // even-r 六边形偏移：奇数行 / 偶数行的邻居方向不同。
    int evenDirs[6][2] = {{-1, 0}, {1, 0}, {0, -1}, {1, -1}, {0, 1}, {1, 1}};
    int oddDirs[6][2]  = {{-1, 0}, {1, 0}, {-1, -1}, {0, -1}, {-1, 1}, {0, 1}};

    bool found = false;
    while (!q.empty()) {
        QPoint current = q.front();
        q.pop();
        if (current == targetPos) { found = true; break; }

        int (*dirs)[2] = (current.y() % 2 == 0) ? evenDirs : oddDirs;
        for (int i = 0; i < 6; ++i) {
            QPoint nextPos(current.x() + dirs[i][0], current.y() + dirs[i][1]);
            if (isValidPosition(nextPos) && !parent.contains(nextPos)) {
                // 避让已有单位，但目标单位所在格视为可通过（以便靠近它）。
                if (!hasUnitAt(nextPos) || nextPos == targetPos) {
                    parent[nextPos] = current;
                    q.push(nextPos);
                    if (nextPos == targetPos) { found = true; break; }
                }
            }
        }
        if (found) break;
    }
    if (!found) return startPos;

    // 回溯路径找到第一步。
    QPoint step = targetPos;
    while (parent[step] != startPos) step = parent[step];

    // 已相邻且目标有单位，则不移动（原地攻击）。
    if (step == targetPos && hasUnitAt(step)) return startPos;
    return step;
}



