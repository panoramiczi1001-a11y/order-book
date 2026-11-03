#include "order_book.h"

#include <algorithm>
#include <iostream>

// ---------------------------------------------------------------------------
// 下单
// ---------------------------------------------------------------------------
void OrderBook::insertOrder(const Order& order)
{
    // 五行校验，任何一条不过就静默丢弃。
    // TODO(v3): 静默 return 导致外部无法知道单子为什么没进去，
    //           应返回错误码或布尔值。注意 NaN 能绕过 price <= 0。
    if (order.getOrderId() <= 0) return;
    if (order.getPrice() <= 0) return;
    if (order.getRemaining() <= 0) return;
    if (order.getDir() != 'B' && order.getDir() != 'S') return;
    // TODO(v3): count() 之后再 insert() 是两次哈希查找，改用 insert 的返回值。
    if (orderMap.count(order.getOrderId())) return;

    // TODO(v3): 这里是整份拷贝，含 std::string symbol，应 move。
    orderMap.insert({order.getOrderId(), order});
    if (order.getDir() == 'B') {
        buyOrders[order.getPrice()].push_back(order);
    }
    else if (order.getDir() == 'S') {
        sellOrders[order.getPrice()].push_back(order);
    }
}

// ---------------------------------------------------------------------------
// 撤单
// ---------------------------------------------------------------------------
void OrderBook::cancelOrder(int orderId)
{
    auto it = orderMap.find(orderId);
    if (it == orderMap.end()) return;

    Order& order = it->second;
    // TODO(v3): 两个分支除容器外完全一样，可合成一个 helper；
    //           且这里的线性扫描是 O(n)，应存迭代器做到 O(1)。
    if (order.getDir() == 'B')
    {
        // TODO(v3): 用 operator[] 会在键不存在时凭空插入空档位，应改用 find()。
        auto& level = buyOrders[order.getPrice()];
        for (auto jt = level.begin(); jt != level.end(); ++jt) {
            if (jt->getOrderId() == orderId) {
                level.erase(jt);
                break;
            }
        }
    }
    else if (order.getDir() == 'S')
    {
        auto& level = sellOrders[order.getPrice()];
        for (auto jt = level.begin(); jt != level.end(); ++jt) {
            if (jt->getOrderId() == orderId) {
                level.erase(jt);
                break;
            }
        }
    }

    // 价位空了就把整档删掉，否则盘口会打出空档、撮合也会看到空队首。
    if (order.getDir() == 'B' && buyOrders[order.getPrice()].empty())
    {
        buyOrders.erase(order.getPrice());
    }
    else if (order.getDir() == 'S' && sellOrders[order.getPrice()].empty())
    {
        sellOrders.erase(order.getPrice());
    }
    orderMap.erase(it);
}

// ---------------------------------------------------------------------------
// 撮合
// ---------------------------------------------------------------------------
bool OrderBook::matchOrders()
{
    bool matched = false;
    while (!buyOrders.empty() && !sellOrders.empty()) {
        double bestBuyPrice = buyOrders.begin()->first;
        double bestSellPrice = sellOrders.begin()->first;

        // 买价低于卖价，没有交叉，收工。
        if (bestBuyPrice < bestSellPrice) {
            break;
        }

        // 两个引用指向 deque 内部元素，后面 pop_front 之后会失效。
        // 当前靠语句顺序侥幸没用到失效的引用。
        // TODO(v3): 先取 orderId 和剩余量再操作，别持有引用跨过 pop。
        Order& buyOrder = buyOrders.begin()->second.front();
        Order& sellOrder = sellOrders.begin()->second.front();
        int quantity = std::min(buyOrder.getRemaining(), sellOrder.getRemaining());
        buyOrder.setFilledQuantity(buyOrder.getFilledQuantity() + quantity);
        sellOrder.setFilledQuantity(sellOrder.getFilledQuantity() + quantity);

        // 改了 deque 里那两份拷贝，必须写回索引那份，否则三处状态分裂。
        // TODO(v3): 单一数据源之后这两行就该消失。
        orderMap.insert_or_assign(buyOrder.getOrderId(), buyOrder);
        orderMap.insert_or_assign(sellOrder.getOrderId(), sellOrder);
        matched = true;

        if (buyOrder.isFilled()) {
            int buyId = buyOrder.getOrderId();
            buyOrders.begin()->second.pop_front();
            orderMap.erase(buyId);
            if (buyOrders.begin()->second.empty()) {
                buyOrders.erase(buyOrders.begin());
            }
        }

        if (sellOrder.isFilled()) {
            int sellId = sellOrder.getOrderId();
            sellOrders.begin()->second.pop_front();
            orderMap.erase(sellId);
            if (sellOrders.begin()->second.empty()) {
                sellOrders.erase(sellOrders.begin());
            }
        }
    }
    return matched;
}

// ---------------------------------------------------------------------------
// 盘口
// ---------------------------------------------------------------------------
void OrderBook::displayTop() const
{
    std::cout << "Top five buy orders:" << std::endl;
    int count = 0;
    // TODO(v4): 每次打印都遍历整档求和，应增量维护每档总量。
    for (auto it = buyOrders.begin(); it != buyOrders.end() && count < 5; ++it, ++count) {
        int totalQty = 0;
        for (const auto& order : it->second) {
            totalQty += order.getRemaining();
        }
        std::cout << "Price: " << it->first << "  Qty: " << totalQty << std::endl;
    }

    std::cout << "Top five sell orders:" << std::endl;
    count = 0;
    for (auto it = sellOrders.begin(); it != sellOrders.end() && count < 5; ++it, ++count) {
        int totalQty = 0;
        for (const auto& order : it->second) {
            totalQty += order.getRemaining();
        }
        std::cout << "Price: " << it->first << "  Qty: " << totalQty << std::endl;
    }
    // TODO(v3): 没有成交记录输出，外部无法核对撮合结果。
}
