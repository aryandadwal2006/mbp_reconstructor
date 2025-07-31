#include "Utils.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <iomanip>

// Platform-specific includes for memory tracking
#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
#elif defined(__linux__)
    #include <unistd.h>
    #include <fstream>
    #include <sys/resource.h>
#elif defined(__APPLE__)
    #include <mach/mach.h>
    #include <sys/resource.h>
#endif

namespace Utils {

/**
 * @brief High-performance string splitting optimized for CSV parsing
 * 
 * This function is heavily optimized for the CSV parsing use case.
 * It uses reserve to minimize memory allocations and processes
 * the string in a single pass.
 */
std::vector<std::string> split_string(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    
    // Reserve space based on estimated number of fields
    // Most CSV lines have 15-20 fields, so we reserve 25 to be safe
    result.reserve(25);
    
    if (str.empty()) {
        return result;
    }
    
    size_t start = 0;
    size_t end = 0;
    
    while (end < str.length()) {
        // Find next delimiter or end of string
        while (end < str.length() && str[end] != delimiter) {
            ++end;
        }
        
        // Extract substring (this creates a copy, but it's necessary)
        result.emplace_back(str.substr(start, end - start));
        
        // Move past the delimiter
        if (end < str.length()) {
            ++end;
        }
        start = end;
    }
    
    return result;
}

/**
 * @brief Fast string to double conversion optimized for price parsing
 * 
 * This function is optimized for parsing financial prices, which typically
 * have up to 9 decimal places. It handles empty strings and basic error cases.
 */
double fast_string_to_double(const std::string& str) {
    if (str.empty()) {
        return 0.0;
    }
    
    // Use fast C-style conversion
    char* end_ptr;
    double result = std::strtod(str.c_str(), &end_ptr);
    
    // Check if conversion was successful
    if (end_ptr == str.c_str()) {
        return 0.0; // No conversion occurred
    }
    
    return result;
}

/**
 * @brief Fast string to uint64_t conversion for order IDs and sequences
 * 
 * Optimized for parsing large integer values commonly found in
 * order IDs and sequence numbers.
 */
uint64_t fast_string_to_uint64(const std::string& str) {
    if (str.empty()) {
        return 0;
    }
    
    uint64_t result = 0;
    const char* ptr = str.c_str();
    
    // Manual parsing for maximum speed
    while (*ptr && std::isdigit(*ptr)) {
        result = result * 10 + (*ptr - '0');
        ++ptr;
    }
    
    return result;
}

/**
 * @brief Fast string to uint32_t conversion for sizes and flags
 * 
 * Similar to uint64_t version but for smaller values.
 */
uint32_t fast_string_to_uint32(const std::string& str) {
    if (str.empty()) {
        return 0;
    }
    
    uint32_t result = 0;
    const char* ptr = str.c_str();
    
    // Manual parsing for maximum speed
    while (*ptr && std::isdigit(*ptr)) {
        result = result * 10 + (*ptr - '0');
        ++ptr;
    }
    
    return result;
}

/**
 * @brief Format double to string with specified precision
 * 
 * Used for price formatting in output. Avoids scientific notation
 * and ensures consistent decimal places.
 */
std::string format_double(double value, int precision) {
    if (value == 0.0) {
        return "";  // Empty string for zero values in MBP output
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    std::string result = oss.str();
    
    // Remove trailing zeros after decimal point
    if (result.find('.') != std::string::npos) {
        result = result.substr(0, result.find_last_not_of('0') + 1);
        if (result.back() == '.') {
            result.pop_back();
        }
    }
    
    return result;
}

/**
 * @brief Check if string is empty or contains only whitespace
 */
bool is_empty_or_whitespace(const std::string& str) {
    return str.empty() || std::all_of(str.begin(), str.end(), 
                                     [](char c) { return std::isspace(c); });
}

/**
 * @brief Trim whitespace from both ends of string in-place
 * 
 * More efficient than creating new strings.
 */
void trim_string(std::string& str) {
    // Trim from start
    str.erase(str.begin(), std::find_if(str.begin(), str.end(),
              [](unsigned char ch) { return !std::isspace(ch); }));
    
    // Trim from end
    str.erase(std::find_if(str.rbegin(), str.rend(),
              [](unsigned char ch) { return !std::isspace(ch); }).base(),
              str.end());
}

// Timer class implementation
Timer::Timer(const std::string& name) : timer_name(name) {
    start_time = std::chrono::high_resolution_clock::now();
}

double Timer::elapsed_ms() const {
    auto current_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        current_time - start_time);
    return duration.count() / 1000.0; // Convert to milliseconds
}

double Timer::elapsed_us() const {
    auto current_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        current_time - start_time);
    return static_cast<double>(duration.count());
}

void Timer::reset() {
    start_time = std::chrono::high_resolution_clock::now();
}

void Timer::print_elapsed() const {
    double elapsed = elapsed_ms();
    std::cout << timer_name << " elapsed: " << std::fixed << std::setprecision(3) 
              << elapsed << " ms" << std::endl;
}

Timer::~Timer() {
    // Automatically print elapsed time when timer goes out of scope
    // This is useful for RAII-style timing
    if (!timer_name.empty()) {
        print_elapsed();
    }
}

// MemoryTracker implementation
long MemoryTracker::get_memory_usage() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<long>(pmc.WorkingSetSize);
    }
    return -1;
    
#elif defined(__linux__)
    std::ifstream status_file("/proc/self/status");
    std::string line;
    
    while (std::getline(status_file, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            std::istringstream iss(line);
            std::string label, value, unit;
            iss >> label >> value >> unit;
            
            long memory_kb = std::stol(value);
            return memory_kb * 1024; // Convert to bytes
        }
    }
    return -1;
    
#elif defined(__APPLE__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t info_count = MACH_TASK_BASIC_INFO_COUNT;
    
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  (task_info_t)&info, &info_count) == KERN_SUCCESS) {
        return static_cast<long>(info.resident_size);
    }
    return -1;
    
#else
    // Fallback for other platforms
    return -1;
#endif
}

void MemoryTracker::print_memory_usage(const std::string& label) {
    long memory_bytes = get_memory_usage();
    
    if (memory_bytes >= 0) {
        double memory_mb = memory_bytes / (1024.0 * 1024.0);
        std::cout << label << ": " << std::fixed << std::setprecision(2) 
                  << memory_mb << " MB" << std::endl;
    } else {
        std::cout << label << ": Unable to determine memory usage" << std::endl;
    }
}

// Statistics implementation
void Statistics::print() const {
    std::cout << "\n=== Performance Statistics ===" << std::endl;
    std::cout << "Total orders processed: " << total_orders_processed << std::endl;
    std::cout << "  - Additions: " << total_additions_processed << std::endl;
    std::cout << "  - Cancellations: " << total_cancellations_processed << std::endl;
    std::cout << "  - Trades: " << total_trades_processed << std::endl;
    std::cout << "MBP updates generated: " << mbp_updates_generated << std::endl;
    std::cout << "Total processing time: " << std::fixed << std::setprecision(3) 
              << total_processing_time_ms << " ms" << std::endl;
    
    if (total_processing_time_ms > 0.0) {
        std::cout << "Processing speed: " << std::fixed << std::setprecision(0) 
                  << get_orders_per_second() << " orders/sec" << std::endl;
    }
    
    if (total_orders_processed > 0) {
        double update_ratio = (static_cast<double>(mbp_updates_generated) / 
                              total_orders_processed) * 100.0;
        std::cout << "MBP update ratio: " << std::fixed << std::setprecision(2) 
                  << update_ratio << "%" << std::endl;
    }
    
    std::cout << "===============================" << std::endl;
}

void Statistics::reset() {
    total_orders_processed = 0;
    total_trades_processed = 0;
    total_cancellations_processed = 0;
    total_additions_processed = 0;
    mbp_updates_generated = 0;
    total_processing_time_ms = 0.0;
}

double Statistics::get_orders_per_second() const {
    if (total_processing_time_ms > 0.0) {
        return (static_cast<double>(total_orders_processed) * 1000.0) / total_processing_time_ms;
    }
    return 0.0;
}

} // namespace Utils