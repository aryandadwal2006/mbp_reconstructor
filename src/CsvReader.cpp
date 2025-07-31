#include "CsvReader.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>

/**
 * @file CsvReader.cpp
 * @brief Implementation of high-performance CSV reader for MBO data
 * 
 * This implementation focuses on maximum parsing speed while maintaining
 * accuracy and robust error handling. Key optimizations include:
 * - Buffered I/O for reduced system calls
 * - Pre-allocated vectors to minimize memory allocations
 * - Fast numeric parsing optimized for financial data
 * - Comprehensive error reporting with line-by-line details
 */

CsvReader::CsvReader(const std::string& csv_filename) 
    : filename(csv_filename) {
    
    // Initialize read buffer for performance
    read_buffer = std::make_unique<char[]>(BUFFER_SIZE);
    
    // Pre-allocate split buffer to avoid repeated allocations
    split_buffer.reserve(20); // Most CSV lines have ~15 columns
    
    // Open the file
    file_stream.open(filename, std::ios::in | std::ios::binary);
    
    if (!file_stream.is_open()) {
        std::cerr << "Error: Cannot open file '" << filename << "'" << std::endl;
        return;
    }
    
    // Configure stream for better performance
    file_stream.rdbuf()->pubsetbuf(read_buffer.get(), BUFFER_SIZE);
    
    std::cout << "CsvReader initialized for file: " << filename << std::endl;
    std::cout << "File size: " << get_file_size() << " bytes" << std::endl;
    std::cout << "Estimated orders: " << estimate_order_count() << std::endl;
}

CsvReader::~CsvReader() {
    if (file_stream.is_open()) {
        file_stream.close();
    }
    
    // Print final parsing statistics
    parsing_stats.print();
}

bool CsvReader::is_open() const {
    return file_stream.is_open() && file_stream.good();
}

CsvReader::ParseResult CsvReader::parse_all_orders() {
    Utils::Timer parse_timer("CSV Parsing");
    ParseResult result;
    
    if (!is_open()) {
        result.error_messages.push_back("File is not open or has errors");
        return result;
    }
    
    // Read and parse header
    std::string header_line;
    if (!std::getline(file_stream, header_line)) {
        result.error_messages.push_back("Cannot read header line");
        return result;
    }
    
    if (!parse_header(header_line)) {
        result.error_messages.push_back("Invalid CSV header format");
        return result;
    }
    
    result.total_lines_read = 1; // Header counts as one line
    
    // Parse data lines
    std::string line;
    Order current_order;
    
    while (std::getline(file_stream, line)) {
        result.total_lines_read++;
        
        // Skip empty lines
        if (line.empty() || Utils::is_empty_or_whitespace(line)) {
            continue;
        }
        
        // Parse the line
        if (parse_line_to_order(line, result.total_lines_read, current_order)) {
            // Validate the parsed order
            if (validate_order(current_order)) {
                result.orders.push_back(current_order);
                result.successful_parses++;
            } else {
                std::string error_msg = "Order validation failed at line " + 
                                      std::to_string(result.total_lines_read);
                handle_parsing_error(result.total_lines_read, error_msg, result);
            }
        } else {
            std::string error_msg = "Failed to parse line " + 
                                  std::to_string(result.total_lines_read);
            handle_parsing_error(result.total_lines_read, error_msg, result);
        }
        
        // Progress reporting for large files
        if (result.total_lines_read % 100000 == 0) {
            std::cout << "Processed " << result.total_lines_read << " lines, "
                      << "parsed " << result.successful_parses << " orders"
                      << " (" << std::fixed << std::setprecision(1) 
                      << result.get_success_rate() << "%)" << std::endl;
            
            // Memory usage check
            Utils::MemoryTracker::print_memory_usage("Current memory usage");
        }
    }
    
    result.parsing_time_ms = parse_timer.elapsed_ms();
    
    // Final statistics
    std::cout << "\nParsing completed:" << std::endl;
    result.print_summary();
    
    return result;
}

bool CsvReader::parse_header(const std::string& header_line) {
    const std::vector<std::string>& fields = split_csv_line(header_line);
    
    // Reset column indices
    column_indices = ColumnIndices();
    
    // Map column names to indices
    for (size_t i = 0; i < fields.size(); ++i) {
        const std::string& field_name = fields[i];
        
        if (field_name == "ts_recv") {
            column_indices.ts_recv = static_cast<int>(i);
        } else if (field_name == "ts_event") {
            column_indices.ts_event = static_cast<int>(i);
        } else if (field_name == "rtype") {
            column_indices.rtype = static_cast<int>(i);
        } else if (field_name == "publisher_id") {
            column_indices.publisher_id = static_cast<int>(i);
        } else if (field_name == "instrument_id") {
            column_indices.instrument_id = static_cast<int>(i);
        } else if (field_name == "action") {
            column_indices.action = static_cast<int>(i);
        } else if (field_name == "side") {
            column_indices.side = static_cast<int>(i);
        } else if (field_name == "price") {
            column_indices.price = static_cast<int>(i);
        } else if (field_name == "size") {
            column_indices.size = static_cast<int>(i);
        } else if (field_name == "channel_id") {
            column_indices.channel_id = static_cast<int>(i);
        } else if (field_name == "order_id") {
            column_indices.order_id = static_cast<int>(i);
        } else if (field_name == "flags") {
            column_indices.flags = static_cast<int>(i);
        } else if (field_name == "ts_in_delta") {
            column_indices.ts_in_delta = static_cast<int>(i);
        } else if (field_name == "sequence") {
            column_indices.sequence = static_cast<int>(i);
        } else if (field_name == "symbol") {
            column_indices.symbol = static_cast<int>(i);
        }
    }
    
    // Validate that we have all required columns
    if (!column_indices.is_valid()) {
        std::cerr << "Error: Missing required columns in CSV header" << std::endl;
        std::cerr << "Required columns: ts_recv, ts_event, action, side, order_id, sequence" << std::endl;
        return false;
    }
    
    std::cout << "Header parsed successfully. Found " << fields.size() << " columns." << std::endl;
    return validate_mbo_format();
}

bool CsvReader::parse_line_to_order(const std::string& line, size_t line_number, Order& order) {
    const std::vector<std::string>& fields = split_csv_line(line);
    
    // Check if we have enough fields
    if (fields.size() < 15) { // Minimum expected fields for MBO
        return false;
    }
    
    try {
        // Parse timestamps (required)
        if (column_indices.ts_recv >= 0 && column_indices.ts_recv < static_cast<int>(fields.size())) {
            order.ts_recv = fields[column_indices.ts_recv];
            Utils::trim_string(order.ts_recv);
        }
        
        if (column_indices.ts_event >= 0 && column_indices.ts_event < static_cast<int>(fields.size())) {
            order.ts_event = fields[column_indices.ts_event];
            Utils::trim_string(order.ts_event);
        }
        
        // Parse action (required)
        if (column_indices.action >= 0 && column_indices.action < static_cast<int>(fields.size())) {
            std::string action_str = fields[column_indices.action];
            Utils::trim_string(action_str);
            order.action = action_str.empty() ? ' ' : action_str[0];
        }
        
        // Parse side (required)
        if (column_indices.side >= 0 && column_indices.side < static_cast<int>(fields.size())) {
            std::string side_str = fields[column_indices.side];
            Utils::trim_string(side_str);
            order.side = side_str.empty() ? 'N' : side_str[0];
        }
        
        // Parse price (might be empty for some actions)
        if (column_indices.price >= 0 && column_indices.price < static_cast<int>(fields.size())) {
            std::string price_str = fields[column_indices.price];
            Utils::trim_string(price_str);
            if (!price_str.empty()) {
                double price = Utils::fast_string_to_double(price_str);
                order.set_price(price);
            } else {
                order.price_scaled = 0;
            }
        }
        
        // Parse size (might be empty for some actions)
        if (column_indices.size >= 0 && column_indices.size < static_cast<int>(fields.size())) {
            std::string size_str = fields[column_indices.size];
            Utils::trim_string(size_str);
            order.size = size_str.empty() ? 0 : Utils::fast_string_to_uint32(size_str);
        }
        
        // Parse order ID (required)
        if (column_indices.order_id >= 0 && column_indices.order_id < static_cast<int>(fields.size())) {
            std::string order_id_str = fields[column_indices.order_id];
            Utils::trim_string(order_id_str);
            order.order_id = Utils::fast_string_to_uint64(order_id_str);
        }
        
        // Parse sequence (required for ordering)
        if (column_indices.sequence >= 0 && column_indices.sequence < static_cast<int>(fields.size())) {
            std::string sequence_str = fields[column_indices.sequence];
            Utils::trim_string(sequence_str);
            order.sequence = Utils::fast_string_to_uint64(sequence_str);
        }
        
        // Parse flags (optional)
        if (column_indices.flags >= 0 && column_indices.flags < static_cast<int>(fields.size())) {
            std::string flags_str = fields[column_indices.flags];
            Utils::trim_string(flags_str);
            order.flags = flags_str.empty() ? 0 : Utils::fast_string_to_uint32(flags_str);
        }
        
        // Parse ts_in_delta (optional)
        if (column_indices.ts_in_delta >= 0 && column_indices.ts_in_delta < static_cast<int>(fields.size())) {
            std::string delta_str = fields[column_indices.ts_in_delta];
            Utils::trim_string(delta_str);
            order.ts_in_delta = delta_str.empty() ? 0 : Utils::fast_string_to_uint64(delta_str);
        }
        
        // Parse symbol (required)
        if (column_indices.symbol >= 0 && column_indices.symbol < static_cast<int>(fields.size())) {
            order.symbol = fields[column_indices.symbol];
            Utils::trim_string(order.symbol);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception parsing line " << line_number << ": " << e.what() << std::endl;
        return false;
    }
}

const std::vector<std::string>& CsvReader::split_csv_line(const std::string& line) {
    split_buffer.clear();
    
    if (line.empty()) {
        return split_buffer;
    }
    
    // Pre-allocate based on typical field count
    split_buffer.reserve(20);
    
    std::istringstream iss(line);
    std::string field;
    
    // Simple CSV parsing - assumes no quoted fields with commas
    // This is sufficient for the MBO data format we're dealing with
    while (std::getline(iss, field, ',')) {
        split_buffer.push_back(field);
    }
    
    return split_buffer;
}

bool CsvReader::validate_order(const Order& order) const {
    // Basic validation for parsed order
    
    // Action must be valid
    if (order.action != 'A' && order.action != 'C' && order.action != 'T' && 
        order.action != 'F' && order.action != 'R' && order.action != 'M') {
        return false;
    }
    
    // Side must be valid
    if (order.side != 'B' && order.side != 'A' && order.side != 'N') {
        return false;
    }
    
    // For non-clear actions, we need valid timestamps
    if (order.action != 'R') {
        if (order.ts_recv.empty() || order.ts_event.empty()) {
            return false;
        }
        
        // Order ID should be non-zero for most actions
        if (order.order_id == 0 && order.action != 'R') {
            return false;
        }
    }
    
    // Symbol should not be empty
    // Only ADD and CLEAR actions require a non-empty symbol
    if ((order.action == Utils::ACTION_ADD || order.action == Utils::ACTION_CLEAR) && order.symbol.empty()) {
       return false;
    }
    
    // For add orders, validate price and size
    if (order.action == 'A') {
        if (order.price_scaled == 0 || order.size == 0) {
            return false;
        }
        
        // Reasonable price bounds
        double price = order.get_price();
        if (price <= 0.0 || price > 1000000.0) {
            return false;
        }
        
        // Reasonable size bounds
        if (order.size > 1000000000) { // 1 billion shares max
            return false;
        }
    }
    
    return true;
}

bool CsvReader::validate_mbo_format() const {
    // Ensure we have the minimum required columns for MBO processing
    std::vector<std::string> required_columns = {
        "ts_recv", "ts_event", "action", "side", "order_id", "sequence", "symbol"
    };
    
    std::vector<int> required_indices = {
        column_indices.ts_recv, column_indices.ts_event, column_indices.action,
        column_indices.side, column_indices.order_id, column_indices.sequence,
        column_indices.symbol
    };
    
    for (size_t i = 0; i < required_columns.size(); ++i) {
        if (required_indices[i] < 0) {
            std::cerr << "Error: Missing required column '" << required_columns[i] << "'" << std::endl;
            return false;
        }
    }
    
    std::cout << "MBO format validation passed." << std::endl;
    return true;
}

void CsvReader::handle_parsing_error(size_t line_number, const std::string& error_message, 
                                   ParseResult& result) const {
    result.parsing_errors++;
    
    // Add detailed error message
    std::string detailed_error = "Line " + std::to_string(line_number) + ": " + error_message;
    result.error_messages.push_back(detailed_error);
    
    // Print error for immediate feedback (but limit to avoid spam)
    if (result.parsing_errors <= 10) {
        std::cerr << "Parsing error: " << detailed_error << std::endl;
    } else if (result.parsing_errors == 11) {
        std::cerr << "... (suppressing further parsing error messages)" << std::endl;
    }
}

size_t CsvReader::get_file_size() const {
    if (!file_stream.is_open()) {
        return 0;
    }
    
    // Save current position
    std::streampos current_pos = file_stream.tellg();
    
    // Go to end and get position
    file_stream.seekg(0, std::ios::end);
    std::streampos end_pos = file_stream.tellg();
    
    // Restore position
    file_stream.seekg(current_pos);
    
    return static_cast<size_t>(end_pos);
}

size_t CsvReader::estimate_order_count() const {
    size_t file_size = get_file_size();
    if (file_size == 0) {
        return 0;
    }
    
    // Estimate based on average line length
    // Typical MBO lines are around 150-200 characters
    size_t estimated_avg_line_length = 175;
    size_t estimated_lines = file_size / estimated_avg_line_length;
    
    // Subtract 1 for header
    return estimated_lines > 1 ? estimated_lines - 1 : 0;
}

// Additional method implementations that were missing from the header
CsvReader::ParseResult CsvReader::parse_in_chunks(size_t chunk_size, 
                                               std::function<void(const std::vector<Order>&)> callback) {
    Utils::Timer parse_timer("Chunked CSV Parsing");
    ParseResult result;
    
    if (!is_open()) {
        result.error_messages.push_back("File is not open or has errors");
        return result;
    }
    
    // Read and parse header
    std::string header_line;
    if (!std::getline(file_stream, header_line)) {
        result.error_messages.push_back("Cannot read header line");
        return result;
    }
    
    if (!parse_header(header_line)) {
        result.error_messages.push_back("Invalid CSV header format");
        return result;
    }
    
    result.total_lines_read = 1;
    std::vector<Order> chunk;
    chunk.reserve(chunk_size);
    
    std::string line;
    while (std::getline(file_stream, line)) {
        result.total_lines_read++;
        
        if (line.empty() || Utils::is_empty_or_whitespace(line)) {
            continue;
        }
        
        Order current_order;
        if (parse_line_to_order(line, result.total_lines_read, current_order)) {
            if (validate_order(current_order)) {
                chunk.push_back(current_order);
                result.successful_parses++;
                
                if (chunk.size() >= chunk_size) {
                    callback(chunk);
                    chunk.clear();
                }
            } else {
                result.parsing_errors++;
            }
        } else {
            result.parsing_errors++;
        }
    }
    
    // Process remaining orders
    if (!chunk.empty()) {
        callback(chunk);
    }
    
    result.parsing_time_ms = parse_timer.elapsed_ms();
    return result;
}

// ParseResult implementation
void CsvReader::ParseResult::print_summary() const {
    std::cout << "\n=== CSV Parsing Summary ===" << std::endl;
    std::cout << "Total lines read: " << total_lines_read << std::endl;
    std::cout << "Successfully parsed orders: " << successful_parses << std::endl;
    std::cout << "Parsing errors: " << parsing_errors << std::endl;
    std::cout << "Success rate: " << std::fixed << std::setprecision(2) 
              << get_success_rate() << "%" << std::endl;
    std::cout << "Parsing time: " << std::fixed << std::setprecision(3) 
              << parsing_time_ms << " ms" << std::endl;
    
    if (parsing_time_ms > 0.0) {
        double orders_per_sec = (successful_parses * 1000.0) / parsing_time_ms;
        std::cout << "Parsing speed: " << std::fixed << std::setprecision(0) 
                  << orders_per_sec << " orders/sec" << std::endl;
    }
    
    if (!error_messages.empty() && error_messages.size() <= 5) {
        std::cout << "\nFirst few errors:" << std::endl;
        for (const auto& error : error_messages) {
            std::cout << "  " << error << std::endl;
        }
    }
    
    std::cout << "=============================" << std::endl;
}