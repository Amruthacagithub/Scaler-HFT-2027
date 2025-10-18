#include "order_book.h"
#include <iostream>
#include <vector>
#include <cstdint>

std::string format_price(double p);
std::string format_qty(uint64_t q);

int main() {
    OrderBook book;

    uint64_t ts = 1'000'000'000ULL;

    book.add_order(Order{1, true, 100.00, 10, ts++});
    book.add_order(Order{2, true, 101.00, 5, ts++});
    book.add_order(Order{3, false, 102.00, 7, ts++});
    book.add_order(Order{4, false, 103.00, 3, ts++});
    book.add_order(Order{5, true, 104.00, 2, ts++});

    std::cout << "Initial book:\n";
    book.print_book(6);

    std::cout << "\nAmend order 1 -> quantity 4\n";
    book.amend_order(1, 100.00, 4);
    book.print_book(5);

    std::cout << "\nAmend order 2 -> price 100.00 (move)\n";
    book.amend_order(2, 100.00, 5);
    book.print_book(5);

    std::cout << "\nCancel order 4\n";
    book.cancel_order(4);
    book.print_book(5);

    std::cout << "\nAdd crossing sell order (id 6) at 100.00 qty 6\n";
    book.add_order(Order{6, false, 100.00, 6, ts++});
    book.print_book(5);

    std::vector<PriceLevel> bids, asks;
    book.get_snapshot(3, bids, asks);

    std::cout << "\nProgrammatic snapshot (top 3):\n";
    std::cout << "BIDS:\n";
    for (auto &b : bids) {
        std::cout << "  " << format_price(b.price) << " x " << format_qty(b.total_quantity) << "\n";
    }
    std::cout << "ASKS:\n";
    for (auto &a : asks) {
        std::cout << "  " << format_price(a.price) << " x " << format_qty(a.total_quantity) << "\n";
    }

    return 0;
}

//  g++ -std=c++17 main.cpp order_book.cpp helper.cpp
// /a.out