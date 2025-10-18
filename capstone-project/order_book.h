#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>

struct Order {
    uint64_t order_id;  
    bool is_buy;         
    double price;      
    uint64_t quantity;  
    uint64_t timestamp_ns; 
};

struct PriceLevel {
    double price;
    uint64_t total_quantity;
};

class OrderBook {
public:
    OrderBook();
    ~OrderBook();

    void add_order(const Order& order);

    bool cancel_order(uint64_t order_id);

    bool amend_order(uint64_t order_id, double new_price, uint64_t new_quantity);

    void get_snapshot(size_t depth, std::vector<PriceLevel>& bids, std::vector<PriceLevel>& asks) const;

    void print_book(size_t depth = 10) const;

    std::unordered_map<uint64_t, Order*> order_lookup;

private:
    struct Impl;
    Impl* pImpl;
};
