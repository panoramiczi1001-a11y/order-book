// ============================================================================
//  orders.cpp  —  v1：与 v0 逻辑完全一致，仅补注释
//
//  背景：这是 2026 年 9 月一笔试题的答卷。原始版本原样存放在 v0/orders.cpp，
//        不修改。本版一字未改逻辑，只把「这段在干什么」和「这段欠着什么账」
//        写清楚，方便后续逐个修复。
//
//  阅读顺序：
//      1. Order        —— 一笔订单的数据与状态
//      2. OrderBook    —— 买卖两本簿子 + 一个全量索引
//      3. main         —— 随机生成订单、撮合、打印盘口
//
//  已知问题都标成 TODO(v2)：，编号对应 README.md 的「v0 的问题清单」。
// ============================================================================

#include <iostream>
#include <string>
#include <deque>
#include <map>
#include <unordered_map>
#include <cstdlib>
#include <ctime>

// TODO(v2): 文件作用域 using namespace std 会在头文件被包含时污染全局，
//           应删掉，改用 std:: 前缀。
using namespace std;

// ----------------------------------------------------------------------------
// Order：一笔限价单。字段都放在 private，通过 getter 读。
//
// 状态只有两个：originalQuantity（下单量）和 filledQuantity（已成交量）。
// 剩余量 = 两者相减，不单独存，避免三处状态不同步。
// ----------------------------------------------------------------------------
class Order {
public:
    // TODO(v2): 应改用初始化列表（Order(...) : orderId(orderId), ...），
    //           现在的写法是先默认构造再赋值，对 std::string 是多一次构造。
    Order(int orderId, string symbol, double price, char dir, int originalQuantity)
    {
        this->orderId = orderId;
        this->symbol = symbol;
        this->price = price;
        this->dir = dir;
        this->originalQuantity = originalQuantity;
        this->filledQuantity = 0;
    }

    // 剩余未成交量。
    int remainingQuantity() const
    {
        return originalQuantity - filledQuantity;
    }

    // 是否已全部成交。注意：只判断 == 0，若 filledQuantity 被写成超过
    // originalQuantity，剩余量会变负数而这里判不出来。
    bool isFilled() const
    {
        return remainingQuantity() == 0;
    }

    int getOrderId() const { return orderId; }
    char getDir() const { return dir; }
    double getPrice() const { return price; }
    // TODO(v2): 与上面的 remainingQuantity() 重复，统一成一个即可。
    int getRemaining() const { return originalQuantity - filledQuantity; }
    int getFilledQuantity() const { return filledQuantity; }
    // 唯一的写入口：撮合时把成交量累加上去。
    void setFilledQuantity(int filledQuantity) { this->filledQuantity = filledQuantity; }

private:
    int orderId;
    string symbol;
    // TODO(v2): 价格用 double 存，而 double 又同时被当作 map 的键（见 OrderBook）。
    //           浮点相等判断会静默丢单，应改成整数 tick（价格 × 最小变动单位）。
    double price;
    char dir;              // 'B' 买 / 'S' 卖
    int originalQuantity;
    int filledQuantity;
};

// ----------------------------------------------------------------------------
// OrderBook：限价订单簿。
//
// 数据结构是三份，这是本版最大的结构问题：
//      buyOrders  价格 -> 该价位所有买单（按到达顺序排队，先到先撮）
//      sellOrders 价格 -> 该价位所有卖单
//      orderMap   订单号 -> 订单（用于 O(1) 找到它）
//
// 同一笔订单被存了三份拷贝，撮合时改了 deque 里那份，必须手动
// insert_or_assign 回 orderMap 才不至于分裂。
// TODO(v2): 收成一份——orderMap 里存指向簿子节点的句柄（迭代器或指针），
//           簿子才是唯一真相，索引只用来定位。
// ----------------------------------------------------------------------------
class OrderBook {
public:
    // ------------------------------------------------------------------
    // 下单：校验 -> 双索引登记。
    // 注意这里只入簿，不撮合；撮合由调用方显式调用 matchOrders()。
    // ------------------------------------------------------------------
    void insertOrder(const Order& order)
    {
        // 五行校验，任何一条不过就静默丢弃。
        // TODO(v2): 静默 return 导致外部无法知道单子为什么没进去，
        //           应返回错误码或布尔值，配合日志。
        if (order.getOrderId() <= 0) return;
        if (order.getPrice() <= 0) return;
        if (order.getRemaining() <= 0) return;
        if (order.getDir() != 'B' && order.getDir() != 'S') return;
        // TODO(v2): count() 之后再 insert() 是两次哈希查找，改用 insert 的返回值判断。
        if (orderMap.count(order.getOrderId())) return;

        // 第一份：全量索引。
        orderMap.insert({order.getOrderId(), order});
        // 第二份：按方向塞进对应价位的队列尾部（队尾 = 最新，撮合从队首取）。
        // TODO(v2): 这里是把 Order 整个拷贝一份，含 std::string symbol，应 move。
        if (order.getDir() == 'B') {
            buyOrders[order.getPrice()].push_back(order);
        }
        else if (order.getDir() == 'S') {
            sellOrders[order.getPrice()].push_back(order);
        }
    };

    // ------------------------------------------------------------------
    // 撤单：先从索引里找到订单，再去对应价位队列里线性扫描删掉。
    //
    // TODO(v2): 两个问题。
    //   1) 复杂度 O(n)：同价位单子越多越慢。应在 orderMap 里直接存迭代器，
    //      或用 std::list 让删除 O(1)。
    //   2) buyOrders[price] 用的是 operator[]，价位的键不存在时会凭空插入
    //      一个空档位，属于「用副作用补救」的写法，应改用 find()。
    // ------------------------------------------------------------------
    void cancelOrder(int orderId)
    {
        auto it = orderMap.find(orderId);
        if (it != orderMap.end()) {
            Order& order = it->second;
            // 按方向去对应簿子里扫。两个分支除了容器不同，逻辑完全一样，
            // TODO(v2): 可以合并成一个 helper。
            if (order.getDir() == 'B')
            {
                auto& deque = buyOrders[order.getPrice()];
                for (auto it = deque.begin(); it != deque.end(); ++it) {
                    if (it->getOrderId() == orderId) {
                        deque.erase(it);
                        break;
                    }
                }
            }
            else if (order.getDir() == 'S')
            {
                auto& deque = sellOrders[order.getPrice()];
                for (auto it = deque.begin(); it != deque.end(); ++it) {
                    if (it->getOrderId() == orderId) {
                        deque.erase(it);
                        break;
                    }
                }
            }
            // 删完之后，如果这个价位已经空了，把整个价位档去掉，
            // 否则 displayTop 会打出空档，撮合循环也会看到空的队首。
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
    };

    // ------------------------------------------------------------------
    // 撮合：价格优先 + 时间优先。
    //
    // buyOrders 用 greater 排序，begin() 是最高买价；
    // sellOrders 用 less，begin() 是最低卖价。
    // 只要最高买价 >= 最低卖价，就成交，成交量取两边剩余量的较小值。
    // ------------------------------------------------------------------
    bool matchOrders()
    {
    bool matched = false;
    while (!buyOrders.empty() && !sellOrders.empty()) {
        double bestBuyPrice = buyOrders.begin()->first;
        double bestSellPrice = sellOrders.begin()->first;

        // 买价低于卖价，没有可成交的交叉，收工。
        if (bestBuyPrice < bestSellPrice) {
            break;
        }

        // 取两边队首（最早到达）的单子。
        // 注意：这两个是引用，指向 deque 内部的元素，后面 pop_front 之后会失效，
        //       本版靠语句顺序侥幸没用到失效的引用。
        // TODO(v2): 改成先拷贝出 orderId 和剩余量再操作，别持有引用跨过 pop。
        Order& buyOrder = buyOrders.begin()->second.front();
        Order& sellOrder = sellOrders.begin()->second.front();
        int quantity = min(buyOrder.getRemaining(), sellOrder.getRemaining());
        buyOrder.setFilledQuantity(buyOrder.getFilledQuantity() + quantity);
        sellOrder.setFilledQuantity(sellOrder.getFilledQuantity() + quantity);
        // 改了 deque 里那两份拷贝，必须写回索引那份，否则三处状态分裂。
        // TODO(v2): 单一数据源之后这两行就该消失。
        orderMap.insert_or_assign(buyOrder.getOrderId(), buyOrder);
        orderMap.insert_or_assign(sellOrder.getOrderId(), sellOrder);
        matched = true;

        // 买单成交完：从队首摘掉、从索引里删掉，价位空了就把价位删掉。
        if (buyOrder.isFilled()) {
            int buyId = buyOrder.getOrderId();
            buyOrders.begin()->second.pop_front();
            orderMap.erase(buyId);
            if (buyOrders.begin()->second.empty()) {
                buyOrders.erase(buyOrders.begin());
            }
        }

        // 卖单同理。
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
    };

    // ------------------------------------------------------------------
    // 打印盘口前五档：价格 + 该价位的合计剩余量。
    //
    // TODO(v2): 两个问题。
    //   1) 每次打印都遍历整档所有单子求和，复杂度 O(档内单数)；
    //      应该在下单/撤单/成交时增量维护每档总量。
    //   2) 只打印不返回，没有成交记录，外部无法核对撮合的正确性。
    //      v2 应产出 trade 结构（价格、数量、双方 orderId）。
    // ------------------------------------------------------------------
    void displayTop() const
    {
        cout << "Top five buy orders:" << endl;
    int count = 0;
    for (auto it = buyOrders.begin(); it != buyOrders.end() && count < 5; ++it, ++count) {
        int totalQty = 0;
        for (const auto& order : it->second) {
            totalQty += order.getRemaining();
        }
        cout << "Price: " << it->first << "  Qty: " << totalQty << endl;
    }

    cout << "Top five sell orders:" << endl;
    count = 0;
    for (auto it = sellOrders.begin(); it != sellOrders.end() && count < 5; ++it, ++count) {
        int totalQty = 0;
        for (const auto& order : it->second) {
            totalQty += order.getRemaining();
        }
        cout << "Price: " << it->first << "  Qty: " << totalQty << endl;
    }
    };

private:
    // 买盘：价格从高到低（greater），begin() 是最高买价。
    // 卖盘：价格从低到高（less），begin() 是最低卖价。
    // TODO(v2): 键是 double，见 Order::price 处的说明。
    map<double, deque<Order>, greater<double>> buyOrders;
    map<double, deque<Order>, less<double>> sellOrders;
    // 全量索引：订单号 -> 订单副本（第三份数据）。
    unordered_map<int, Order> orderMap;
};

// ----------------------------------------------------------------------------
// main：随机造单，跑 50 轮。
//
// 每轮固定生成一买一卖：价格 95~105 的整数，数量 10~100。
// 这个测试的覆盖面非常窄：
//   - 价格永远是整数，测不出 double 键的问题
//   - 每轮必有一买一卖，且只撮合一次
//   - cancelOrder() 从头到尾没有被调用过
// TODO(v2): 测试搬到 tests/ 下，用固定序列覆盖边界：
//           价格相等、数量不整除、重复 id、撤单后再挂、多笔同价排队。
// ----------------------------------------------------------------------------
int main() {
    // TODO(v2): 用 rand() + time 种子，结果不可复现；应固定种子或直接用写死的序列。
    srand((unsigned)time(nullptr));
    OrderBook book;

    int nextOrderId = 1;

    for (int round = 1; round <= 50; ++round)
    {
        cout << "Round " << round << endl;

        int buyId = nextOrderId++;
        double buyPrice = 95.0 + (rand() % 11);
        int buyQty = 10 + (rand() % 91);
        Order buyOrder(buyId, "A", buyPrice, 'B', buyQty);

        int sellId = nextOrderId++;
        double sellPrice = 95.0 + (rand() % 11);
        int sellQty = 10 + (rand() % 91);
        Order sellOrder(sellId, "A", sellPrice, 'S', sellQty);

        cout << "Generated Buy id=" << buyId
             << " price=" << buyPrice
             << " qty=" << buyQty << endl;

        cout << "Generated Sell id=" << sellId
             << " price=" << sellPrice
             << " qty=" << sellQty << endl;


        book.insertOrder(buyOrder);
        book.insertOrder(sellOrder);

        bool matched = book.matchOrders();
        // 注意：botalpha 是粘性操纵符，第一次用完之后所有 bool 都会打印成 true/false。
        cout << "Matched this round: " << boolalpha << matched << endl;

        book.displayTop();
        cout << endl;
    }

    return 0;
}
