#include <iostream>
#include <vector>
#include <algorithm>
#include <limits>
#include <cassert>
#include <chrono>
#include <list>
#include <map>

using namespace std;
using namespace std::chrono;

enum class Side {
    BUY,
    SELL
};

struct Order {
    int ID;
    Side side;
    int price;
    int quantity;
    long long arrival_sequence;
};

struct Trade {
    int buyOrderID;
    int sellOrderID;
    int price;
    int quantity;
};

// Map-based order book: best price first, FIFO within each price level.
map<int, list<Order>, greater<int>> buy_orders;
map<int, list<Order>> sell_orders;

// Vector-based order functions retained from the original interactive version.
bool add_order(vector<Order>& orders, int ID, Side side,
               int price, int quantity, long long& next_sequence) {
    if (ID <= 0 || price <= 0 || quantity <= 0) {
        cout << "Invalid order parameters.\n";
        return false;
    }

    for (const auto& order : orders) {
        if (order.ID == ID) {
            cout << "Order ID already exists.\n";
            return false;
        }
    }

    orders.push_back({ID, side, price, quantity, next_sequence++});
    cout << "Order added successfully.\n";
    return true;
}

void sort_orders(vector<Order>& orders) {
    sort(orders.begin(), orders.end(), [](const Order& a, const Order& b) {
        if (a.side != b.side) {
            return a.side == Side::BUY;
        }
        if (a.price != b.price) {
            if (a.side == Side::BUY) return a.price > b.price;
            return a.price < b.price;
        }
        return a.arrival_sequence < b.arrival_sequence;
    });
}

void add_order_to_map(Order order) {
    if (order.side == Side::BUY) {
        buy_orders[order.price].push_back(order);
    } else {
        sell_orders[order.price].push_back(order);
    }
}

bool cancel_order(vector<Order>& orders, int ID) {
    for (auto it = orders.begin(); it != orders.end(); ++it) {
        if (it->ID == ID) {
            orders.erase(it);
            return true;
        }
    }
    return false;
}

vector<Trade> matching_orders(vector<Order>& orders) {
    vector<Trade> trades;
    sort_orders(orders);

    while (true) {
        int buy_index = -1;
        int sell_index = -1;

        for (int i = 0; i < static_cast<int>(orders.size()); ++i) {
            if (orders[i].side == Side::BUY && buy_index == -1) buy_index = i;
            if (orders[i].side == Side::SELL && sell_index == -1) sell_index = i;
        }

        if (buy_index == -1 || sell_index == -1) break;
        if (orders[buy_index].price < orders[sell_index].price) break;

        int matched_quantity = min(orders[buy_index].quantity,
                                   orders[sell_index].quantity);
        int buy_id = orders[buy_index].ID;
        int sell_id = orders[sell_index].ID;
        int execution_price = orders[sell_index].price;

        trades.push_back({buy_id, sell_id, execution_price, matched_quantity});
        orders[buy_index].quantity -= matched_quantity;
        orders[sell_index].quantity -= matched_quantity;

        bool buy_filled = orders[buy_index].quantity == 0;
        bool sell_filled = orders[sell_index].quantity == 0;

        if (buy_filled && sell_filled) {
            int first = min(buy_index, sell_index);
            int second = max(buy_index, sell_index);
            orders.erase(orders.begin() + second);
            orders.erase(orders.begin() + first);
        } else if (buy_filled) {
            orders.erase(orders.begin() + buy_index);
        } else if (sell_filled) {
            orders.erase(orders.begin() + sell_index);
        }
    }

    return trades;
}

void display_orders(vector<Order>& orders) {
    sort_orders(orders);
    if (orders.empty()) {
        cout << "Order book is empty.\n";
        return;
    }

    cout << "\n--- Current Order Book ---\n";
    for (const auto& order : orders) {
        cout << "ID: " << order.ID
             << " | Side: " << (order.side == Side::BUY ? "BUY" : "SELL")
             << " | Price: " << order.price
             << " | Quantity: " << order.quantity << '\n';
    }
}

void display_trades(const vector<Trade>& trades) {
    if (trades.empty()) {
        cout << "No trades executed yet.\n";
        return;
    }

    cout << "\n--- Trade History ---\n";
    for (const auto& trade : trades) {
        cout << "Buy Order ID: " << trade.buyOrderID
             << " | Sell Order ID: " << trade.sellOrderID
             << " | Price: " << trade.price
             << " | Quantity: " << trade.quantity << '\n';
    }
    cout << "Total trades: " << trades.size() << '\n';
}

void benchmark_sorting() {
    vector<Order> orders;
    for (int i = 0; i < 10000; ++i) {
        Side side = (i % 2 == 0) ? Side::BUY : Side::SELL;
        orders.push_back({i + 1, side, 90 + (i % 20), 10, i});
    }

    auto start = high_resolution_clock::now();
    sort_orders(orders);
    auto end = high_resolution_clock::now();
    auto elapsed = duration_cast<microseconds>(end - start).count();

    cout << "Sorting 10,000 orders took " << elapsed << " microseconds.\n";
}

bool can_match() {
    if (buy_orders.empty() || sell_orders.empty()) return false;
    int best_buy = buy_orders.begin()->first;
    int best_sell = sell_orders.begin()->first;
    return best_buy >= best_sell;
}

bool match_one_trade(vector<Trade>& trades) {
    if (!can_match()) return false;

    auto buy_level = buy_orders.begin();
    auto sell_level = sell_orders.begin();
    Order& buy_order = buy_level->second.front();
    Order& sell_order = sell_level->second.front();

    int matched_quantity = min(buy_order.quantity, sell_order.quantity);
    int execution_price = sell_order.price;

    trades.push_back({buy_order.ID, sell_order.ID,
                      execution_price, matched_quantity});

    buy_order.quantity -= matched_quantity;
    sell_order.quantity -= matched_quantity;

    bool buy_filled = buy_order.quantity == 0;
    bool sell_filled = sell_order.quantity == 0;

    if (buy_filled) {
        buy_level->second.pop_front();
        if (buy_level->second.empty()) buy_orders.erase(buy_level);
    }
    if (sell_filled) {
        sell_level->second.pop_front();
        if (sell_level->second.empty()) sell_orders.erase(sell_level);
    }

    return true;
}

int match_all(vector<Trade>& trades) {
    int count = 0;
    while (match_one_trade(trades)) ++count;
    return count;
}

bool cancel_map_orders(int order_id) {
    for (auto level = buy_orders.begin(); level != buy_orders.end(); ++level) {
        auto& orders_at_price = level->second;
        for (auto order = orders_at_price.begin(); order != orders_at_price.end(); ++order) {
            if (order->ID == order_id) {
                orders_at_price.erase(order);
                if (orders_at_price.empty()) buy_orders.erase(level);
                return true;
            }
        }
    }

    for (auto level = sell_orders.begin(); level != sell_orders.end(); ++level) {
        auto& orders_at_price = level->second;
        for (auto order = orders_at_price.begin(); order != orders_at_price.end(); ++order) {
            if (order->ID == order_id) {
                orders_at_price.erase(order);
                if (orders_at_price.empty()) sell_orders.erase(level);
                return true;
            }
        }
    }

    return false;
}

int run_menu() {
    vector<Order> orders;
    vector<Trade> trade_history;
    int choice;
    long long next_sequence = 0;

    while (true) {
        cout << "\n===== Limit Order Book Simulator =====\n"
             << "1. Add order\n"
             << "2. Cancel order\n"
             << "3. Match orders\n"
             << "4. Display order book\n"
             << "5. Display trade history\n"
             << "6. Benchmark sorting 10,000 orders\n"
             << "0. Exit\n"
             << "Enter choice: ";

        if (!(cin >> choice)) {
            cout << "Invalid input. Exiting.\n";
            break;
        }

        switch (choice) {
            case 1: {
                int ID, price, quantity;
                char side;
                cout << "Enter Order ID: "; cin >> ID;
                cout << "Enter side (B = BUY, S = SELL): "; cin >> side;
                cout << "Enter price: "; cin >> price;
                cout << "Enter quantity: "; cin >> quantity;

                if (side == 'B' || side == 'b') {
                    add_order(orders, ID, Side::BUY, price, quantity, next_sequence);
                } else if (side == 'S' || side == 's') {
                    add_order(orders, ID, Side::SELL, price, quantity, next_sequence);
                } else {
                    cout << "Invalid side. Enter B or S.\n";
                }
                break;
            }
            case 2: {
                int ID;
                cout << "Enter Order ID to cancel: "; cin >> ID;
                cout << (cancel_order(orders, ID)
                         ? "Order cancelled successfully.\n"
                         : "Order not found.\n");
                break;
            }
            case 3: {
                vector<Trade> new_trades = matching_orders(orders);
                if (new_trades.empty()) {
                    cout << "No trades executed.\n";
                } else {
                    trade_history.insert(trade_history.end(),
                                         new_trades.begin(), new_trades.end());
                    cout << new_trades.size() << " trade(s) executed successfully.\n";
                }
                break;
            }
            case 4:
                display_orders(orders);
                break;
            case 5:
                display_trades(trade_history);
                break;
            case 6:
                benchmark_sorting();
                break;
            case 0:
                cout << "Exiting simulator. Goodbye!\n";
                return 0;
            default:
                cout << "Invalid choice. Try again.\n";
        }
    }

    return 0;
}

#ifndef LOB_TESTING
int main() {
    return run_menu();
}
#endif
