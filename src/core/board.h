#ifndef BOARD_H
#define BOARD_H

// Qt引入的库类说明：
// QHash: 类似 std::unordered_map，用于存储键值对，用于快速反查
// QPoint: 表示二维坐标系中的一个点(x, y)
// QVector: 类似 std::vector的动态数组容器
#include <QHash>
#include <QPoint>
#include <QVector>
#include "entity/unit.h"

// Board类表示游戏中的棋盘逻辑，它是一个M x N的网格区域
// 根据PA文档要求，主棋盘分为玩家半场与敌人半场。
class Board
{
public:
    // 棋盘行数和列数，默认8x8，对应PA中的 M x N 规格
    static constexpr int ROWS = 8;
    static constexpr int COLS = 8;

    Board();
    ~Board() = default;

    // 添加单位到棋盘指定坐标 (对应地块的占用逻辑)
    void addUnit(Unit* unit, const QPoint& pos);
    // 从棋盘中移除实体单位
    void removeUnit(Unit* unit);
    // 棋盘内部单位移动（如果目标位置已有单位，则完成位置互换）
    void moveUnit(const QPoint& fromPos, const QPoint& toPos);
    // 获取指定坐标点上的单位指针，如果越界或该点无单位则返回 nullptr
    Unit* getUnitAt(const QPoint& pos) const;
    // 检查指定坐标的地块上是否已被占用
    bool hasUnitAt(const QPoint& pos) const;

    // 判断传入的二维坐标是否在棋盘内部合法范围
    bool isValidPosition(const QPoint& pos) const;
    // PA文档中区分为玩家半场（下半区）和敌人半场（上半区），用来判断该点是否在我方半场
    bool isPlayerHalf(const QPoint& pos) const;

    // 清空棋盘上的所有地块数据
    void clear();

    // 实现寻路算法(BFS)帮助单位移动时计算可达性（返回下一步走哪个坐标）
    QPoint findPath(const QPoint& startPos, const QPoint& targetPos) const;

    // TODO[T2-3]: 实现获取当前最接近敌方的索敌函数（根据欧氏距离+从左到右等平局判断找 target Unit*）
    // TODO[T2-3]: 实现两点之间欧氏距离的辅助计算接口，以便所有Unit能在索敌时调用判断攻击范围
    // TODO[T3-4]: 当单位需要合并（同名同星级3合1）时，实现备战区到场上同名单位查找过滤并移除另外2个合并的过程

private:
    // 核心辅助算法：把二维的 QPoint(x,y) 坐标映射转换为一维数组中的索引 (y * 宽 + x)
    int indexOf(const QPoint& pos) const;

    // 存储棋盘上每个地块所占用的单位指针。利用一维数组管理二维网格实体，大小为ROWS * COLS
    QVector<Unit*> m_cells;
    // 单位与棋盘坐标的映射字典，主要用于提供高效率反向查询 (即通过Unit*快速找到它在棋盘的哪一格)
    QHash<Unit*, QPoint> m_unitToPosition;
};

#endif // BOARD_H
