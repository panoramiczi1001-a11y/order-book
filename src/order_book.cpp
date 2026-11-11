#include "order_book.h"

#include <algorithm>
#include <iostream>

namespace ob {

OrderBook::OrderBook(std::string symbol) : symbol_(std::move(symbol)) {}

OrderBook::Node& OrderBook::levelOrCreate(Side side, Price price) {
    if (side == Side::Buy) return bids_[price];
    return asks_[price];
}

InsertResult OrderBook::insertOrder(const Order& order) {
    if (order.id() <= 0) return InsertResult::BadId;
    if (order.price() <= 0) return InsertResult::BadPrice;   // NaN 在 to_ticks 里已变成 0
    if (order.remaining() <= 0) return InsertResult::BadQty;
    if (order.side() != Side::Buy && order.side() != Side::Sell) return InsertResult::BadSide;

    // 订单号终身唯一：既不能和在场订单重复，也不能用已经成交/撤销掉的老号
    if (index_.count(order.id()) || retired_.count(order.id())) {
        return InsertResult::DuplicateId;
    }

    Order o = order;
    o.setSeq(seqCounter_++);            // 到达序号，用于时间优先与成交价判定

    Node& level = levelOrCreate(o.side(), o.price());
    level.push_back(std::move(o));
    auto nodeIt = std::prev(level.end());

    index_[nodeIt->id()] = Handle{nodeIt->price(), nodeIt};
    return InsertResult::Ok;
}

bool OrderBook::cancelOrder(OrderId id) {
    auto it = index_.find(id);
    if (it == index_.end()) return false;

    // 句柄里存的是价位与链表节点；摘除时用节点迭代器，O(1)
    const Handle h = it->second;
    const Side side = (*h.node).side();

    if (side == Side::Buy) {
        auto levelIt = bids_.find(h.price);
        levelIt->second.erase(h.node);
        if (levelIt->second.empty()) bids_.erase(levelIt);
    } else {
        auto levelIt = asks_.find(h.price);
        levelIt->second.erase(h.node);
        if (levelIt->second.empty()) asks_.erase(levelIt);
    }

    index_.erase(it);
    retired_.insert(id);      // 用过的号永久保留，防止迟到撤单误伤新单
    return true;
}

std::vector<Trade> OrderBook::matchOrders() {
    std::vector<Trade> trades;

    while (!bids_.empty() && !asks_.empty()) {
        auto bidLevelIt = bids_.begin();
        auto askLevelIt = asks_.begin();

        if (bidLevelIt->first < askLevelIt->first) break;   // 买价低于卖价，无交叉

        Order& bid = bidLevelIt->second.front();
        Order& ask = askLevelIt->second.front();

        const Qty qty = std::min(bid.remaining(), ask.remaining());

        // 成交价取先到的一方（maker）。序号小的先到。
        const bool bidIsMaker = bid.seq() < ask.seq();
        const Price px = bidIsMaker ? bid.price() : ask.price();

        bid.fill(qty);
        ask.fill(qty);
        trades.push_back(Trade{bid.id(), ask.id(), px, qty, bidIsMaker ? bid.seq() : ask.seq()});

        // 成交后可能要从簿子里摘掉，摘之前先把 id 记下来
        const OrderId bidId = bid.id();
        const OrderId askId = ask.id();
        const bool bidDone = bid.isFilled();
        const bool askDone = ask.isFilled();

        if (bidDone) cancelOrder(bidId);
        if (askDone) cancelOrder(askId);
    }

    return trades;
}

void OrderBook::displayTop() const {
    std::cout << "Top five buy orders:" << std::endl;
    int n = 0;
    for (auto it = bids_.begin(); it != bids_.end() && n < 5; ++it, ++n) {
        Qty total = 0;
        for (const auto& o : it->second) total += o.remaining();
        std::cout << "Price: " << to_price(it->first) << "  Qty: " << total << std::endl;
    }

    std::cout << "Top five sell orders:" << std::endl;
    n = 0;
    for (auto it = asks_.begin(); it != asks_.end() && n < 5; ++it, ++n) {
        Qty total = 0;
        for (const auto& o : it->second) total += o.remaining();
        std::cout << "Price: " << to_price(it->first) << "  Qty: " << total << std::endl;
    }
}

Price OrderBook::bestBid() const {
    return bids_.empty() ? 0 : bids_.begin()->first;
}

Price OrderBook::bestAsk() const {
    return asks_.empty() ? 0 : asks_.begin()->first;
}

}  // namespace ob
