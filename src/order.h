// order.h —— 订单、价格、成交记录
//
// v3 起价格一律用整数 tick 表示。理由是 double 做比较和做 map 的键都会出事：
// 0.1+0.2 != 0.3，同一个价位会被拆成两档。
#pragma once

#include <cstdint>
#include <string>

namespace ob {

// 价格的最小变动单位：1 tick = 0.0001 元（4 位小数）
inline constexpr std::int64_t kPriceScale = 10000;

using Price   = std::int64_t;   // 以 tick 为单位的价格
using Qty     = std::int64_t;   // 数量
using OrderId = std::int64_t;
using Seq     = std::int64_t;   // 到达序号，用于判断谁是先到的（成交价、时间优先）

enum class Side : char { Buy = 'B', Sell = 'S' };

// double 只保留给测试和演示用，内部一律换算成 tick
Price to_ticks(double price);
double to_price(Price ticks);

class Order {
public:
    Order(OrderId id, std::string symbol, Price price, Side side, Qty qty, Seq seq = 0);

    // 便捷构造：接受 double 价格，四舍五入到最近的 tick
    Order(OrderId id, std::string symbol, double price, char side, Qty qty);

    OrderId id() const { return id_; }
    const std::string& symbol() const { return symbol_; }
    Price price() const { return price_; }
    Side side() const { return side_; }
    Qty originalQty() const { return originalQty_; }
    Qty filledQty() const { return filledQty_; }
    Qty remaining() const { return originalQty_ - filledQty_; }
    Seq seq() const { return seq_; }
    bool isFilled() const { return remaining() <= 0; }

    void setSeq(Seq s) { seq_ = s; }
    void fill(Qty qty) { filledQty_ += qty; }

private:
    OrderId id_;
    std::string symbol_;
    Price price_;
    Side side_;
    Qty originalQty_;
    Qty filledQty_ = 0;
    Seq seq_ = 0;
};

// 一笔成交。价格取先到的一方（maker）的价格。
struct Trade {
    OrderId bidId;
    OrderId askId;
    Price price;
    Qty qty;
    Seq makerSeq;
};

// 下单结果。v3 起不再静默丢弃，调用方必须处理。
enum class InsertResult {
    Ok,
    BadId,          // id <= 0
    BadPrice,       // 价格非正，或 NaN/Inf（转换后无法落在 tick 上）
    BadQty,         // 数量非正
    BadSide,        // 方向既不是 B 也不是 S
    DuplicateId,    // 该 id 曾经出现过（含已成交/已撤销的），终身不可复用
};

const char* to_string(InsertResult r);

}  // namespace ob
