// test_v3.cpp —— v3 新增行为的定点测试
//
// 覆盖 v0~v2 的六个已知缺陷中可以直接断言的部分：
//   1. 多标的不会互相成交
//   2. 订单号终身不复用
//   3. NaN / 非正价格被拒
//   4. 撮合产出成交记录
//   5. 撤单后序列号同样不可复用
#include <cmath>
#include <iostream>
#include <string>

#include "exchange.h"

using namespace ob;

static int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::cout << "  FAIL: " #cond " (line " << __LINE__ << ")\n";    \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

int main() {
    // ---------------------------------------------------------------- 1
    std::cout << "1. 多标的隔离\n";
    {
        Exchange ex;
        ex.insertOrder(Order(1, "600000", 100.0, 'B', 100));
        ex.insertOrder(Order(2, "000001", 100.0, 'S', 100));   // 另一个标的

        auto t1 = ex.matchOrders("600000");
        auto t2 = ex.matchOrders("000001");
        CHECK(t1.empty());                       // 不该成交
        CHECK(t2.empty());
        CHECK(ex.book("600000")->liveOrders() == 1);
        CHECK(ex.book("000001")->liveOrders() == 1);
        CHECK(ex.symbolCount() == 2);
    }

    // ---------------------------------------------------------------- 2
    std::cout << "2. 订单号终身不复用\n";
    {
        Exchange ex;
        CHECK(ex.insertOrder(Order(7, "A", 100.0, 'B', 100)) == InsertResult::Ok);
        CHECK(ex.insertOrder(Order(7, "A", 100.0, 'B', 999)) == InsertResult::DuplicateId);

        ex.insertOrder(Order(8, "A", 100.0, 'S', 100));
        ex.matchOrders("A");                     // 7 和 8 全部成交并被摘掉

        CHECK(ex.book("A")->liveOrders() == 0);
        CHECK(ex.insertOrder(Order(7, "A", 99.0, 'B', 50)) == InsertResult::DuplicateId);
    }

    // ---------------------------------------------------------------- 3
    std::cout << "3. 非法输入被拒\n";
    {
        OrderBook bk("A");
        const double nan_price = std::nan("");
        CHECK(bk.insertOrder(Order(1, "A", nan_price, 'B', 10)) == InsertResult::BadPrice);
        CHECK(bk.insertOrder(Order(2, "A", -1.0, 'B', 10)) == InsertResult::BadPrice);
        CHECK(bk.insertOrder(Order(3, "A", 100.0, 'B', 0)) == InsertResult::BadQty);
        CHECK(bk.insertOrder(Order(0, "A", 100.0, 'B', 10)) == InsertResult::BadId);
        CHECK(bk.liveOrders() == 0);
    }

    // ---------------------------------------------------------------- 4
    std::cout << "4. 成交记录\n";
    {
        OrderBook bk("A");
        bk.insertOrder(Order(1, "A", 100.0, 'B', 100));   // 先到，是 maker
        bk.insertOrder(Order(2, "A", 99.0, 'S', 40));     // 后到，是 taker

        auto trades = bk.matchOrders();
        CHECK(trades.size() == 1);
        CHECK(trades[0].qty == 40);
        CHECK(trades[0].price == to_ticks(100.0));        // 成交在 maker 的价格
        CHECK(trades[0].bidId == 1);
        CHECK(trades[0].askId == 2);
        CHECK(bk.liveOrders() == 1);                      // 买单还剩 60
    }

    // ---------------------------------------------------------------- 5
    std::cout << "5. 撤单后订单号仍不可复用\n";
    {
        OrderBook bk("A");
        CHECK(bk.insertOrder(Order(5, "A", 100.0, 'B', 10)) == InsertResult::Ok);
        CHECK(bk.cancelOrder(5));
        CHECK(!bk.cancelOrder(5));                        // 重复撤单返回 false
        CHECK(!bk.cancelOrder(999));                      // 不存在的单返回 false
        CHECK(bk.insertOrder(Order(5, "A", 100.0, 'B', 10)) == InsertResult::DuplicateId);
    }

    // ---------------------------------------------------------------- 6
    std::cout << "6. 同价位时间优先\n";
    {
        OrderBook bk("A");
        bk.insertOrder(Order(1, "A", 100.0, 'B', 10));
        bk.insertOrder(Order(2, "A", 100.0, 'B', 10));
        bk.insertOrder(Order(3, "A", 99.0, 'S', 10));

        auto trades = bk.matchOrders();
        CHECK(trades.size() == 1);
        CHECK(trades[0].bidId == 1);                      // 先到的 1 号先成交
        CHECK(bk.liveOrders() == 1);                      // 1 号与 3 号全部成交，只剩 2 号买单
    }

    if (failures == 0) {
        std::cout << "\nv3 行为测试全部通过\n";
        return 0;
    }
    std::cout << "\n失败 " << failures << " 项\n";
    return 1;
}
