// main.cpp —— 演示：两个标的、随机下单、撮合、打印成交与盘口
#include <cstdlib>
#include <ctime>
#include <iostream>

#include "exchange.h"

using namespace ob;

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    Exchange ex;
    OrderId nextId = 1;

    const char* symbols[] = {"600000", "000001"};

    for (int round = 1; round <= 20; ++round) {
        std::cout << "=== Round " << round << " ===\n";

        for (const char* sym : symbols) {
            // 两个标的价格区间不同，若串账会立刻看出成交价异常
            const double base = (sym[0] == '6') ? 100.0 : 20.0;

            const OrderId buyId = nextId++;
            const double buyPrice = base + (std::rand() % 5) * 0.01;
            const Qty buyQty = 10 + std::rand() % 90;
            ex.insertOrder(Order(buyId, sym, buyPrice, 'B', buyQty));

            const OrderId sellId = nextId++;
            const double sellPrice = base + (std::rand() % 5) * 0.01;
            const Qty sellQty = 10 + std::rand() % 90;
            ex.insertOrder(Order(sellId, sym, sellPrice, 'S', sellQty));

            const auto trades = ex.matchOrders(sym);
            for (const Trade& t : trades) {
                std::cout << "  [" << sym << "] trade " << t.qty << " @ "
                          << to_price(t.price) << "  bid#" << t.bidId
                          << " ask#" << t.askId << "\n";
            }
        }
    }

    std::cout << "\n--- 收盘盘口 ---\n";
    for (const char* sym : symbols) {
        const OrderBook* b = ex.book(sym);
        std::cout << "[" << sym << "]\n";
        if (b) b->displayTop();
    }

    return 0;
}
