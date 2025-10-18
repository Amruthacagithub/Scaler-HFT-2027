#include "order_book.h"
#include <map>
#include <list>
#include <deque>
#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cassert>

struct OrderBook::Impl {
    std::deque<Order> pool;

    struct PriceLevelData {
        double price = 0.0;
        uint64_t total_quantity = 0;
        std::list<Order*> orders; // FIFO of order pointers
    };

    std::map<double, PriceLevelData, std::greater<double>> bids;

    std::map<double, PriceLevelData, std::less<double>> asks;

    std::unordered_map<uint64_t, Order*> order_lookup_public;

    using ListIt = std::list<Order*>::iterator;
    std::unordered_map<uint64_t, std::pair<PriceLevelData*, ListIt>> order_iter_lookup;

    Order* allocate_order(const Order& o) {
        pool.push_back(o);
        return &pool.back();
    }

    std::map<double, PriceLevelData, std::greater<double>>& bids_ref() { return bids; }
    std::map<double, PriceLevelData, std::less<double>>& asks_ref() { return asks; }
};

OrderBook::OrderBook() {
    pImpl = new Impl();
    order_lookup.clear();
}

OrderBook::~OrderBook() {
    delete pImpl;
    pImpl = nullptr;
    order_lookup.clear();
}

void OrderBook::add_order(const Order& order) {
    Order* stored = pImpl->allocate_order(order);

    if (order.is_buy) {
        auto &side = pImpl->bids_ref();

        auto it = side.find(order.price);
        if (it == side.end()) {
            Impl::PriceLevelData lvl;
            lvl.price = order.price;
            lvl.total_quantity = order.quantity;
            lvl.orders.push_back(stored);
            auto res = side.emplace(order.price, std::move(lvl));
            Impl::PriceLevelData &inserted = res.first->second;
            pImpl->order_iter_lookup[order.order_id] = { &inserted, std::prev(inserted.orders.end()) };
        } else {
            Impl::PriceLevelData &lvl = it->second;
            lvl.total_quantity += order.quantity;
            lvl.orders.push_back(stored);
            pImpl->order_iter_lookup[order.order_id] = { &lvl, std::prev(lvl.orders.end()) };
        }
    } else {
        auto &side = pImpl->asks_ref();

        auto it = side.find(order.price);
        if (it == side.end()) {
            Impl::PriceLevelData lvl;
            lvl.price = order.price;
            lvl.total_quantity = order.quantity;
            lvl.orders.push_back(stored);
            auto res = side.emplace(order.price, std::move(lvl));
            Impl::PriceLevelData &inserted = res.first->second;
            pImpl->order_iter_lookup[order.order_id] = { &inserted, std::prev(inserted.orders.end()) };
        } else {
            Impl::PriceLevelData &lvl = it->second;
            lvl.total_quantity += order.quantity;
            lvl.orders.push_back(stored);
            pImpl->order_iter_lookup[order.order_id] = { &lvl, std::prev(lvl.orders.end()) };
        }
    }

    pImpl->order_lookup_public[order.order_id] = stored;
    order_lookup[order.order_id] = stored; 
}

bool OrderBook::cancel_order(uint64_t order_id) {
    auto pub_it = pImpl->order_lookup_public.find(order_id);
    if (pub_it == pImpl->order_lookup_public.end()) return false;

    auto it = pImpl->order_iter_lookup.find(order_id);
    if (it == pImpl->order_iter_lookup.end()) {
        return false;
    }
    Impl::PriceLevelData* lvl = it->second.first;
    auto list_it = it->second.second;
    Order* o = *list_it;
    assert(o->order_id == order_id);

    uint64_t remove_qty = o->quantity;
    if (remove_qty >= lvl->total_quantity) lvl->total_quantity = 0;
    else lvl->total_quantity -= remove_qty;

    lvl->orders.erase(list_it);

    if (lvl->orders.empty()) {
        if (o->is_buy) pImpl->bids_ref().erase(lvl->price);
        else pImpl->asks_ref().erase(lvl->price);
    }

    o->quantity = 0;

    pImpl->order_iter_lookup.erase(order_id);
    pImpl->order_lookup_public.erase(order_id);
    order_lookup.erase(order_id);

    return true;
}

bool OrderBook::amend_order(uint64_t order_id, double new_price, uint64_t new_quantity) {
    auto pub_it = pImpl->order_lookup_public.find(order_id);
    if (pub_it == pImpl->order_lookup_public.end()) return false;
    Order* o = pub_it->second;

    if (new_price != o->price) {
        Order new_order{ o->order_id, o->is_buy, new_price, new_quantity, o->timestamp_ns };
        if (!cancel_order(order_id)) return false;
        add_order(new_order);
        return true;
    }

    auto it = pImpl->order_iter_lookup.find(order_id);
    if (it == pImpl->order_iter_lookup.end()) return false;
    Impl::PriceLevelData* lvl = it->second.first;

    uint64_t old_qty = o->quantity;
    if (new_quantity > old_qty) {
        lvl->total_quantity += (new_quantity - old_qty);
    } else {
        lvl->total_quantity -= (old_qty - new_quantity);
    }
    o->quantity = new_quantity;
    return true;
}

void OrderBook::get_snapshot(size_t depth, std::vector<PriceLevel>& bids, std::vector<PriceLevel>& asks) const {
    bids.clear();
    asks.clear();

    size_t cnt = 0;
    for (auto it = pImpl->bids.begin(); it != pImpl->bids.end() && cnt < depth; ++it, ++cnt) {
        bids.push_back(PriceLevel{ it->first, it->second.total_quantity });
    }

    cnt = 0;
    for (auto it = pImpl->asks.begin(); it != pImpl->asks.end() && cnt < depth; ++it, ++cnt) {
        asks.push_back(PriceLevel{ it->first, it->second.total_quantity });
    }
}

void OrderBook::print_book(size_t depth) const {
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
    get_snapshot(depth, bids, asks);

    std::cout << "\n------ ORDER BOOK (Top " << depth << ") ------\n";
    std::cout << "    BIDS\t\t\tASKS\n";
    std::cout << "Price\tQty\t | Price\tQty\n";
    size_t rows = std::max(bids.size(), asks.size());
    for (size_t i = 0; i < rows; ++i) {
        if (i < bids.size()) {
            std::cout << std::fixed << std::setprecision(2) << bids[i].price << '\t' << bids[i].total_quantity;
        } else {
            std::cout << "\t\t";
        }
        std::cout << '\t' << " | ";
        if (i < asks.size()) {
            std::cout << std::fixed << std::setprecision(2) << asks[i].price << '\t' << asks[i].total_quantity;
        }
        std::cout << '\n';
    }
    std::cout << "------------------------------\n";
}
