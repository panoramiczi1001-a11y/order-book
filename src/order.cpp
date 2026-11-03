#include "order.h"

Order::Order(int orderId, std::string symbol, double price, char dir, int originalQuantity)
{
    this->orderId = orderId;
    this->symbol = symbol;   // 纯拷贝，v2 不做任何行为改动（move 留给 v3）
    this->price = price;
    this->dir = dir;
    this->originalQuantity = originalQuantity;
    this->filledQuantity = 0;
}

int Order::remainingQuantity() const
{
    return originalQuantity - filledQuantity;
}

// 是否已全部成交。
// TODO(v3): 只判断 == 0，若 filledQuantity 被写成超过 originalQuantity，
//           剩余量会变负数而这里判不出来。
bool Order::isFilled() const
{
    return remainingQuantity() == 0;
}
