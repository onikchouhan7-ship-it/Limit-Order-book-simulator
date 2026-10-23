// Persistent JSON bridge for the existing C++ limit order book.
// Build on Windows with: g++ -std=c++17 -O2 bridge.cpp -o bridge.exe
#define LOB_TESTING
#include "main.cpp"

#include <sstream>
#include <string>
#include <set>

namespace {
    std::set<int> used_ids;
    std::vector<Trade> bridge_trades;
    long long next_sequence = 1;

    void print_state(const std::string& ok, const std::string& message) {
        // Messages emitted here are fixed internal strings, so no JSON escaping is needed.
        std::cout << "{\"ok\":" << ok
                  << ",\"message\":\"" << message << "\",\"bids\":[";

        bool first_level = true;
        for (const auto& level : buy_orders) {
            if (!first_level) std::cout << ',';
            first_level = false;
            int total_quantity = 0;
            for (const auto& order : level.second) total_quantity += order.quantity;
            std::cout << "{\"price\":" << level.first
                      << ",\"quantity\":" << total_quantity
                      << ",\"orders\":" << level.second.size() << '}';
        }

        std::cout << "],\"asks\":[";
        first_level = true;
        for (const auto& level : sell_orders) {
            if (!first_level) std::cout << ',';
            first_level = false;
            int total_quantity = 0;
            for (const auto& order : level.second) total_quantity += order.quantity;
            std::cout << "{\"price\":" << level.first
                      << ",\"quantity\":" << total_quantity
                      << ",\"orders\":" << level.second.size() << '}';
        }

        std::cout << "],\"active_orders\":[";
        bool first_order = true;
        auto emit_orders = [&](const auto& book, const char* side_name) {
            for (const auto& level : book) {
                for (const auto& order : level.second) {
                    if (!first_order) std::cout << ',';
                    first_order = false;
                    std::cout << "{\"id\":" << order.ID
                              << ",\"side\":\"" << side_name << "\""
                              << ",\"price\":" << order.price
                              << ",\"quantity\":" << order.quantity << '}';
                }
            }
        };
        emit_orders(buy_orders, "BUY");
        emit_orders(sell_orders, "SELL");

        std::cout << "],\"trades\":[";
        const std::size_t start = bridge_trades.size() > 500
                                ? bridge_trades.size() - 500 : 0;
        for (std::size_t i = start; i < bridge_trades.size(); ++i) {
            if (i > start) std::cout << ',';
            const auto& trade = bridge_trades[i];
            std::cout << "{\"buy_id\":" << trade.buyOrderID
                      << ",\"sell_id\":" << trade.sellOrderID
                      << ",\"price\":" << trade.price
                      << ",\"quantity\":" << trade.quantity << '}';
        }

        long long volume = 0;
        long long notional = 0;
        for (const auto& trade : bridge_trades) {
            volume += trade.quantity;
            notional += static_cast<long long>(trade.price) * trade.quantity;
        }

        std::cout << "],\"trade_count\":" << bridge_trades.size()
                  << ",\"traded_volume\":" << volume
                  << ",\"traded_notional\":" << notional
                  << ",\"best_bid\":";
        if (buy_orders.empty()) std::cout << "null";
        else std::cout << buy_orders.begin()->first;
        std::cout << ",\"best_ask\":";
        if (sell_orders.empty()) std::cout << "null";
        else std::cout << sell_orders.begin()->first;
        std::cout << ",\"spread\":";
        if (buy_orders.empty() || sell_orders.empty()) std::cout << "null";
        else std::cout << sell_orders.begin()->first - buy_orders.begin()->first;
        std::cout << "}\n" << std::flush;
    }

    bool parse_side(const std::string& value, Side& side) {
        if (value == "BUY" || value == "B" || value == "buy" || value == "b") {
            side = Side::BUY;
            return true;
        }
        if (value == "SELL" || value == "S" || value == "sell" || value == "s") {
            side = Side::SELL;
            return true;
        }
        return false;
    }

    void execute_limit_order(Order incoming) {
        while (incoming.quantity > 0) {
            if (incoming.side == Side::BUY) {
                if (sell_orders.empty() || sell_orders.begin()->first > incoming.price) break;
                auto level = sell_orders.begin();
                Order& resting = level->second.front();
                const int quantity = std::min(incoming.quantity, resting.quantity);
                bridge_trades.push_back({incoming.ID, resting.ID, resting.price, quantity});
                incoming.quantity -= quantity;
                resting.quantity -= quantity;
                if (resting.quantity == 0) {
                    level->second.pop_front();
                    if (level->second.empty()) sell_orders.erase(level);
                }
            } else {
                if (buy_orders.empty() || buy_orders.begin()->first < incoming.price) break;
                auto level = buy_orders.begin();
                Order& resting = level->second.front();
                const int quantity = std::min(incoming.quantity, resting.quantity);
                bridge_trades.push_back({resting.ID, incoming.ID, resting.price, quantity});
                incoming.quantity -= quantity;
                resting.quantity -= quantity;
                if (resting.quantity == 0) {
                    level->second.pop_front();
                    if (level->second.empty()) buy_orders.erase(level);
                }
            }
        }

        if (incoming.quantity > 0) {
            if (incoming.side == Side::BUY) buy_orders[incoming.price].push_back(incoming);
            else sell_orders[incoming.price].push_back(incoming);
        }
    }

    void execute_market_order(int id, Side side, int quantity) {
        int remaining = quantity;
        while (remaining > 0) {
            if (side == Side::BUY) {
                if (sell_orders.empty()) break;
                auto level = sell_orders.begin();
                Order& resting = level->second.front();
                const int executed = std::min(remaining, resting.quantity);
                bridge_trades.push_back({id, resting.ID, resting.price, executed});
                remaining -= executed;
                resting.quantity -= executed;
                if (resting.quantity == 0) {
                    level->second.pop_front();
                    if (level->second.empty()) sell_orders.erase(level);
                }
            } else {
                if (buy_orders.empty()) break;
                auto level = buy_orders.begin();
                Order& resting = level->second.front();
                const int executed = std::min(remaining, resting.quantity);
                bridge_trades.push_back({resting.ID, id, resting.price, executed});
                remaining -= executed;
                resting.quantity -= executed;
                if (resting.quantity == 0) {
                    level->second.pop_front();
                    if (level->second.empty()) buy_orders.erase(level);
                }
            }
        }
        // Any unfilled market quantity is cancelled and never rests in the book.
    }

    template <typename Book>
    bool remove_from_book(Book& book, int id) {
        for (auto level = book.begin(); level != book.end(); ++level) {
            auto& orders = level->second;
            for (auto order = orders.begin(); order != orders.end(); ++order) {
                if (order->ID == id) {
                    orders.erase(order);
                    if (orders.empty()) book.erase(level);
                    return true;
                }
            }
        }
        return false;
    }

    bool cancel_resting_order(int id) {
        // Check both sides; the old helper in main.cpp only searched the buy map.
        if (remove_from_book(buy_orders, id)) return true;
        return remove_from_book(sell_orders, id);
    }

    void reset_engine() {
        buy_orders.clear();
        sell_orders.clear();
        bridge_trades.clear();
        used_ids.clear();
        next_sequence = 1;
    }
}

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        std::istringstream input(line);
        std::string command;
        input >> command;

        if (command == "STATE") {
            print_state("true", "Current order book");
        } else if (command == "RESET") {
            reset_engine();
            print_state("true", "Order book reset");
        } else if (command == "QUIT") {
            break;
        } else if (command == "ADD") {
            int id, price, quantity;
            std::string side_text;
            Side side;
            if (!(input >> id >> side_text >> price >> quantity) ||
                id <= 0 || price <= 0 || quantity <= 0 || !parse_side(side_text, side)) {
                print_state("false", "Invalid limit-order parameters");
                continue;
            }
            if (used_ids.count(id)) {
                print_state("false", "Order ID has already been used");
                continue;
            }
            used_ids.insert(id);
            execute_limit_order({id, side, price, quantity, next_sequence++});
            print_state("true", "Limit order accepted");
        } else if (command == "MARKET") {
            int id, quantity;
            std::string side_text;
            Side side;
            if (!(input >> id >> side_text >> quantity) ||
                id <= 0 || quantity <= 0 || !parse_side(side_text, side)) {
                print_state("false", "Invalid market-order parameters");
                continue;
            }
            if (used_ids.count(id)) {
                print_state("false", "Order ID has already been used");
                continue;
            }
            used_ids.insert(id);
            execute_market_order(id, side, quantity);
            print_state("true", "Market order processed; unfilled remainder cancelled");
        } else if (command == "CANCEL") {
            int id;
            if (!(input >> id) || id <= 0) {
                print_state("false", "Invalid order ID");
                continue;
            }
            const bool cancelled = cancel_resting_order(id);
            print_state(cancelled ? "true" : "false",
                        cancelled ? "Order cancelled" : "Active order not found");
        } else {
            print_state("false", "Unknown command");
        }
    }
    return 0;
}
