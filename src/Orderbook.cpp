#include "OrderBook.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>

/**
 * @file OrderBook.cpp
 * @brief Implementation of the high-performance order book for MBP-10 reconstruction
 * 
 * This file contains the core logic for maintaining the order book state and
 * generating MBP-10 updates. The implementation is heavily optimized for speed
 * while maintaining correctness according to the specified requirements.
 */

OrderBook::OrderBook() {
    // Pre-allocate memory for better performance
    active_orders.reserve(Utils::INITIAL_RESERVE_SIZE);
    
    // Initialize statistics
    stats.reset();
    
    std::cout << "OrderBook initialized with optimizations for MBP-10 reconstruction" << std::endl;
}

OrderBook::~OrderBook() {
    // Print final statistics when order book is destroyed
    std::cout << "\nOrderBook destruction - Final Statistics:" << std::endl;
    stats.print();
}

void OrderBook::clear() {
    // Clear all data structures
    bid_levels.clear();
    ask_levels.clear();
    active_orders.clear();
    
    // Reset statistics
    stats.reset();
    
    std::cout << "OrderBook cleared (R action processed)" << std::endl;
}

const OrderBook::MBPRow* OrderBook::process_order(const Order& order) {
    Utils::Timer processing_timer("");  // Anonymous timer for this operation
    
    bool should_generate_mbp = false;
    
    // Process based on action type
    switch (order.action) {
        case Utils::ACTION_CLEAR:
            // Clear action - ignore as per requirements (first row)
            clear();
            break;
            
        case Utils::ACTION_ADD:
            should_generate_mbp = add_order(order);
            stats.total_additions_processed++;
            break;
            
        case Utils::ACTION_CANCEL:
            should_generate_mbp = cancel_order(order);
            stats.total_cancellations_processed++;
            break;
            
        case Utils::ACTION_TRADE:
            // Special handling for trade orders as per requirements
            should_generate_mbp = process_trade(order);
            stats.total_trades_processed++;
            break;
            
        case Utils::ACTION_FILL:
            // Fill actions don't directly affect order book per requirements
            // They are handled as part of T->F->C sequences
            break;
            
        default:
            // Unknown action - log but don't process
            std::cout << "Warning: Unknown action '" << order.action 
                      << "' for order " << order.order_id << std::endl;
            break;
    }
    
    stats.total_orders_processed++;
    stats.total_processing_time_ms += processing_timer.elapsed_ms();
    
    // Generate MBP snapshot if needed
    if (should_generate_mbp) {
        generate_mbp_snapshot(order);
        stats.mbp_updates_generated++;
        return &current_mbp_row;
    }
    
    return nullptr;
}

bool OrderBook::add_order(const Order& order) {
    // Validate order
    if (!order.is_valid() || order.price_scaled == 0 || order.size == 0) {
        return false;
    }
    
    // Add to active orders tracking
    active_orders[order.order_id] = OrderInfo(order.side, order.price_scaled, order.size);
    
    // Add to appropriate side
    if (order.is_bid()) {
        auto& level = bid_levels[order.price_scaled];
        if (level.order_count == 0) {
            // New price level
            level = PriceLevel(order.price_scaled, order.size, order.order_id);
        } else {
            // Existing price level
            level.add_order(order.order_id, order.size);
        }
    } else if (order.is_ask()) {
        auto& level = ask_levels[order.price_scaled];
        if (level.order_count == 0) {
            // New price level
            level = PriceLevel(order.price_scaled, order.size, order.order_id);
        } else {
            // Existing price level
            level.add_order(order.order_id, order.size);
        }
    }
    
    // Check if this affects the top 10 levels
    return affects_top_levels(order.side, order.price_scaled);
}

bool OrderBook::cancel_order(const Order& order) {
    // Find the order in our tracking
    auto order_iter = active_orders.find(order.order_id);
    if (order_iter == active_orders.end()) {
        // Order not found - might have been already cancelled or never existed
        return false;
    }
    
    const OrderInfo& order_info = order_iter->second;
    bool affects_top = affects_top_levels(order_info.side, order_info.price_scaled);
    
    // Remove from appropriate side
    if (order_info.side == Utils::SIDE_BID) {
        auto level_iter = bid_levels.find(order_info.price_scaled);
        if (level_iter != bid_levels.end()) {
            bool level_empty = level_iter->second.remove_order(order.order_id, order_info.size);
            if (level_empty) {
                bid_levels.erase(level_iter);
            }
        }
    } else if (order_info.side == Utils::SIDE_ASK) {
        auto level_iter = ask_levels.find(order_info.price_scaled);
        if (level_iter != ask_levels.end()) {
            bool level_empty = level_iter->second.remove_order(order.order_id, order_info.size);
            if (level_empty) {
                ask_levels.erase(level_iter);
            }
        }
    }
    
    // Remove from active orders
    active_orders.erase(order_iter);
    
    return affects_top;
}

bool OrderBook::process_trade(const Order& order) {
    // As per requirements:
    // 1. If side is 'N', ignore the trade
    if (order.side == Utils::SIDE_NEUTRAL) {
        return false;
    }
    
    // 2. T actions are combined with F->C actions
    // 3. The T action should be placed on the side that actually changes
    // 
    // The trade order represents a trade that will be followed by F and C actions.
    // According to the requirements, the T action should go on the side that
    // is actually affected in the book (the side being consumed).
    //
    // If we see a T action on the ASK side at price X, but there's no order
    // at that price on the ASK side, it means the trade is consuming a BID
    // order, so we should treat it as affecting the BID side.
    
    char effective_side = order.side;
    
    // Check if there's actually an order on the specified side at this price
    if (order.side == Utils::SIDE_ASK) {
        auto ask_iter = ask_levels.find(order.price_scaled);
        if (ask_iter == ask_levels.end() || ask_iter->second.order_count == 0) {
            // No orders on ASK side at this price, so trade affects BID side
            effective_side = Utils::SIDE_BID;
        }
    } else if (order.side == Utils::SIDE_BID) {
        auto bid_iter = bid_levels.find(order.price_scaled);
        if (bid_iter == bid_levels.end() || bid_iter->second.order_count == 0) {
            // No orders on BID side at this price, so trade affects ASK side
            effective_side = Utils::SIDE_ASK;
        }
    }
    
    // For now, we'll process the trade as a reduction on the effective side
    // The actual F and C actions will handle the detailed order modifications
    
    // Check if this trade affects the top 10 levels
    return affects_top_levels(effective_side, order.price_scaled);
}

void OrderBook::generate_mbp_snapshot(const Order& triggering_order) const {
    // Clear the current MBP row
    current_mbp_row = MBPRow();
    
    // Fill in metadata from the triggering order
    current_mbp_row.ts_recv = triggering_order.ts_recv;
    current_mbp_row.ts_event = triggering_order.ts_event;
    current_mbp_row.action = triggering_order.action;
    current_mbp_row.side = triggering_order.side;
    current_mbp_row.price = triggering_order.get_price();
    current_mbp_row.size = triggering_order.size;
    current_mbp_row.flags = triggering_order.flags;
    current_mbp_row.ts_in_delta = triggering_order.ts_in_delta;
    current_mbp_row.sequence = triggering_order.sequence;
    current_mbp_row.symbol = triggering_order.symbol;
    current_mbp_row.order_id = triggering_order.order_id;
    
    // Determine depth based on action and position
    current_mbp_row.depth = get_price_depth(triggering_order.side, triggering_order.price_scaled);
    
    // Update bid and ask levels
    update_bid_levels(current_mbp_row);
    update_ask_levels(current_mbp_row);
}

bool OrderBook::affects_top_levels(char side, uint64_t price_scaled) const {
    int depth = get_price_depth(side, price_scaled);
    return depth >= 0 && depth < Utils::MAX_DEPTH;
}

void OrderBook::update_bid_levels(MBPRow& mbp_row) const {
    int level = 0;
    
    // Iterate through bid levels (highest price first due to greater<> comparator)
    for (const auto& [price, level_info] : bid_levels) {
        if (level >= Utils::MAX_DEPTH) break;
        
        mbp_row.bid_levels[level] = MBPRow::Level(
            level_info.get_price(),
            level_info.total_size,
            level_info.order_count
        );
        
        level++;
    }
    
    // Clear remaining levels
    for (int i = level; i < Utils::MAX_DEPTH; ++i) {
        mbp_row.bid_levels[i] = MBPRow::Level(); // Default constructor gives zeros
    }
}

void OrderBook::update_ask_levels(MBPRow& mbp_row) const {
    int level = 0;
    
    // Iterate through ask levels (lowest price first due to less<> comparator)
    for (const auto& [price, level_info] : ask_levels) {
        if (level >= Utils::MAX_DEPTH) break;
        
        mbp_row.ask_levels[level] = MBPRow::Level(
            level_info.get_price(),
            level_info.total_size,
            level_info.order_count
        );
        
        level++;
    }
    
    // Clear remaining levels
    for (int i = level; i < Utils::MAX_DEPTH; ++i) {
        mbp_row.ask_levels[i] = MBPRow::Level(); // Default constructor gives zeros
    }
}

int OrderBook::get_price_depth(char side, uint64_t price_scaled) const {
    if (side == Utils::SIDE_BID) {
        int depth = 0;
        for (const auto& [price, level_info] : bid_levels) {
            if (price == price_scaled) {
                return depth;
            }
            depth++;
            if (depth >= Utils::MAX_DEPTH) break;
        }
    } else if (side == Utils::SIDE_ASK) {
        int depth = 0;
        for (const auto& [price, level_info] : ask_levels) {
            if (price == price_scaled) {
                return depth;
            }
            depth++;
            if (depth >= Utils::MAX_DEPTH) break;
        }
    }
    
    return -1; // Not found in top 10
}

std::pair<double, double> OrderBook::get_spread() const {
    double best_bid = 0.0;
    double best_ask = 0.0;
    
    if (!bid_levels.empty()) {
        best_bid = bid_levels.begin()->second.get_price();
    }
    
    if (!ask_levels.empty()) {
        best_ask = ask_levels.begin()->second.get_price();
    }
    
    return std::make_pair(best_bid, best_ask);
}

size_t OrderBook::get_total_orders() const {
    return active_orders.size();
}

std::pair<size_t, size_t> OrderBook::get_level_counts() const {
    return std::make_pair(bid_levels.size(), ask_levels.size());
}

void OrderBook::print_book_state(int max_levels) const {
    std::cout << "\n=== Order Book State ===" << std::endl;
    
    auto [best_bid, best_ask] = get_spread();
    std::cout << "Spread: " << std::fixed << std::setprecision(6) 
              << best_bid << " / " << best_ask;
    
    if (best_bid > 0 && best_ask > 0) {
        double spread = best_ask - best_bid;
        std::cout << " (spread: " << spread << ")";
    }
    std::cout << std::endl;
    
    std::cout << "\nASK Levels (lowest first):" << std::endl;
    int level = 0;
    for (const auto& [price, level_info] : ask_levels) {
        if (level >= max_levels) break;
        std::cout << "  L" << level << ": " << std::fixed << std::setprecision(6)
                  << level_info.get_price() << " x " << level_info.total_size 
                  << " (" << level_info.order_count << " orders)" << std::endl;
        level++;
    }
    
    std::cout << "\nBID Levels (highest first):" << std::endl;
    level = 0;
    for (const auto& [price, level_info] : bid_levels) {
        if (level >= max_levels) break;
        std::cout << "  L" << level << ": " << std::fixed << std::setprecision(6)
                  << level_info.get_price() << " x " << level_info.total_size 
                  << " (" << level_info.order_count << " orders)" << std::endl;
        level++;
    }
    
    auto [bid_count, ask_count] = get_level_counts();
    std::cout << "\nTotal levels - Bids: " << bid_count << ", Asks: " << ask_count << std::endl;
    std::cout << "Total active orders: " << get_total_orders() << std::endl;
    std::cout << "=======================" << std::endl;
}