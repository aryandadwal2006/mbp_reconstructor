#include "Order.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

/**
 * @file Order.cpp
 * @brief Implementation of Order structure methods
 * 
 * This file contains the implementation of utility methods for the Order
 * structure. While most methods are inlined in the header for performance,
 * some utility functions that are not performance-critical are implemented here.
 */

// Note: Most Order methods are implemented inline in the header file
// for maximum performance. This file contains additional utility methods
// that are useful for debugging and logging but not performance-critical.

namespace OrderUtils {
    
    /**
     * @brief Convert action character to human-readable string
     * Useful for debugging and logging purposes
     * 
     * @param action The action character
     * @return Human-readable action string
     */
    std::string action_to_string(char action) {
        switch (action) {
            case 'A': return "ADD";
            case 'C': return "CANCEL";
            case 'T': return "TRADE";
            case 'F': return "FILL";
            case 'R': return "CLEAR";
            case 'M': return "MODIFY";
            default: return "UNKNOWN(" + std::string(1, action) + ")";
        }
    }
    
    /**
     * @brief Convert side character to human-readable string
     * Useful for debugging and logging purposes
     * 
     * @param side The side character
     * @return Human-readable side string
     */
    std::string side_to_string(char side) {
        switch (side) {
            case 'B': return "BID";
            case 'A': return "ASK";
            case 'N': return "NEUTRAL";
            default: return "UNKNOWN(" + std::string(1, side) + ")";
        }
    }
    
    /**
     * @brief Validate an order for basic correctness
     * Performs sanity checks on order data
     * 
     * @param order The order to validate
     * @return true if order appears valid
     */
    bool validate_order(const Order& order) {
        // Check for valid action
        if (order.action != 'A' && order.action != 'C' && order.action != 'T' && 
            order.action != 'F' && order.action != 'R' && order.action != 'M') {
            return false;
        }
        
        // Check for valid side (allow 'N' for some actions like clear)
        if (order.side != 'B' && order.side != 'A' && order.side != 'N') {
            return false;
        }
        
        // For most actions, we need a valid order ID
        if (order.action != 'R' && order.order_id == 0) {
            return false;
        }
        
        // For add orders, we need valid price and size
        if (order.action == 'A') {
            if (order.price_scaled == 0 || order.size == 0) {
                return false;
            }
            
            // Price should be reasonable (not negative, not absurdly large)
            double price = order.get_price();
            if (price <= 0.0 || price > 1000000.0) { // Reasonable upper bound
                return false;
            }
        }
        
        // Timestamps should not be empty for most actions
        if (order.action != 'R' && (order.ts_recv.empty() || order.ts_event.empty())) {
            return false;
        }
        
        // Symbol should not be empty
        if (order.symbol.empty()) {
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Print order details in human-readable format
     * Useful for debugging and logging
     * 
     * @param order The order to print
     * @param detailed Whether to print all fields or just key ones
     */
    void print_order(const Order& order, bool detailed = false) {
        std::cout << "Order ID: " << order.order_id 
                  << " | Action: " << action_to_string(order.action)
                  << " | Side: " << side_to_string(order.side);
        
        if (order.price_scaled > 0) {
            std::cout << " | Price: " << std::fixed << std::setprecision(9) 
                      << order.get_price();
        }
        
        if (order.size > 0) {
            std::cout << " | Size: " << order.size;
        }
        
        std::cout << " | Symbol: " << order.symbol;
        
        if (detailed) {
            std::cout << "\n  Timestamps: recv=" << order.ts_recv 
                      << ", event=" << order.ts_event
                      << "\n  Flags: " << order.flags
                      << ", Delta: " << order.ts_in_delta
                      << ", Sequence: " << order.sequence;
        }
        
        std::cout << std::endl;
    }
    
    /**
     * @brief Compare two orders for equality
     * Useful for testing and validation
     * 
     * @param order1 First order
     * @param order2 Second order
     * @return true if orders are equal
     */
    bool orders_equal(const Order& order1, const Order& order2) {
        return order1.order_id == order2.order_id &&
               order1.price_scaled == order2.price_scaled &&
               order1.size == order2.size &&
               order1.side == order2.side &&
               order1.action == order2.action &&
               order1.ts_recv == order2.ts_recv &&
               order1.ts_event == order2.ts_event &&
               order1.flags == order2.flags &&
               order1.ts_in_delta == order2.ts_in_delta &&
               order1.sequence == order2.sequence &&
               order1.symbol == order2.symbol;
    }
    
    /**
     * @brief Create a formatted string representation of an order
     * Useful for logging and debugging
     * 
     * @param order The order to convert
     * @return Formatted string representation
     */
    std::string order_to_string(const Order& order) {
        std::ostringstream oss;
        oss << "[" << order.sequence << "] "
            << action_to_string(order.action) << " "
            << side_to_string(order.side) << " "
            << order.order_id;
        
        if (order.price_scaled > 0) {
            oss << " @ " << std::fixed << std::setprecision(9) << order.get_price();
        }
        
        if (order.size > 0) {
            oss << " x" << order.size;
        }
        
        oss << " (" << order.symbol << ")";
        
        return oss.str();
    }
    
    /**
     * @brief Parse timestamp string and extract time components
     * Useful for time-based analysis and filtering
     * 
     * @param timestamp ISO format timestamp string
     * @return Pair of (date_string, time_string) or empty strings if parsing fails
     */
    std::pair<std::string, std::string> parse_timestamp(const std::string& timestamp) {
        // Expected format: 2025-07-17T08:05:03.360677248Z
        size_t t_pos = timestamp.find('T');
        size_t z_pos = timestamp.find('Z');
        
        if (t_pos != std::string::npos && z_pos != std::string::npos && t_pos < z_pos) {
            std::string date_part = timestamp.substr(0, t_pos);
            std::string time_part = timestamp.substr(t_pos + 1, z_pos - t_pos - 1);
            return std::make_pair(date_part, time_part);
        }
        
        return std::make_pair("", "");
    }
    
    /**
     * @brief Extract hour from timestamp
     * Useful for time-based filtering and analysis
     * 
     * @param timestamp ISO format timestamp string
     * @return Hour (0-23) or -1 if parsing fails
     */
    int extract_hour_from_timestamp(const std::string& timestamp) {
        auto [date, time] = parse_timestamp(timestamp);
        if (!time.empty() && time.length() >= 2) {
            try {
                return std::stoi(time.substr(0, 2));
            } catch (...) {
                return -1;
            }
        }
        return -1;
    }
    
    /**
     * @brief Check if two orders represent the same trade sequence
     * Useful for identifying T->F->C sequences
     * 
     * @param trade_order The trade order (T action)
     * @param fill_order The fill order (F action)
     * @param cancel_order The cancel order (C action)
     * @return true if they form a valid T->F->C sequence
     */
    bool is_trade_sequence(const Order& trade_order, const Order& fill_order, 
                          const Order& cancel_order) {
        // Check action sequence
        if (trade_order.action != 'T' || fill_order.action != 'F' || cancel_order.action != 'C') {
            return false;
        }
        
        // Check that fill and cancel are on the same side
        if (fill_order.side != cancel_order.side) {
            return false;
        }
        
        // Check that prices match for fill and cancel
        if (fill_order.price_scaled != cancel_order.price_scaled) {
            return false;
        }
        
        // Check that sizes match for fill and cancel
        if (fill_order.size != cancel_order.size) {
            return false;
        }
        
        // Check that order IDs match for fill and cancel
        if (fill_order.order_id != cancel_order.order_id) {
            return false;
        }
        
        // Check sequence ordering (trade should come first)
        if (trade_order.sequence >= fill_order.sequence || 
            fill_order.sequence >= cancel_order.sequence) {
            return false;
        }
        
        return true;
    }
    
} // namespace OrderUtils