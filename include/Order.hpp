#pragma once

#include <string>
#include <cstdint>

/**
 * @brief Represents a single order in the order book
 * 
 * This structure holds all the essential information about an order
 * that's needed for order book reconstruction. We use fixed-size types
 * for better memory alignment and performance.
 */
struct Order {
    // Core order identification
    uint64_t order_id;          // Unique identifier for the order
    
    // Price and size information (stored as integers for precision)
    // Price is multiplied by 1e9 to avoid floating point precision issues
    uint64_t price_scaled;      // Price * 1e9 (to handle up to 9 decimal places)
    uint32_t size;              // Order size/quantity
    
    // Market data fields
    char side;                  // 'B' for bid, 'A' for ask
    char action;                // 'A' for add, 'C' for cancel, 'T' for trade, etc.
    
    // Timing information
    std::string ts_recv;        // Reception timestamp
    std::string ts_event;       // Event timestamp
    
    // Additional metadata
    uint32_t flags;             // Order flags
    uint64_t ts_in_delta;       // Timestamp delta
    uint64_t sequence;          // Sequence number
    std::string symbol;         // Trading symbol
    
    /**
     * @brief Default constructor
     */
    Order() : order_id(0), price_scaled(0), size(0), side('N'), action(' '), 
              flags(0), ts_in_delta(0), sequence(0) {}
    
    /**
     * @brief Parameterized constructor for quick order creation
     */
    Order(uint64_t id, uint64_t price, uint32_t sz, char s, char act, 
          const std::string& ts_r, const std::string& ts_e, uint32_t f, 
          uint64_t delta, uint64_t seq, const std::string& sym)
        : order_id(id), price_scaled(price), size(sz), side(s), action(act),
          ts_recv(ts_r), ts_event(ts_e), flags(f), ts_in_delta(delta), 
          sequence(seq), symbol(sym) {}
    
    /**
     * @brief Get the actual price as a double
     * We scale back from the integer representation
     */
    inline double get_price() const {
        return static_cast<double>(price_scaled) / 1e9;
    }
    
    /**
     * @brief Set price from double value
     * We scale up to integer representation for precision
     */
    inline void set_price(double price) {
        price_scaled = static_cast<uint64_t>(price * 1e9 + 0.5); // +0.5 for rounding
    }
    
    /**
     * @brief Check if this is a bid order
     */
    inline bool is_bid() const {
        return side == 'B';
    }
    
    /**
     * @brief Check if this is an ask order
     */
    inline bool is_ask() const {
        return side == 'A';
    }
    
    /**
     * @brief Check if this is a valid order (not neutral side)
     */
    inline bool is_valid() const {
        return side == 'B' || side == 'A';
    }
};