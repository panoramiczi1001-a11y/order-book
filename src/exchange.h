// exchange.h —— 按标的隔离的多本订单簿
//
// v0~v2 只有一个全局买卖簿，symbol 字段形同虚设：不同标的的单子会互相成交。
// v3 起由 Exchange 按 symbol 路由，每个标的独立一本簿子、独立序号。
#pragma once

#include <map>
#include <string>
#include <vector>

#include "order_book.h"

namespace ob {

class Exchange {
public:
    // 按 order.symbol() 路由；标的第一次出现时自动建簿
    InsertResult insertOrder(const Order& order);

    bool cancelOrder(const std::string& symbol, OrderId id);

    std::vector<Trade> matchOrders(const std::string& symbol);

    const OrderBook* book(const std::string& symbol) const;
    std::size_t symbolCount() const { return books_.size(); }

private:
    std::map<std::string, OrderBook> books_;
};

}  // namespace ob
