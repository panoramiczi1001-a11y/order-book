#include "exchange.h"

namespace ob {

InsertResult Exchange::insertOrder(const Order& order) {
    auto it = books_.find(order.symbol());
    if (it == books_.end()) {
        it = books_.emplace(order.symbol(), OrderBook(order.symbol())).first;
    }
    return it->second.insertOrder(order);
}

bool Exchange::cancelOrder(const std::string& symbol, OrderId id) {
    auto it = books_.find(symbol);
    if (it == books_.end()) return false;
    return it->second.cancelOrder(id);
}

std::vector<Trade> Exchange::matchOrders(const std::string& symbol) {
    auto it = books_.find(symbol);
    if (it == books_.end()) return {};
    return it->second.matchOrders();
}

const OrderBook* Exchange::book(const std::string& symbol) const {
    auto it = books_.find(symbol);
    return it == books_.end() ? nullptr : &it->second;
}

}  // namespace ob
