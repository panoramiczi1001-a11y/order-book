// order.h —— 一笔限价单的数据与状态
//
// 状态只有两个：originalQuantity（下单量）和 filledQuantity（已成交量）。
// 剩余量 = 两者相减，不单独存，避免出现第三份可分裂的状态。
#pragma once

#include <string>

class Order {
public:
    // TODO(v3): 应改用初始化列表，现在的写法是先默认构造再赋值，
    //           对 std::string 是多一次构造。
    Order(int orderId, std::string symbol, double price, char dir, int originalQuantity);

    int remainingQuantity() const;
    bool isFilled() const;

    int getOrderId() const { return orderId; }
    char getDir() const { return dir; }
    double getPrice() const { return price; }
    // TODO(v3): 与 remainingQuantity() 重复，统一成一个。
    int getRemaining() const { return originalQuantity - filledQuantity; }
    int getFilledQuantity() const { return filledQuantity; }
    void setFilledQuantity(int filledQuantity) { this->filledQuantity = filledQuantity; }

private:
    int orderId;
    std::string symbol;
    // TODO(v3): 价格用 double 存，而 double 又被当作 map 的键（见 OrderBook）。
    //           浮点相等判断会静默丢单，应改成整数 tick。
    double price;
    char dir;              // 'B' 买 / 'S' 卖
    int originalQuantity;
    int filledQuantity;
};
