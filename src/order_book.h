// order_book.h —— 单标的限价订单簿（v3）
//
// v3 的三个结构性改变：
//   1. 价格用整数 tick，不再用 double 做 map 的键
//   2. 订单只存在一处（价位链表节点），index_ 里存的是指向它的两个迭代器
//   3. 撮合产出 Trade 列表，外部可以核对成交
#pragma once

#include <functional>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "order.h"

namespace ob {

class OrderBook {
public:
    explicit OrderBook(std::string symbol);

    // 下单：返回结果而不再静默丢弃。同一 id 终身只能出现一次。
    InsertResult insertOrder(const Order& order);

    // 撤单：O(1)，靠 index_ 里的迭代器直接摘节点。找不到返回 false。
    bool cancelOrder(OrderId id);

    // 撮合到不能再撮为止，返回本轮产生的成交。
    std::vector<Trade> matchOrders();

    // 盘口前五档（价格按 tick 换算回 double 打印）。
    void displayTop() const;

    Price bestBid() const;          // 无买单时返回 0
    Price bestAsk() const;          // 无卖单时返回 0
    std::size_t liveOrders() const { return index_.size(); }
    const std::string& symbol() const { return symbol_; }

private:
    using Node = std::list<Order>;                 // 一个价位上的所有订单，按到达顺序

    // 价位迭代器随 map 增删其他键而保持有效；节点迭代器随 list 增删其他节点而保持有效。
    // 这两个性质是「单一数据源」能成立的前提。
    struct Handle {
        Price price;
        Node::iterator node;
    };

    Node& levelOrCreate(Side side, Price price);

    std::string symbol_;
    std::map<Price, Node, std::greater<Price>> bids_;   // 买盘，begin() 是最高价
    std::map<Price, Node, std::less<Price>>    asks_;   // 卖盘，begin() 是最低价
    std::unordered_map<OrderId, Handle> index_;         // 订单号 -> 句柄
    std::unordered_set<OrderId> retired_;               // 用过的订单号，永不复用
    Seq seqCounter_ = 0;
};

}  // namespace ob
