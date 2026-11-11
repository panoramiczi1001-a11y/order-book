// test_parity.cpp —— 随机对拍：把 OrderBook 的结果跟一份朴素参考实现逐档比对
//
// 目的不是「测出正确答案」，而是「把现有行为冻住」。
// 重构（换成单一数据源、换容器、换价格类型）之后跑一遍，输出必须与重构前一致，
// 否则说明行为被无意改动了。
//
// 参考实现故意写得又笨又直白：用 vector 存所有订单，每次撮合都重新扫一遍找最优价。
// 它慢，但一眼能看懂，适合当标准答案。
//
// 用法：test_parity [轮数] [随机种子]
// 返回 0 = 通过，非 0 = 不一致（会打印第一个不一致的位置）
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "order_book.h"

using namespace ob;

namespace {

// ---------------------------------------------------------------- 参考实现
struct RefOrder {
    int id;
    char side;
    double price;
    int rem;
    long seq;   // 到达顺序，用于同价位的先到先撮
};

class RefBook {
public:
    void insert(int id, char side, double price, int qty)
    {
        if (id <= 0 || price <= 0 || qty <= 0) return;
        if (side != 'B' && side != 'S') return;
        if (find(id) != nullptr) return;
        store.push_back({id, side, price, qty, seqCounter++});
    }

    void cancel(int id)
    {
        store.erase(std::remove_if(store.begin(), store.end(),
                                   [id](const RefOrder& o) { return o.id == id; }),
                    store.end());
    }

    void match()
    {
        for (;;) {
            RefOrder* bestBuy = best('B');
            RefOrder* bestSell = best('S');
            if (!bestBuy || !bestSell) return;
            if (bestBuy->price < bestSell->price) return;

            // 先把 id 记下来再改数量：cancel() 会从 vector 里删除元素，
            // 之前取到的指针会立刻失效。
            const int buyId = bestBuy->id;
            const int sellId = bestSell->id;
            const int q = std::min(bestBuy->rem, bestSell->rem);

            find(buyId)->rem -= q;
            find(sellId)->rem -= q;
            if (find(buyId)->rem == 0) cancel(buyId);
            if (find(sellId)->rem == 0) cancel(sellId);
        }
    }

    // 与 OrderBook::displayTop 完全相同的输出格式
    std::string render() const
    {
        std::ostringstream os;
        os << "Top five buy orders:\n";
        int n = 0;
        for (auto& kv : levels('B', true)) {
            if (n++ >= 5) break;
            os << "Price: " << kv.first << "  Qty: " << kv.second << "\n";
        }
        os << "Top five sell orders:\n";
        n = 0;
        for (auto& kv : levels('S', false)) {
            if (n++ >= 5) break;
            os << "Price: " << kv.first << "  Qty: " << kv.second << "\n";
        }
        return os.str();
    }

private:
    RefOrder* find(int id)
    {
        for (auto& o : store) if (o.id == id) return &o;
        return nullptr;
    }

    RefOrder* best(char side)
    {
        RefOrder* r = nullptr;
        for (auto& o : store) {
            if (o.side != side || o.rem <= 0) continue;
            if (!r) { r = &o; continue; }
            if (side == 'B') {
                if (o.price > r->price || (o.price == r->price && o.seq < r->seq)) r = &o;
            } else {
                if (o.price < r->price || (o.price == r->price && o.seq < r->seq)) r = &o;
            }
        }
        return r;
    }

    std::vector<std::pair<double, int>> levels(char side, bool descending) const
    {
        std::map<double, int> m;
        for (auto& o : store) {
            if (o.side == side && o.rem > 0) m[o.price] += o.rem;
        }
        std::vector<std::pair<double, int>> v(m.begin(), m.end());
        if (descending) std::reverse(v.begin(), v.end());
        return v;
    }

    std::vector<RefOrder> store;
    long seqCounter = 0;
};

// ---------------------------------------------------------------- 抓盘口输出
std::string render(const OrderBook& book)
{
    std::ostringstream captured;
    std::streambuf* old = std::cout.rdbuf(captured.rdbuf());
    book.displayTop();
    std::cout.rdbuf(old);
    return captured.str();
}

}  // namespace

int main(int argc, char** argv)
{
    const int rounds = (argc > 1) ? std::atoi(argv[1]) : 500;
    const unsigned seed = (argc > 2) ? static_cast<unsigned>(std::atoi(argv[2])) : 20261103u;

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> priceDist(95, 105);   // 先用整数价格，避免 double 干扰
    std::uniform_int_distribution<int> qtyDist(1, 100);
    std::uniform_int_distribution<int> opDist(0, 99);

    OrderBook impl("A");
    RefBook ref;
    int nextId = 1;
    int checked = 0;

    for (int i = 0; i < rounds; ++i) {
        const int pick = opDist(rng);

        if (pick < 45) {                       // 45% 下单买
            const int id = nextId++;
            const double price = priceDist(rng);
            const int qty = qtyDist(rng);
            impl.insertOrder(Order(id, "A", price, 'B', qty));
            ref.insert(id, 'B', price, qty);
        } else if (pick < 85) {                // 40% 下单卖
            const int id = nextId++;
            const double price = priceDist(rng);
            const int qty = qtyDist(rng);
            impl.insertOrder(Order(id, "A", price, 'S', qty));
            ref.insert(id, 'S', price, qty);
        } else if (pick < 95 && nextId > 1) {  // 10% 撤单
            std::uniform_int_distribution<int> idDist(1, nextId - 1);
            const int id = idDist(rng);
            impl.cancelOrder(id);
            ref.cancel(id);
        } else {                               // 5% 撮合
            impl.matchOrders();
            ref.match();
        }

        const std::string got = render(impl);
        const std::string want = ref.render();
        ++checked;
        if (got != want) {
            std::cout << "第 " << (i + 1) << " 步不一致（随机种子 " << seed << "）\n";
            std::cout << "--- 实现 ---\n" << got;
            std::cout << "--- 参考 ---\n" << want;
            return 1;
        }
    }

    std::cout << "通过：" << checked << " 步全部一致（种子 " << seed << "）\n";
    return 0;
}
