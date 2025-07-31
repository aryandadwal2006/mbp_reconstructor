#pragma once

#include "Order.hpp"
#include "Utils.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <memory>
#include <functional>

/**
 * @brief High-performance CSV reader optimized for MBO data parsing
 * 
 * This class is specifically designed to parse MBO CSV files with maximum
 * efficiency. It implements several optimizations:
 * 
 * 1. Buffered reading to minimize I/O operations
 * 2. In-place string parsing to avoid unnecessary allocations
 * 3. Fast numeric conversions optimized for financial data
 * 4. Memory pre-allocation based on expected data sizes
 * 5. Error handling with detailed line-by-line reporting
 */
class CsvReader {
public:
    /**
     * @brief CSV parsing result structure
     * Contains both the parsed orders and metadata about the parsing process
     */
    struct ParseResult {
        std::vector<Order> orders;          // Parsed orders
        size_t total_lines_read;            // Total lines processed
        size_t successful_parses;           // Successfully parsed orders
        size_t parsing_errors;              // Number of parsing errors
        double parsing_time_ms;             // Time taken for parsing
        std::vector<std::string> error_messages; // Detailed error messages
        
        ParseResult() : total_lines_read(0), successful_parses(0), 
                       parsing_errors(0), parsing_time_ms(0.0) {
            // Pre-allocate for typical file sizes
            orders.reserve(Utils::INITIAL_RESERVE_SIZE);
            error_messages.reserve(100); // Reserve space for error messages
        }
        
        /**
         * @brief Print parsing summary
         */
        void print_summary() const;
        
        /**
         * @brief Check if parsing was successful
         */
        bool is_successful() const {
            return successful_parses > 0;
        }
        
        /**
         * @brief Get parsing success rate as percentage
         */
        double get_success_rate() const {
            if (total_lines_read <= 1) return 100.0; // Only header
            return (static_cast<double>(successful_parses) / (total_lines_read - 1)) * 100.0;
        }
    };

private:
    std::string filename;
    mutable std::ifstream file_stream;  // Made mutable for const methods
    
    // Buffer for efficient line reading
    static constexpr size_t BUFFER_SIZE = 64 * 1024; // 64KB buffer
    std::unique_ptr<char[]> read_buffer;
    
    // Column indices (determined from header)
    struct ColumnIndices {
        int ts_recv = -1;
        int ts_event = -1;
        int rtype = -1;
        int publisher_id = -1;
        int instrument_id = -1;
        int action = -1;
        int side = -1;
        int price = -1;
        int size = -1;
        int channel_id = -1;
        int order_id = -1;
        int flags = -1;
        int ts_in_delta = -1;
        int sequence = -1;
        int symbol = -1;
        
        bool is_valid() const {
            // Check that essential columns are present
            return ts_recv >= 0 && ts_event >= 0 && action >= 0 && 
                   side >= 0 && order_id >= 0 && sequence >= 0;
        }
    } column_indices;
    
    // Pre-allocated vector for line splitting to avoid repeated allocations
    mutable std::vector<std::string> split_buffer;
    
    // Statistics
    mutable Utils::Statistics parsing_stats;

public:
    /**
     * @brief Constructor
     * @param csv_filename Path to the CSV file to read
     */
    explicit CsvReader(const std::string& csv_filename);
    
    /**
     * @brief Destructor - ensures file is properly closed
     */
    ~CsvReader();
    
    /**
     * @brief Check if the file was opened successfully
     */
    bool is_open() const;
    
    /**
     * @brief Get the filename being processed
     */
    const std::string& get_filename() const { return filename; }
    
    /**
     * @brief Parse the entire CSV file and return all orders
     * 
     * This is the main entry point for parsing. It reads the entire file,
     * parses all orders, and returns them along with parsing statistics.
     * 
     * @return ParseResult containing orders and parsing metadata
     */
    ParseResult parse_all_orders();
    
    /**
     * @brief Parse orders in chunks for memory-efficient processing
     * 
     * This method allows processing very large files without loading
     * everything into memory at once.
     * 
     * @param chunk_size Maximum number of orders to parse in one chunk
     * @param callback Function called for each chunk of orders
     * @return Overall parsing statistics
     */
    ParseResult parse_in_chunks(size_t chunk_size, 
                               std::function<void(const std::vector<Order>&)> callback);
    
    /**
     * @brief Get file size in bytes
     * Useful for progress reporting and memory planning
     */
    size_t get_file_size() const;
    
    /**
     * @brief Estimate number of orders in the file
     * Uses file size and average line length estimation
     */
    size_t estimate_order_count() const;

private:
    /**
     * @brief Parse the CSV header and determine column indices
     * @param header_line The first line of the CSV file
     * @return true if header parsing was successful
     */
    bool parse_header(const std::string& header_line);
    
    /**
     * @brief Parse a single CSV line into an Order object
     * 
     * This is the performance-critical function that converts a CSV line
     * into an Order structure. It's heavily optimized for speed.
     * 
     * @param line The CSV line to parse
     * @param line_number Current line number (for error reporting)
     * @param order Output order object
     * @return true if parsing was successful
     */
    bool parse_line_to_order(const std::string& line, size_t line_number, Order& order);
    
    /**
     * @brief Fast CSV line splitting optimized for our specific format
     * 
     * Uses the pre-allocated split_buffer to avoid memory allocations.
     * Handles quoted fields and escaped commas properly.
     * 
     * @param line The line to split
     * @return Reference to the internal split_buffer
     */
    const std::vector<std::string>& split_csv_line(const std::string& line);
    
    /**
     * @brief Validate that a parsed order makes sense
     * 
     * Performs sanity checks on the parsed data to catch parsing errors
     * early and provide meaningful error messages.
     * 
     * @param order The order to validate
     * @return true if order is valid
     */
    bool validate_order(const Order& order) const;
    
    /**
     * @brief Get column name by index (for error reporting)
     * @param index Column index
     * @return Column name or "Unknown"
     */
    std::string get_column_name(int index) const;
    
    /**
     * @brief Check if the file has the expected MBO format
     * Validates that we have the required columns for MBO processing
     */
    bool validate_mbo_format() const;
    
    /**
     * @brief Efficient line reading with buffering
     * 
     * Uses internal buffer to minimize I/O operations and improve
     * performance when reading large files.
     * 
     * @param line Output string for the line
     * @return true if a line was successfully read
     */
    bool read_buffered_line(std::string& line);
    
    /**
     * @brief Handle parsing error with detailed logging
     * @param line_number The line where error occurred
     * @param error_message Description of the error
     * @param result ParseResult object to update with error info
     */
    void handle_parsing_error(size_t line_number, const std::string& error_message, 
                             ParseResult& result) const;
};