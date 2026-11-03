#include <iostream>
#include <string>
#include <deque>
#include <map>
#include <unordered_map>
#include <cstdlib>
#include <ctime>

using namespace std;

class Order {
public:
    Order(int orderId, string symbol, double price, char dir, int originalQuantity) 
    {
        this->orderId = orderId;
        this->symbol = symbol;
        this->price = price;
        this->dir = dir;
        this->originalQuantity = originalQuantity;
        this->filledQuantity = 0;
    }

    int remainingQuantity() const
    {
        return originalQuantity - filledQuantity;
    }

    bool isFilled() const 
    {
        return remainingQuantity() == 0;
    }

    int getOrderId() const { return orderId; }
    char getDir() const { return dir; }
    double getPrice() const { return price; }
    int getRemaining() const { return originalQuantity - filledQuantity; }
    int getFilledQuantity() const { return filledQuantity; }
    void setFilledQuantity(int filledQuantity) { this->filledQuantity = filledQuantity; }

private:
    int orderId; 
    string symbol; 
    double price;
    char dir;
    int originalQuantity; 
    int filledQuantity; 
};

class OrderBook {
public:
    void insertOrder(const Order& order)
    {
        if (order.getOrderId() <= 0) return;
        if (order.getPrice() <= 0) return;
        if (order.getRemaining() <= 0) return;
        if (order.getDir() != 'B' && order.getDir() != 'S') return;
        if (orderMap.count(order.getOrderId())) return;
        orderMap.insert({order.getOrderId(), order});
        if (order.getDir() == 'B') {
            buyOrders[order.getPrice()].push_back(order);
        } 
        else if (order.getDir() == 'S') {
            sellOrders[order.getPrice()].push_back(order);
        }
    };
    void cancelOrder(int orderId)
    {  
        auto it = orderMap.find(orderId);
        if (it != orderMap.end()) {
            Order& order = it->second;
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

    bool matchOrders()
    {
    bool matched = false;
    while (!buyOrders.empty() && !sellOrders.empty()) {
        double bestBuyPrice = buyOrders.begin()->first;
        double bestSellPrice = sellOrders.begin()->first;

        if (bestBuyPrice < bestSellPrice) {
            break;
        }

        Order& buyOrder = buyOrders.begin()->second.front();
        Order& sellOrder = sellOrders.begin()->second.front();
        int quantity = min(buyOrder.getRemaining(), sellOrder.getRemaining());
        buyOrder.setFilledQuantity(buyOrder.getFilledQuantity() + quantity);
        sellOrder.setFilledQuantity(sellOrder.getFilledQuantity() + quantity);
        orderMap.insert_or_assign(buyOrder.getOrderId(), buyOrder);
        orderMap.insert_or_assign(sellOrder.getOrderId(), sellOrder);
        matched = true;

        if (buyOrder.isFilled()) {
            int buyId = buyOrder.getOrderId();
            buyOrders.begin()->second.pop_front();
            orderMap.erase(buyId);
            if (buyOrders.begin()->second.empty()) {
                buyOrders.erase(buyOrders.begin());
            }
        }

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
    map<double, deque<Order>, greater<double>> buyOrders;
    map<double, deque<Order>, less<double>> sellOrders;
    unordered_map<int, Order> orderMap;
};

int main() {
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
        cout << "Matched this round: " << boolalpha << matched << endl;

        book.displayTop();
        cout << endl;
    }

    return 0;
}