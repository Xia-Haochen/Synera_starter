#include "board.h"
#include <queue>
#include <vector>

// 构造函数：初始化时为底层一维数组申请 ROWS * COLS 大小空间，并将整个网格初始化设置为 nullptr 没有占用单位
Board::Board()
    : m_cells(ROWS * COLS, nullptr)
{}

// 放置单位核心逻辑：通过 indexOf 转换为一维下标并做非空、越界以及重叠检查。
//PA要求同一地块只能容纳一个单位如果在同一个格内发生，那么更新记录。
void Board::addUnit(Unit* unit, const QPoint& pos)
{
    const int idx = indexOf(pos);
    if (!unit || idx < 0 || m_cells[idx]) {
        return;
    }

    m_cells[idx] = unit;
    m_unitToPosition[unit] = pos;
    unit->setPosition(pos); // 同时把坐标信息同步在单位实例里去
}

// 移除单位：先在哈希表中查看是否包含此单位，再将网格中对应的单位数组位置置空，最后从哈希表删除记录
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

// 棋盘内部单位移动（遇到重叠时互换位置）
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

// 查询某坐标下的单位：同样计算出对应一维数组索引下标，如果是有效索引则返回该位置的实例指针
Unit* Board::getUnitAt(const QPoint& pos) const
{
    const int idx = indexOf(pos);
    return idx < 0 ? nullptr : m_cells[idx];
}

// 检查某个坐标是否存在单位：如果通过该坐标查到的不为空，则说明有单位对象在该地块
bool Board::hasUnitAt(const QPoint& pos) const
{
    return getUnitAt(pos) != nullptr;
}

// 安全边界范围检查：x 取值范围必须在 [0, COLS-1] 内，同时 y 的范围在 [0, ROWS-1] 之间
bool Board::isValidPosition(const QPoint& pos) const
{
    return pos.x() >= 0 && pos.x() < COLS && pos.y() >= 0 && pos.y() < ROWS;
}

// PA描述玩家半场在下半区，敌方半场在上半区：基于 y 坐标（原点0在顶部向下递增或者符合图形渲染规律），即y坐标超过网格总行数是一半即为玩家场地
bool Board::isPlayerHalf(const QPoint& pos) const
{
    return pos.y() >= ROWS / 2;
}

// 清空当前棋盘内存结构操作：利用标准库 std::fill 粗暴覆盖全体为空，清空位置查找表字典。常用于关卡结束转换
void Board::clear()
{
    std::fill(m_cells.begin(), m_cells.end(), nullptr);
    m_unitToPosition.clear();
}

// 将传入进来的 QPoint 坐标从二维拍平为一维（二维数组展平算法）：公式是索引等于 "y * 总列数 + x"
int Board::indexOf(const QPoint& pos) const
{
    if (!isValidPosition(pos)) {
        return -1;
    }
    return pos.y() * COLS + pos.x();
}

// QHash需要为QPoint提供全局的qHash支持
inline uint qHash(const QPoint& key, uint seed = 0)
{
    return qHash(key.x() * 1000 + key.y(), seed);
}

// 寻路算法，返回前往 targetPos 应该迈出的下一步。如果无法到达或已在旁边，则返回 startPos
QPoint Board::findPath(const QPoint& startPos, const QPoint& targetPos) const
{
    if (startPos == targetPos) return startPos;
    if (!isValidPosition(startPos) || !isValidPosition(targetPos)) return startPos;

    std::queue<QPoint> q;
    QHash<QPoint, QPoint> parent;
    q.push(startPos);
    parent[startPos] = startPos;
    
    // 偏置坐标系(even-r)：偶数行和奇数行的周围6个方向偏移量不同
    // 注意：x是列，y是行
    int evenDirs[6][2] = {{-1, 0}, {1, 0}, {0, -1}, {1, -1}, {0, 1}, {1, 1}};
    int oddDirs[6][2] = {{-1, 0}, {1, 0}, {-1, -1}, {0, -1}, {-1, 1}, {0, 1}};

    bool found = false;

    while (!q.empty()) {
        QPoint current = q.front();
        q.pop();

        if (current == targetPos) {
            found = true;
            break;
        }

        int (*dirs)[2] = (current.y() % 2 == 0) ? evenDirs : oddDirs;

        for (int i = 0; i < 6; ++i) {
            QPoint nextPos(current.x() + dirs[i][0], current.y() + dirs[i][1]);
            
            if (isValidPosition(nextPos) && !parent.contains(nextPos)) {
                // 避让已有单位（目标单位所在位置允许看作可通过，以便靠近它）
                if (!hasUnitAt(nextPos) || nextPos == targetPos) {
                    parent[nextPos] = current;
                    q.push(nextPos);
                    if (nextPos == targetPos) {
                        found = true;
                        break;
                    }
                }
            }
        }
        if (found) break;
    }

    if (!found) {
        return startPos; // 无法到达
    }

    // 回溯路径找到第一步
    QPoint step = targetPos;
    while (parent[step] != startPos) {
        step = parent[step];
    }
    
    // 如果第一步就是目标（即已经相邻），且目标位置有单位占用，则不移动（保持原地即可攻击）
    if (step == targetPos && hasUnitAt(step)) {
        return startPos;
    }
    
    return step;
}



