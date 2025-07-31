#pragma once

#include "OrderBook.hpp"
#include "Utils.hpp"
#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include <memory>

/**
 * @brief High-performance CSV writer optimized for MBP-10 output
 * 
 * This class is specifically designed to write MBP-10 CSV files with maximum
 * efficiency and exact format matching. Key optimizations include:
 * 
 * 1. Buffered writing to minimize I/O operations
 * 2. Pre-formatted string templates to avoid repeated formatting
 * 3. Fast double-to-string conversion with precise decimal handling
 * 4. Memory-efficient row construction
 * 5. Exact format matching with the expected mbp.csv output
 */
class CsvWriter {
public:
    /**
     * @brief Writing statistics and result information
     */
    struct WriteResult {
        size_t rows_written;                // Number of MBP rows written
        size_t bytes_written;               // Total bytes written to file
        double writing_time_ms;             // Time taken for writing
        bool success;                       // Overall success flag
        std::string error_message;          // Error details if any
        
        WriteResult() : rows_written(0), bytes_written(0), writing_time_ms(0.0), 
                       success(true) {}
        
        /**
         * @brief Print writing summary
         */
        void print_summary() const;
        
        /**
         * @brief Get writing speed in rows per second
         */
        double get_rows_per_second() const {
            return writing_time_ms > 0.0 ? (rows_written * 1000.0) / writing_time_ms : 0.0;
        }
        
        /**
         * @brief Get writing speed in MB per second
         */
        double get_mbps() const {
            return writing_time_ms > 0.0 ? (bytes_written / 1024.0 / 1024.0 * 1000.0) / writing_time_ms : 0.0;
        }
    };

private:
    std::string output_filename;
    std::ofstream output_stream;
    
    // Write buffer for performance optimization
    static constexpr size_t WRITE_BUFFER_SIZE = 128 * 1024; // 128KB buffer
    std::unique_ptr<char[]> write_buffer;
    std::ostringstream buffer_stream;
    
    // Row counter for the index column
    size_t row_index;
    
    // Pre-formatted strings to avoid repeated string operations
    std::string header_line;
    
    // Statistics tracking
    mutable WriteResult current_result;
    Utils::Timer write_timer;

public:
    /**
     * @brief Constructor
     * @param csv_filename Path to the output CSV file
     */
    explicit CsvWriter(const std::string& csv_filename);
    
    /**
     * @brief Destructor - ensures proper file closure and final flush
     */
    ~CsvWriter();
    
    /**
     * @brief Check if the output file was opened successfully
     */
    bool is_open() const;
    
    /**
     * @brief Get the output filename
     */
    const std::string& get_filename() const { return output_filename; }
    
    /**
     * @brief Write the CSV header
     * Must be called before writing any MBP rows
     * 
     * @return true if header was written successfully
     */
    bool write_header();
    
    /**
     * @brief Write a single MBP row to the output file
     * 
     * This is the main function for writing MBP-10 data. It formats
     * the OrderBook::MBPRow into the exact CSV format required.
     * 
     * @param mbp_row The MBP row to write
     * @return true if row was written successfully
     */
    bool write_mbp_row(const OrderBook::MBPRow& mbp_row);
    
    /**
     * @brief Write multiple MBP rows efficiently
     * 
     * This method is optimized for writing large batches of rows
     * with minimal I/O operations through buffering.
     * 
     * @param mbp_rows Vector of MBP rows to write
     * @return true if all rows were written successfully
     */
    bool write_mbp_rows(const std::vector<OrderBook::MBPRow>& mbp_rows);
    
    /**
     * @brief Flush any buffered data to disk
     * Should be called periodically during long writes
     * 
     * @return true if flush was successful
     */
    bool flush();
    
    /**
     * @brief Get current writing statistics
     */
    const WriteResult& get_write_result() const { return current_result; }
    
    /**
     * @brief Reset statistics counters
     */
    void reset_statistics();
    
    /**
     * @brief Close the output file explicitly
     * Automatically called by destructor
     */
    void close();

private:
    /**
     * @brief Initialize the write buffer and prepare for output
     * @return true if initialization was successful
     */
    bool initialize_writer();
    
    /**
     * @brief Format a single MBP row into CSV string
     * 
     * This function handles the precise formatting required to match
     * the expected output format exactly. It's optimized for speed
     * while maintaining exact decimal precision.
     * 
     * @param mbp_row The MBP row to format
     * @param output Output string (pre-allocated for performance)
     * @return true if formatting was successful
     */
    bool format_mbp_row(const OrderBook::MBPRow& mbp_row, std::string& output);
    
    /**
     * @brief Format price value with exact precision
     * 
     * Handles the specific decimal formatting required for prices
     * in the output CSV. Uses fast conversion and avoids scientific notation.
     * 
     * @param price The price value to format
     * @return Formatted price string
     */
    std::string format_price(double price) const;
    
    /**
     * @brief Format size value (always integer)
     * @param size The size value to format
     * @return Formatted size string
     */
    std::string format_size(uint64_t size) const;
    
    /**
     * @brief Format count value (always integer)
     * @param count The count value to format
     * @return Formatted count string
     */
    std::string format_count(uint32_t count) const;
    
    /**
     * @brief Format timestamp string
     * 
     * Ensures timestamp format matches exactly with input format
     * 
     * @param timestamp The timestamp string to format
     * @return Formatted timestamp string
     */
    std::string format_timestamp(const std::string& timestamp) const;
    
    /**
     * @brief Create the CSV header string
     * 
     * Generates the exact header format required for MBP-10 output.
     * This includes all the metadata columns plus the 20 price level columns
     * (10 bid levels + 10 ask levels, each with price, size, count).
     * 
     * @return Formatted header string
     */
    std::string create_header_line();
    
    /**
     * @brief Write string to output with buffering
     * 
     * Uses internal buffer to batch writes and improve I/O performance
     * 
     * @param data The string data to write
     * @return true if write was successful
     */
    bool buffered_write(const std::string& data);
    
    /**
     * @brief Handle writing error with detailed logging
     * @param error_message Description of the error
     */
    void handle_write_error(const std::string& error_message);
    
    /**
     * @brief Validate MBP row before writing
     * 
     * Performs sanity checks on the MBP row to ensure data integrity
     * 
     * @param mbp_row The row to validate
     * @return true if row is valid for writing
     */
    bool validate_mbp_row(const OrderBook::MBPRow& mbp_row) const;
    
    /**
     * @brief Get the current file position/size
     * Used for statistics tracking
     * 
     * @return Current file size in bytes
     */
    size_t get_current_file_size();
    
    /**
     * @brief Create empty level string for unused price levels
     * 
     * When there are fewer than 10 levels on a side, we need to output
     * empty fields for the unused levels.
     * 
     * @return String with empty price, size, and count fields
     */
    std::string create_empty_level_string() const;
};