#define LOB_TESTING
#include "main.cpp"

#include <string>

int passed = 0;
int failed = 0;

void check(bool condition, const string& description) {
    if (condition) {
        cout << (condition ? "[PASS] " : "[FAIL] ") << description << '\n';
        if (condition) ++passed;
        else ++failed;
    } else {
        cout << "[FAIL] " << description << '\n';
        ++failed;
    }
}

void clear_map_book() {
    buy_orders.clear();
    sell_orders.clear();
}

int best_bid_for_test() {
    return buy_orders.empty() ? -1 : buy_orders.begin()->first;
}

int best_ask_for_test() {
    return sell_orders.empty() ? -1 : sell_orders.begin()->first;
}

int spread_for_test() {
    return (buy_orders.empty() || sell_orders.empty())
        ? -1 : best_ask_for_test() - best_bid_for_test();
}

void test_can_match() {
    clear_map_book();
    check(!can_match(), "Empty book cannot match");

    add_order_to_map({1, Side::BUY, 100, 5, 1});
    check(!can_match(), "Only buy side cannot match");

    add_order_to_map({2, Side::SELL, 105, 5, 2});
    check(!can_match(), "Non-crossing prices cannot match");

    clear_map_book();
    add_order_to_map({3, Side::BUY, 105, 5, 3});
    add_order_to_map({4, Side::SELL, 105, 5, 4});
    check(can_match(), "Equal prices can match");
}

void test_best_bid_ask_and_spread() {
    clear_map_book();
    check(best_bid_for_test() == -1, "Best bid is absent when buy book is empty");
    check(best_ask_for_test() == -1, "Best ask is absent when sell book is empty");
    check(spread_for_test() == -1, "Spread is absent if either side is empty");

    add_order_to_map({30, Side::BUY, 99, 2, 1});
    add_order_to_map({31, Side::BUY, 101, 3, 2});
    add_order_to_map({32, Side::SELL, 110, 4, 3});
    add_order_to_map({33, Side::SELL, 105, 5, 4});
    check(best_bid_for_test() == 101, "Best bid is the highest buy price");
    check(best_ask_for_test() == 105, "Best ask is the lowest sell price");
    check(spread_for_test() == 4, "Spread equals best ask minus best bid");
}

void test_cancel_map_orders() {
    clear_map_book();
    add_order_to_map({1, Side::BUY, 100, 5, 1});
    add_order_to_map({2, Side::BUY, 100, 7, 2});
    add_order_to_map({3, Side::SELL, 105, 4, 3});

    check(cancel_map_orders(1), "Cancel existing buy order");
    check(buy_orders.count(100) == 1 && buy_orders.at(100).size() == 1
          && buy_orders.at(100).front().ID == 2,
          "Other order at same buy price remains");
    check(cancel_map_orders(2), "Cancel final order at buy price level");
    check(buy_orders.empty(), "Empty buy price level is removed");
    check(cancel_map_orders(3), "Cancel existing sell order");
    check(sell_orders.empty(), "Empty sell price level is removed");
    check(!cancel_map_orders(999), "Missing order ID returns false");
}

void test_fifo_and_partial_fill() {
    clear_map_book();
    vector<Trade> trades;

    add_order_to_map({10, Side::SELL, 100, 5, 10});
    add_order_to_map({11, Side::SELL, 100, 5, 11});
    add_order_to_map({12, Side::BUY, 100, 6, 12});

    int matches = match_all(trades);
    check(matches == 2, "Matching continues across FIFO orders");
    check(trades.size() == 2, "Two trades are recorded");
    check(trades.size() >= 1 && trades[0].sellOrderID == 10
          && trades[0].quantity == 5,
          "First sell order at price level gets FIFO priority");
    check(trades.size() >= 2 && trades[1].sellOrderID == 11
          && trades[1].quantity == 1,
          "Second sell order is partially filled after first");
    check(sell_orders.count(100) == 1 && sell_orders.at(100).front().quantity == 4,
          "Remaining quantity stays on the book");
    check(!can_match(), "No crossed prices remain after matching");
}

void test_multi_price_priority() {
    clear_map_book();
    vector<Trade> trades;

    add_order_to_map({20, Side::SELL, 102, 10, 20});
    add_order_to_map({21, Side::SELL, 101, 5, 21});
    add_order_to_map({22, Side::BUY, 103, 6, 22});

    match_all(trades);
    check(trades.size() == 2, "Order matches across two price levels");
    check(trades.size() >= 1 && trades[0].price == 101
          && trades[0].quantity == 5,
          "Best sell price executes first");
    check(trades.size() >= 2 && trades[1].price == 102
          && trades[1].quantity == 1,
          "Remaining quantity executes at next sell price");
    check(sell_orders.count(102) == 1 && sell_orders.at(102).front().quantity == 9,
          "Correct remainder remains at second price level");
}

void test_fully_filled_orders_are_removed() {
    clear_map_book();
    vector<Trade> trades;
    add_order_to_map({40, Side::BUY, 100, 3, 1});
    add_order_to_map({41, Side::SELL, 100, 3, 2});
    check(match_all(trades) == 1, "Exactly one trade for equal-size orders");
    check(buy_orders.empty() && sell_orders.empty(), "Fully filled orders are removed");
    check(trades.size() == 1 && trades[0].quantity == 3,
          "Trade quantity equals matched order quantity");
}

void test_no_match_when_prices_do_not_cross() {
    clear_map_book();
    vector<Trade> trades;
    add_order_to_map({50, Side::BUY, 99, 8, 1});
    add_order_to_map({51, Side::SELL, 100, 8, 2});
    check(match_all(trades) == 0, "No trade occurs when bid is below ask");
    check(trades.empty(), "Trade history remains empty for non-crossing orders");
    check(buy_orders.size() == 1 && sell_orders.size() == 1,
          "Non-crossing orders remain on the book");
}

int main() {
    cout << "===== LIMIT ORDER BOOK UNIT TESTS =====\n";
    test_can_match();
    test_best_bid_ask_and_spread();
    test_cancel_map_orders();
    test_fifo_and_partial_fill();
    test_multi_price_priority();
    test_fully_filled_orders_are_removed();
    test_no_match_when_prices_do_not_cross();

    cout << "\n===== SUMMARY =====\n";
    cout << "Passed: " << passed << '\n';
    cout << "Failed: " << failed << '\n';
    return failed == 0 ? 0 : 1;
}
