#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>

/**
 * @brief Utility functions and constants for the order book reconstructor
 * 
 * This header contains various helper functions, constants, and utilities
 * that are used across the project for performance optimization and
 * code reusability.
 */

namespace Utils {
    
    // Constants for performance optimization
    constexpr int MAX_DEPTH = 10;              // Maximum depth for MBP-10
    constexpr double PRICE_EPSILON = 1e-9;     // Precision for price comparisons
    constexpr size_t INITIAL_RESERVE_SIZE = 10000; // Initial vector reserve size
    
    // Action types as constants for faster comparison
    constexpr char ACTION_ADD = 'A';
    constexpr char ACTION_CANCEL = 'C';
    constexpr char ACTION_TRADE = 'T';
    constexpr char ACTION_FILL = 'F';
    constexpr char ACTION_CLEAR = 'R';
    
    // Side types
    constexpr char SIDE_BID = 'B';
    constexpr char SIDE_ASK = 'A';
    constexpr char SIDE_NEUTRAL = 'N';
    
    /**
     * @brief High-performance string splitting function
     * Uses reserve to minimize memory allocations
     * 
     * @param str The string to split
     * @param delimiter The delimiter character
     * @return Vector of split strings
     */
    std::vector<std::string> split_string(const std::string& str, char delimiter);
    
    /**
     * @brief Fast string to double conversion
     * Optimized for price parsing with error handling
     * 
     * @param str The string to convert
     * @return Converted double value, 0.0 if conversion fails
     */
    double fast_string_to_double(const std::string& str);
    
    /**
     * @brief Fast string to uint64_t conversion
     * Optimized for order ID and sequence number parsing
     * 
     * @param str The string to convert
     * @return Converted uint64_t value, 0 if conversion fails
     */
    uint64_t fast_string_to_uint64(const std::string& str);
    
    /**
     * @brief Fast string to uint32_t conversion
     * Optimized for size and flags parsing
     * 
     * @param str The string to convert
     * @return Converted uint32_t value, 0 if conversion fails
     */
    uint32_t fast_string_to_uint32(const std::string& str);
    
    /**
     * @brief Format double to string with specified precision
     * Used for price formatting in output
     * 
     * @param value The double value to format
     * @param precision Number of decimal places
     * @return Formatted string
     */
    std::string format_double(double value, int precision = 9);
    
    /**
     * @brief Check if a string is empty or contains only whitespace
     * 
     * @param str The string to check
     * @return True if empty or whitespace only
     */
    bool is_empty_or_whitespace(const std::string& str);
    
    /**
     * @brief Trim whitespace from both ends of string
     * In-place operation for performance
     * 
     * @param str String to trim (modified in place)
     */
    void trim_string(std::string& str);
    
    /**
     * @brief Performance timer class for benchmarking
     * Uses high-resolution clock for accurate measurements
     */
    class Timer {
    private:
        std::chrono::high_resolution_clock::time_point start_time;
        std::string timer_name;
        
    public:
        /**
         * @brief Constructor starts the timer
         * @param name Name of the timer for identification
         */
        explicit Timer(const std::string& name = "Timer");
        
        /**
         * @brief Get elapsed time in milliseconds
         * @return Elapsed time in milliseconds
         */
        double elapsed_ms() const;
        
        /**
         * @brief Get elapsed time in microseconds
         * @return Elapsed time in microseconds
         */
        double elapsed_us() const;
        
        /**
         * @brief Reset the timer
         */
        void reset();
        
        /**
         * @brief Print elapsed time with the timer name
         */
        void print_elapsed() const;
        
        /**
         * @brief Destructor prints elapsed time automatically
         */
        ~Timer();
    };
    
    /**
     * @brief Memory usage tracking utility
     * Helps monitor memory consumption during reconstruction
     */
    class MemoryTracker {
    public:
        /**
         * @brief Get current memory usage in bytes
         * @return Memory usage in bytes, -1 if unable to determine
         */
        static long get_memory_usage();
        
        /**
         * @brief Print current memory usage in human-readable format
         * @param label Label to print with the memory usage
         */
        static void print_memory_usage(const std::string& label = "Memory Usage");
    };
    
    /**
     * @brief Statistics collector for performance analysis
     */
    struct Statistics {
        size_t total_orders_processed = 0;
        size_t total_trades_processed = 0;
        size_t total_cancellations_processed = 0;
        size_t total_additions_processed = 0;
        size_t mbp_updates_generated = 0;
        double total_processing_time_ms = 0.0;
        
        /**
         * @brief Print comprehensive statistics
         */
        void print() const;
        
        /**
         * @brief Reset all statistics
         */
        void reset();
        
        /**
         * @brief Calculate orders per second
         */
        double get_orders_per_second() const;
    };
    
} // namespace Utils