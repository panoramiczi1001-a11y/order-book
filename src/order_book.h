// order_book.h —— 限价订单簿
//
// 数据结构是三份，这是当前最大的结构问题：
//      buyOrders   价格 -> 该价位所有买单（按到达顺序排队，先到先撮）
//      sellOrders  价格 -> 该价位所有卖单
//      orderMap    订单号 -> 订单（用于按 id 定位）
//
// 同一笔订单被存了三份拷贝，撮合时改了 deque 里那份，必须手动
// insert_or_assign 回 orderMap 才不至于分裂。
// TODO(v3): 收成一份——orderMap 里存指向簿子节点的句柄，簿子才是唯一真相。
#pragma once

#include <deque>
#include <map>
#include <unordered_map>

#include "order.h"

class OrderBook {
public:
    // 下单：校验后写入两个索引。这里只入簿，不撮合，
    // 撮合由调用方显式调用 matchOrders()。
    void insertOrder(const Order& order);

    // 撤单：按 id 找到订单，再去对应价位队列里线性扫掉。
    void cancelOrder(int orderId);

    // 撮合：价格优先 + 时间优先，可成交则循环到不能成交为止。
    bool matchOrders();

    // 打印买卖各前五档（价格 + 该档剩余量合计）。
    void displayTop() const;

private:
    // 买盘：价格降序，begin() 是最高买价。
    // 卖盘：价格升序，begin() 是最低价。
    // TODO(v3): 键是 double，见 order.h 里的说明。
    std::map<double, std::deque<Order>, std::greater<double>> buyOrders;
    std::map<double, std::deque<Order>, std::less<double>> sellOrders;
    // 全量索引：订单号 -> 订单副本（第三份数据）。
    std::unordered_map<int, Order> orderMap;
};
