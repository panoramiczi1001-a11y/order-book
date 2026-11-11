#include "order.h"

#include <cmath>

namespace ob {

Price to_ticks(double price) {
    // NaN / Inf 转成整数是未定义行为，先在入口挡掉（返回 0，调用方判为非法）
    if (!std::isfinite(price)) return 0;
    const double scaled = price * static_cast<double>(kPriceScale);
    if (scaled > 9.0e18 || scaled < -9.0e18) return 0;   // 防溢出
    return static_cast<Price>(std::llround(scaled));
}

double to_price(Price ticks) {
    return static_cast<double>(ticks) / static_cast<double>(kPriceScale);
}

Order::Order(OrderId id, std::string symbol, Price price, Side side, Qty qty, Seq seq)
    : id_(id),
      symbol_(std::move(symbol)),
      price_(price),
      side_(side),
      originalQty_(qty),
      seq_(seq) {}

Order::Order(OrderId id, std::string symbol, double price, char side, Qty qty)
    : Order(id, std::move(symbol), to_ticks(price),
            side == 'B' ? Side::Buy : Side::Sell, qty) {}

const char* to_string(InsertResult r) {
    switch (r) {
        case InsertResult::Ok:          return "ok";
        case InsertResult::BadId:       return "bad id";
        case InsertResult::BadPrice:    return "bad price";
        case InsertResult::BadQty:      return "bad qty";
        case InsertResult::BadSide:     return "bad side";
        case InsertResult::DuplicateId: return "duplicate id";
    }
    return "unknown";
}

}  // namespace ob
