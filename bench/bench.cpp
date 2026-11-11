// bench.cpp —— 订单簿基准测试
//
// 两个口径，各回答一个问题：
//   分位数（单次计时）：单笔操作有多慢？尾部有多长？—— 但会掺入计时器开销
//   吞吐（批处理）    ：整体能跑多快？—— 开销被摊薄，但没有尾部分布
//
// 所以两个都测，并把计时器开销单独打出来，让读者自己判断可信度。
//
// 用法：
//   ./order_book_bench <mode> <ops> [seed]
//   mode = insert | cancel | mixed | throughput
//
//   insert      只放单（价格随机，不刻意交叉，簿子会变深）
//   cancel      先放 ops 笔单，再随机撤一半
//   mixed       70% 放单 / 25% 撤单 / 5% 交叉成交
//   throughput  批处理测每秒操作数（不逐笔计时）
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#include "exchange.h"

using namespace ob;
using Clock = std::chrono::steady_clock;

namespace {

inline std::uint64_t now_ns() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now().time_since_epoch())
            .count());
}

double percentile(const std::vector<std::uint64_t>& sorted, double p) {
    if (sorted.empty()) return 0.0;
    std::size_t idx = static_cast<std::size_t>(p * (sorted.size() - 1));
    if (idx >= sorted.size()) idx = sorted.size() - 1;
    return static_cast<double>(sorted[idx]);
}

// 计时器自身开销：连续取两次时间的差值
double timer_overhead_ns(int iters) {
    double sum = 0.0;
    for (int i = 0; i < iters; ++i) {
        std::uint64_t a = now_ns();
        std::uint64_t b = now_ns();
        sum += static_cast<double>(b - a);
    }
    return sum / iters;
}

struct Report {
    std::vector<std::uint64_t> samples;

    void add(std::uint64_t ns) { samples.push_back(ns); }

    void dump(const char* path, const char* label) const {
        if (!path) return;
        FILE* f = std::fopen(path, "a");
        if (!f) return;
        for (auto v : samples) std::fprintf(f, "%s,%llu\n", label, static_cast<unsigned long long>(v));
        std::fclose(f);
    }

    void print(const char* label) const {
        if (samples.empty()) {
            std::printf("%-28s (no samples)\n", label);
            return;
        }
        std::vector<std::uint64_t> s = samples;
        std::sort(s.begin(), s.end());

        double sum = 0;
        for (auto v : s) sum += static_cast<double>(v);

        std::printf("%-28s n=%7zu  p50=%6.0f  p90=%7.0f  p99=%8.0f  p999=%9.0f  max=%10.0f  mean=%6.1f  (ns)\n",
                    label, s.size(),
                    percentile(s, 0.50), percentile(s, 0.90),
                    percentile(s, 0.99), percentile(s, 0.999),
                    static_cast<double>(s.back()), sum / s.size());
    }
};

constexpr Price kMid = 100 * kPriceScale;     // 100.00

}  // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "mixed";
    const int ops = argc > 2 ? std::atoi(argv[2]) : 200000;
    const unsigned seed = argc > 3 ? static_cast<unsigned>(std::atoi(argv[3])) : 42u;
    const char* dumpPath = argc > 4 ? argv[4] : nullptr;   // 可选：把逐笔样本写到 CSV

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int>  priceOff(-50, 50);   // ±0.0050 元
    std::uniform_int_distribution<Qty>  qtyDist(1, 100);
    std::uniform_int_distribution<int>  coin(0, 99);

    Exchange ex;
    OrderId nextId = 1;
    std::vector<OrderId> live;
    live.reserve(static_cast<std::size_t>(ops));

    const double t_ovh = timer_overhead_ns(20000);
    std::printf("mode=%s ops=%d seed=%u\n", mode.c_str(), ops, seed);
    std::printf("timer_overhead=%.1f ns   （单次计时会掺入这个量级，分位数仅供参考）\n", t_ovh);

    if (mode == "throughput") {
        // 批处理：一次性放 ops 笔单，只计总时间
        const std::uint64_t t0 = now_ns();
        for (int i = 0; i < ops; ++i) {
            const Price px = kMid + priceOff(rng);
            const Side side = (coin(rng) < 50) ? Side::Buy : Side::Sell;
            ex.insertOrder(Order(nextId++, "BENCH", px, side, qtyDist(rng)));
        }
        const std::uint64_t t1 = now_ns();
        const double ns_per_op = static_cast<double>(t1 - t0) / ops;
        std::printf("%-28s %d ops in %.3f ms  = %.1f ns/op  = %.2f M ops/s\n",
                    "insert throughput", ops, (t1 - t0) / 1e6,
                    ns_per_op, 1e3 / ns_per_op);
        return 0;
    }

    Report ins, canc, mat;

    for (int i = 0; i < ops; ++i) {
        const int r = coin(rng);

        if (mode == "insert" || (mode == "mixed" && r < 70) || (mode == "cancel" && i < ops / 2)) {
            const Price px = kMid + priceOff(rng);
            const Side side = (coin(rng) < 50) ? Side::Buy : Side::Sell;
            const OrderId id = nextId++;

            const std::uint64_t t0 = now_ns();
            const InsertResult res = ex.insertOrder(Order(id, "BENCH", px, side, qtyDist(rng)));
            const std::uint64_t t1 = now_ns();

            if (res == InsertResult::Ok) {
                ins.add(t1 - t0);
                live.push_back(id);
            }
        } else if (mode == "cancel" || (mode == "mixed" && r < 95)) {
            if (live.empty()) continue;
            std::uniform_int_distribution<std::size_t> pick(0, live.size() - 1);
            const std::size_t idx = pick(rng);
            const OrderId id = live[idx];

            const std::uint64_t t0 = now_ns();
            const bool ok = ex.cancelOrder("BENCH", id);
            const std::uint64_t t1 = now_ns();
            if (ok) {
                canc.add(t1 - t0);
                live[idx] = live.back();
                live.pop_back();
            }
        } else {
            // 交叉单：故意穿价，触发撮合
            const OrderBook* bk = ex.book("BENCH");
            const Price bestAsk = bk ? bk->bestAsk() : 0;
            const Price bestBid = bk ? bk->bestBid() : 0;
            const Price px = (coin(rng) < 50)
                                 ? (bestAsk ? bestAsk : kMid)
                                 : (bestBid ? bestBid : kMid);
            const Side side = (coin(rng) < 50) ? Side::Buy : Side::Sell;

            const std::uint64_t t0 = now_ns();
            ex.insertOrder(Order(nextId++, "BENCH", px, side, qtyDist(rng)));
            ex.matchOrders("BENCH");
            const std::uint64_t t1 = now_ns();
            mat.add(t1 - t0);
        }
    }

    std::printf("live_orders=%zu\n", live.size());
    ins.dump(dumpPath, "insert");
    canc.dump(dumpPath, "cancel");
    mat.dump(dumpPath, "match");
    if (dumpPath) std::printf("samples -> %s\n", dumpPath);
    if (!ins.samples.empty())  ins.print("insert");
    if (!canc.samples.empty()) canc.print("cancel");
    if (!mat.samples.empty())  mat.print("cross+match");
    return 0;
}
