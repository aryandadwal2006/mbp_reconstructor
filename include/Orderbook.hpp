#pragma once

#include "Order.hpp"
#include "Utils.hpp"
#include <map>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>  // ADD THIS LINE - needed for std::find

/**
 * @brief High-performance order book implementation for MBP-10 reconstruction
 * 
 * This class maintains the state of the limit order book and provides
 * efficient methods for order management and MBP-10 generation.
 * 
 * Key optimizations:
 * - Uses maps for price-level aggregation (automatic sorting)
 * - Maintains order tracking for fast cancellations
 * - Pre-allocated vectors for MBP output
 * - Minimal memory allocations during hot path operations
 */
class OrderBook {
public:
    /**
     * @brief Price level information
     * Aggregates all orders at a specific price level
     */
    struct PriceLevel {
        uint64_t price_scaled;      // Price * 1e9 for precision
        uint64_t total_size;        // Total size at this price level
        uint32_t order_count;       // Number of orders at this level
        
        // We maintain order IDs for proper cancellation handling
        std::vector<uint64_t> order_ids;
        
        PriceLevel() : price_scaled(0), total_size(0), order_count(0) {
            order_ids.reserve(10); // Most price levels have few orders
        }
        
        PriceLevel(uint64_t price, uint64_t size, uint64_t order_id) 
            : price_scaled(price), total_size(size), order_count(1) {
            order_ids.reserve(10);
            order_ids.push_back(order_id);
        }
        
        /**
         * @brief Add an order to this price level
         */
        void add_order(uint64_t order_id, uint64_t size) {
            order_ids.push_back(order_id);
            total_size += size;
            order_count++;
        }
        
        /**
         * @brief Remove an order from this price level
         * @return true if the price level becomes empty
         */
        bool remove_order(uint64_t order_id, uint64_t size) {
            auto it = std::find(order_ids.begin(), order_ids.end(), order_id);
            if (it != order_ids.end()) {
                order_ids.erase(it);
                total_size -= size;
                order_count--;
                return order_count == 0;
            }
            return false;
        }
        
        /**
         * @brief Get price as double
         */
        double get_price() const {
            return static_cast<double>(price_scaled) / 1e9;
        }
    };
    
    /**
     * @brief MBP-10 output row structure
     * Represents one row in the output CSV with all 10 bid/ask levels
     */
    struct MBPRow {
        // Metadata from the triggering order
        std::string ts_recv;
        std::string ts_event;
        int rtype;
        int publisher_id;
        int instrument_id;
        char action;
        char side;
        int depth;
        double price;
        uint64_t size;
        uint32_t flags;
        uint64_t ts_in_delta;
        uint64_t sequence;
        std::string symbol;
        uint64_t order_id;
        
        // MBP-10 data: 10 levels each for bid and ask
        struct Level {
            double price;
            uint64_t size;
            uint32_t count;
            
            Level() : price(0.0), size(0), count(0) {}
            Level(double p, uint64_t s, uint32_t c) : price(p), size(s), count(c) {}
        };
        
        Level bid_levels[Utils::MAX_DEPTH];
        Level ask_levels[Utils::MAX_DEPTH];
        
        MBPRow() : rtype(10), publisher_id(2), instrument_id(1108), 
                   action(' '), side('N'), depth(0), price(0.0), size(0), 
                   flags(0), ts_in_delta(0), sequence(0), order_id(0) {}
    };

private:
    // Bid side: higher prices first (reverse order for max-heap behavior)
    std::map<uint64_t, PriceLevel, std::greater<uint64_t>> bid_levels;
    
    // Ask side: lower prices first (natural order for min-heap behavior)
    std::map<uint64_t, PriceLevel, std::less<uint64_t>> ask_levels;
    
    // Fast order lookup for cancellations and modifications
    // Maps order_id -> (side, price_scaled, size)
    struct OrderInfo {
        char side;
        uint64_t price_scaled;
        uint64_t size;
        
        OrderInfo() : side('N'), price_scaled(0), size(0) {}
        OrderInfo(char s, uint64_t p, uint64_t sz) : side(s), price_scaled(p), size(sz) {}
    };
    
    std::unordered_map<uint64_t, OrderInfo> active_orders;
    
    // Performance statistics
    mutable Utils::Statistics stats;
    
    // Pre-allocated MBP row to avoid repeated allocations
    mutable MBPRow current_mbp_row;

public:
    /**
     * @brief Constructor initializes the order book
     */
    OrderBook();
    
    /**
     * @brief Destructor - prints final statistics
     */
    ~OrderBook();
    
    /**
     * @brief Clear the entire order book (for 'R' action)
     */
    void clear();
    
    /**
     * @brief Process an individual order and return MBP update if needed
     * 
     * This is the main entry point for order processing.
     * Returns nullptr if no MBP update should be generated.
     * 
     * @param order The order to process
     * @return Pointer to MBPRow if update needed, nullptr otherwise
     */
    const MBPRow* process_order(const Order& order);
    
    /**
     * @brief Add a new order to the book
     * @param order The order to add
     * @return true if MBP update should be generated
     */
    bool add_order(const Order& order);
    
    /**
     * @brief Cancel an existing order
     * @param order The cancellation order
     * @return true if MBP update should be generated
     */
    bool cancel_order(const Order& order);
    
    /**
     * @brief Process a trade order (special handling as per requirements)
     * 
     * According to the requirements:
     * - T actions are combined with F and C actions
     * - The T action should be placed on the side that actually changes
     * - If side is 'N', ignore the trade
     * 
     * @param order The trade order
     * @return true if MBP update should be generated
     */
    bool process_trade(const Order& order);
    
    /**
     * @brief Generate current MBP-10 snapshot
     * Fills the pre-allocated MBPRow with current book state
     * 
     * @param triggering_order The order that triggered this update
     */
    void generate_mbp_snapshot(const Order& triggering_order) const;
    
    /**
     * @brief Get current bid/ask spread
     * @return pair of (best_bid, best_ask), 0.0 if side is empty
     */
    std::pair<double, double> get_spread() const;
    
    /**
     * @brief Get total number of orders in the book
     */
    size_t get_total_orders() const;
    
    /**
     * @brief Get number of price levels on each side
     * @return pair of (bid_levels, ask_levels)
     */
    std::pair<size_t, size_t> get_level_counts() const;
    
    /**
     * @brief Print current book state (for debugging)
     * @param max_levels Maximum levels to print (default 5)
     */
    void print_book_state(int max_levels = 5) const;
    
    /**
     * @brief Get performance statistics
     */
    const Utils::Statistics& get_statistics() const { return stats; }
    
    /**
     * @brief Reset performance statistics
     */
    void reset_statistics() { stats.reset(); }

private:
    /**
     * @brief Helper function to determine if order affects top 10 levels
     * Used to optimize MBP generation - only generate updates for relevant changes
     * 
     * @param side The side being modified ('B' or 'A')
     * @param price_scaled The price level being modified
     * @return true if this change affects MBP-10 output
     */
    bool affects_top_levels(char side, uint64_t price_scaled) const;
    
    /**
     * @brief Update bid levels in MBP row
     * @param mbp_row The MBP row to update
     */
    void update_bid_levels(MBPRow& mbp_row) const;
    
    /**
     * @brief Update ask levels in MBP row
     * @param mbp_row The MBP row to update
     */
    void update_ask_levels(MBPRow& mbp_row) const;
    
    /**
     * @brief Get the depth (0-based index) of a price level on given side
     * Returns -1 if not in top 10
     * 
     * @param side 'B' for bid, 'A' for ask
     * @param price_scaled The price to check
     * @return Depth index (0-9) or -1 if not in top 10
     */
    int get_price_depth(char side, uint64_t price_scaled) const;
};