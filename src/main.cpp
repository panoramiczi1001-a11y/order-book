// main.cpp —— 演示程序：随机造单，跑 50 轮
//
// 每轮固定生成一买一卖：价格 95~105 的整数，数量 10~100。
// 覆盖面很窄：
//   - 价格永远是整数，测不出 double 键的问题
//   - 每轮必有一买一卖，只撮合一次
//   - cancelOrder() 从头到尾没被调用过
// TODO(v3): 这份 demo 应该退出 src/，换成 tests/ 下的固定序列用例。
#include <cstdlib>
#include <ctime>
#include <iostream>

#include "order_book.h"

int main() {
    // TODO(v3): rand() + time 种子结果不可复现，应固定种子或写死序列。
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    OrderBook book;

    int nextOrderId = 1;

    for (int round = 1; round <= 50; ++round)
    {
        std::cout << "Round " << round << std::endl;

        int buyId = nextOrderId++;
        double buyPrice = 95.0 + (std::rand() % 11);
        int buyQty = 10 + (std::rand() % 91);
        Order buyOrder(buyId, "A", buyPrice, 'B', buyQty);

        int sellId = nextOrderId++;
        double sellPrice = 95.0 + (std::rand() % 11);
        int sellQty = 10 + (std::rand() % 91);
        Order sellOrder(sellId, "A", sellPrice, 'S', sellQty);

        std::cout << "Generated Buy id=" << buyId
                  << " price=" << buyPrice
                  << " qty=" << buyQty << std::endl;

        std::cout << "Generated Sell id=" << sellId
                  << " price=" << sellPrice
                  << " qty=" << sellQty << std::endl;

        book.insertOrder(buyOrder);
        book.insertOrder(sellOrder);

        bool matched = book.matchOrders();
        std::cout << "Matched this round: " << std::boolalpha << matched << std::endl;

        book.displayTop();
        std::cout << std::endl;
    }

    return 0;
}
