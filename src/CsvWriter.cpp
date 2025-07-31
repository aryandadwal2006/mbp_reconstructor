#include "CsvWriter.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

/**
 * @file CsvWriter.cpp
 * @brief Implementation of high-performance CSV writer for MBP-10 output
 * 
 * This implementation focuses on exact format matching with the expected
 * mbp.csv output while maintaining high performance through:
 * - Buffered writing to minimize I/O operations
 * - Pre-formatted string templates
 * - Efficient double-to-string conversion
 * - Precise decimal handling for financial data
 */

CsvWriter::CsvWriter(const std::string& csv_filename) 
    : output_filename(csv_filename), row_index(0) {
    
    // Initialize write buffer
    write_buffer = std::make_unique<char[]>(WRITE_BUFFER_SIZE);
    
    // Open output file
    output_stream.open(output_filename, std::ios::out | std::ios::trunc);
    
    if (!output_stream.is_open()) {
        std::cerr << "Error: Cannot create output file '" << output_filename << "'" << std::endl;
        current_result.success = false;
        current_result.error_message = "Failed to open output file";
        return;
    }
    
    // Configure stream for better performance
    output_stream.rdbuf()->pubsetbuf(write_buffer.get(), WRITE_BUFFER_SIZE);
    
    // Initialize header
    header_line = create_header_line();
    
    // Start timing
    write_timer.reset();
    
    std::cout << "CsvWriter initialized for output: " << output_filename << std::endl;
}

CsvWriter::~CsvWriter() {
    // Ensure final flush and close
    if (output_stream.is_open()) {
        flush();
        output_stream.close();
    }
    
    // Final timing
    current_result.writing_time_ms = write_timer.elapsed_ms();
    
    // Print final statistics
    std::cout << "\nCsvWriter destruction - Final Results:" << std::endl;
    current_result.print_summary();
}

bool CsvWriter::is_open() const {
    return output_stream.is_open() && output_stream.good();
}

bool CsvWriter::write_header() {
    if (!is_open()) {
        current_result.success = false;
        current_result.error_message = "Output stream is not open";
        return false;
    }
    
    // Write the header line
    if (!buffered_write(header_line + "\n")) {
        handle_write_error("Failed to write header");
        return false;
    }
    
    std::cout << "CSV header written successfully" << std::endl;
    return true;
}

bool CsvWriter::write_mbp_row(const OrderBook::MBPRow& mbp_row) {
    if (!is_open()) {
        handle_write_error("Output stream is not open");
        return false;
    }
    
    // Validate the row before writing
    if (!validate_mbp_row(mbp_row)) {
        handle_write_error("MBP row validation failed");
        return false;
    }
    
    // Format the row
    std::string formatted_row;
    if (!format_mbp_row(mbp_row, formatted_row)) {
        handle_write_error("Failed to format MBP row");
        return false;
    }
    
    // Write the formatted row
    if (!buffered_write(formatted_row + "\n")) {
        handle_write_error("Failed to write MBP row to file");
        return false;
    }
    
    // Update statistics
    current_result.rows_written++;
    current_result.bytes_written += formatted_row.length() + 1; // +1 for newline
    row_index++;
    
    return true;
}

bool CsvWriter::write_mbp_rows(const std::vector<OrderBook::MBPRow>& mbp_rows) {
    if (!is_open()) {
        handle_write_error("Output stream is not open");
        return false;
    }
    
    for (const auto& mbp_row : mbp_rows) {
        if (!write_mbp_row(mbp_row)) {
            return false; // Error already handled by write_mbp_row
        }
        
        // Periodic flush for large batches
        if (current_result.rows_written % 1000 == 0) {
            flush();
        }
    }
    
    return true;
}

bool CsvWriter::flush() {
    if (!is_open()) {
        return false;
    }
    
    output_stream.flush();
    return output_stream.good();
}

void CsvWriter::close() {
    if (output_stream.is_open()) {
        flush();
        output_stream.close();
    }
    
    // Final timing update
    current_result.writing_time_ms = write_timer.elapsed_ms();
}

void CsvWriter::reset_statistics() {
    current_result = WriteResult();
    write_timer.reset();
    row_index = 0;
}

std::string CsvWriter::create_header_line() {
    // Create exact header matching the expected output format
    std::ostringstream header;
    
    header << ",ts_recv,ts_event,rtype,publisher_id,instrument_id,action,side,depth,price,size,flags,ts_in_delta,sequence";
    
    // Add bid levels (10 levels: bid_px_00 to bid_px_09, bid_sz_00 to bid_sz_09, bid_ct_00 to bid_ct_09)
    for (int i = 0; i < Utils::MAX_DEPTH; ++i) {
        header << ",bid_px_" << std::setfill('0') << std::setw(2) << i;
        header << ",bid_sz_" << std::setfill('0') << std::setw(2) << i;
        header << ",bid_ct_" << std::setfill('0') << std::setw(2) << i;
    }
    
    // Add ask levels (10 levels: ask_px_00 to ask_px_09, ask_sz_00 to ask_sz_09, ask_ct_00 to ask_ct_09)
    for (int i = 0; i < Utils::MAX_DEPTH; ++i) {
        header << ",ask_px_" << std::setfill('0') << std::setw(2) << i;
        header << ",ask_sz_" << std::setfill('0') << std::setw(2) << i;
        header << ",ask_ct_" << std::setfill('0') << std::setw(2) << i;
    }
    
    header << ",symbol,order_id";
    
    return header.str();
}

bool CsvWriter::format_mbp_row(const OrderBook::MBPRow& mbp_row, std::string& output) {
    std::ostringstream row;
    
    try {
        // Row index (starts from 0)
        row << row_index;
        
        // Metadata fields
        row << "," << format_timestamp(mbp_row.ts_recv);
        row << "," << format_timestamp(mbp_row.ts_event);
        row << "," << mbp_row.rtype;
        row << "," << mbp_row.publisher_id;
        row << "," << mbp_row.instrument_id;
        row << "," << mbp_row.action;
        row << "," << mbp_row.side;
        row << "," << mbp_row.depth;
        row << "," << format_price(mbp_row.price);
        row << "," << format_size(mbp_row.size);
        row << "," << mbp_row.flags;
        row << "," << mbp_row.ts_in_delta;
        row << "," << mbp_row.sequence;
        
        // Bid levels (10 levels)
        for (int i = 0; i < Utils::MAX_DEPTH; ++i) {
            const auto& level = mbp_row.bid_levels[i];
            row << "," << format_price(level.price);
            row << "," << format_size(level.size);
            row << "," << format_count(level.count);
        }
        
        // Ask levels (10 levels)
        for (int i = 0; i < Utils::MAX_DEPTH; ++i) {
            const auto& level = mbp_row.ask_levels[i];
            row << "," << format_price(level.price);
            row << "," << format_size(level.size);
            row << "," << format_count(level.count);
        }
        
        // Final fields
        row << "," << mbp_row.symbol;
        row << "," << mbp_row.order_id;
        
        output = row.str();
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception formatting MBP row: " << e.what() << std::endl;
        return false;
    }
}

std::string CsvWriter::format_price(double price) const {
    if (price == 0.0) {
        return ""; // Empty string for zero prices
    }
    
    // Format with high precision and remove trailing zeros
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(9) << price;
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

std::string CsvWriter::format_size(uint64_t size) const {
    if (size == 0) {
        return "0"; // Always show 0 for sizes, not empty
    }
    
    return std::to_string(size);
}

std::string CsvWriter::format_count(uint32_t count) const {
    if (count == 0) {
        return "0"; // Always show 0 for counts, not empty
    }
    
    return std::to_string(count);
}

std::string CsvWriter::format_timestamp(const std::string& timestamp) const {
    // Return timestamp as-is (already in correct format)
    return timestamp;
}

bool CsvWriter::buffered_write(const std::string& data) {
    if (!is_open()) {
        return false;
    }
    
    output_stream << data;
    return output_stream.good();
}

bool CsvWriter::validate_mbp_row(const OrderBook::MBPRow& mbp_row) const {
    // Basic validation to ensure data integrity
    
    // Check that timestamps are not empty
    if (mbp_row.ts_recv.empty() || mbp_row.ts_event.empty()) {
        return false;
    }
    
    // Check that symbol is not empty
    if (mbp_row.symbol.empty()) {
        return false;
    }
    
    // Check that action and side are valid
    if (mbp_row.action != 'A' && mbp_row.action != 'C' && mbp_row.action != 'T' && 
        mbp_row.action != 'F' && mbp_row.action != 'R') {
        return false;
    }
    
    if (mbp_row.side != 'B' && mbp_row.side != 'A' && mbp_row.side != 'N') {
        return false;
    }
    
    // Validate price levels - prices should be non-negative
    for (int i = 0; i < Utils::MAX_DEPTH; ++i) {
        if (mbp_row.bid_levels[i].price < 0.0 || mbp_row.ask_levels[i].price < 0.0) {
            return false;
        }
        
        // If price is non-zero, size should typically be non-zero too
        if (mbp_row.bid_levels[i].price > 0.0 && mbp_row.bid_levels[i].size == 0) {
            // This might be valid in some cases, so just warn
            // return false;
        }
        
        if (mbp_row.ask_levels[i].price > 0.0 && mbp_row.ask_levels[i].size == 0) {
            // This might be valid in some cases, so just warn
            // return false;
        }
    }
    
    return true;
}

size_t CsvWriter::get_current_file_size() {
    if (!output_stream.is_open()) {
        return 0;
    }
    
    // Get current position (which represents bytes written so far)
    std::streampos current_pos = output_stream.tellp();
    return static_cast<size_t>(current_pos);
}

std::string CsvWriter::create_empty_level_string() const {
    return ",,0"; // Empty price, empty size, zero count
}

void CsvWriter::handle_write_error(const std::string& error_message) {
    current_result.success = false;
    current_result.error_message = error_message;
    
    std::cerr << "CsvWriter error: " << error_message << std::endl;
}

// WriteResult implementation
void CsvWriter::WriteResult::print_summary() const {
    std::cout << "\n=== CSV Writing Summary ===" << std::endl;
    std::cout << "Success: " << (success ? "Yes" : "No") << std::endl;
    
    if (!success && !error_message.empty()) {
        std::cout << "Error: " << error_message << std::endl;
    }
    
    std::cout << "Rows written: " << rows_written << std::endl;
    std::cout << "Bytes written: " << bytes_written << std::endl;
    std::cout << "Writing time: " << std::fixed << std::setprecision(3) 
              << writing_time_ms << " ms" << std::endl;
    
    if (writing_time_ms > 0.0) {
        std::cout << "Writing speed: " << std::fixed << std::setprecision(0) 
                  << get_rows_per_second() << " rows/sec" << std::endl;
        std::cout << "Throughput: " << std::fixed << std::setprecision(2) 
                  << get_mbps() << " MB/sec" << std::endl;
    }
    
    std::cout << "=============================" << std::endl;
}