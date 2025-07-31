#include "CsvReader.hpp"
#include "CsvWriter.hpp"
#include "OrderBook.hpp"
#include "Utils.hpp"
#include <iostream>
#include <string>
#include <memory>

/**
 * @file main.cpp
 * @brief Main entry point for MBP-10 reconstruction from MBO data
 * 
 * This program takes MBO (Market by Order) CSV data and reconstructs
 * MBP-10 (Market by Price - top 10 levels) output according to the
 * specified requirements:
 * 
 * 1. Ignores the initial 'R' (clear) action
 * 2. Handles T->F->C sequences by combining them into single T actions
 * 3. Places T actions on the side that actually changes in the book
 * 4. Ignores T actions with side 'N'
 * 
 * Usage: ./reconstruction_<name> input.csv [output.csv]
 */

/**
 * @brief Print usage information
 */
void print_usage(const std::string& program_name) {
    std::cout << "Usage: " << program_name << " <input_mbo.csv> [output_mbp.csv]" << std::endl;
    std::cout << std::endl;
    std::cout << "Arguments:" << std::endl;
    std::cout << "  input_mbo.csv   : Input MBO CSV file to process" << std::endl;
    std::cout << "  output_mbp.csv  : Output MBP-10 CSV file (optional, defaults to 'output_mbp.csv')" << std::endl;
    std::cout << std::endl;
    std::cout << "This program reconstructs MBP-10 data from MBO data with the following rules:" << std::endl;
    std::cout << "- Ignores initial 'R' (clear) actions" << std::endl;
    std::cout << "- Combines T->F->C sequences into single T actions" << std::endl;
    std::cout << "- Places T actions on the side that actually changes" << std::endl;
    std::cout << "- Ignores T actions with side 'N'" << std::endl;
    std::cout << "- Outputs exactly matching MBP-10 format" << std::endl;
}

/**
 * @brief Print program header with version and build info
 */
void print_header() {
    std::cout << "=============================================" << std::endl;
    std::cout << "   MBP-10 Order Book Reconstruction Tool    " << std::endl;
    std::cout << "   High-Performance Financial Data Processor" << std::endl;
    std::cout << "=============================================" << std::endl;
    std::cout << "Built with optimizations for:" << std::endl;
    std::cout << "- Maximum processing speed" << std::endl;
    std::cout << "- Memory efficiency" << std::endl;
    std::cout << "- Exact format compliance" << std::endl;
    std::cout << "- Robust error handling" << std::endl;
    std::cout << "=============================================" << std::endl;
    std::cout << std::endl;
}

/**
 * @brief Process MBO file and generate MBP-10 output
 * 
 * This is the main processing function that coordinates reading MBO data,
 * processing it through the order book, and writing MBP-10 output.
 * 
 * @param input_filename Path to input MBO CSV file
 * @param output_filename Path to output MBP-10 CSV file
 * @return 0 on success, non-zero on error
 */
int process_reconstruction(const std::string& input_filename, const std::string& output_filename) {
    // Overall processing timer
    Utils::Timer total_timer("Total Processing");
    
    std::cout << "Starting MBP-10 reconstruction..." << std::endl;
    std::cout << "Input file: " << input_filename << std::endl;
    std::cout << "Output file: " << output_filename << std::endl;
    std::cout << std::endl;
    
    // Initialize memory tracking
    Utils::MemoryTracker::print_memory_usage("Initial memory");
    
    // Step 1: Initialize CSV reader
    std::cout << "=== Step 1: Initializing CSV Reader ===" << std::endl;
    auto csv_reader = std::make_unique<CsvReader>(input_filename);
    
    if (!csv_reader->is_open()) {
        std::cerr << "Error: Failed to open input file: " << input_filename << std::endl;
        return 1;
    }
    
    Utils::MemoryTracker::print_memory_usage("After CSV reader initialization");
    
    // Step 2: Initialize order book
    std::cout << "\n=== Step 2: Initializing Order Book ===" << std::endl;
    auto order_book = std::make_unique<OrderBook>();
    
    Utils::MemoryTracker::print_memory_usage("After order book initialization");
    
    // Step 3: Initialize CSV writer
    std::cout << "\n=== Step 3: Initializing CSV Writer ===" << std::endl;
    auto csv_writer = std::make_unique<CsvWriter>(output_filename);
    
    if (!csv_writer->is_open()) {
        std::cerr << "Error: Failed to create output file: " << output_filename << std::endl;
        return 1;
    }
    
    // Write CSV header
    if (!csv_writer->write_header()) {
        std::cerr << "Error: Failed to write output header" << std::endl;
        return 1;
    }
    
    // Step 4: Parse input file
    std::cout << "\n=== Step 4: Parsing Input File ===" << std::endl;
    auto parse_result = csv_reader->parse_all_orders();
    
    if (!parse_result.is_successful()) {
        std::cerr << "Error: Failed to parse input file successfully" << std::endl;
        std::cerr << "Success rate: " << parse_result.get_success_rate() << "%" << std::endl;
        return 1;
    }
    
    Utils::MemoryTracker::print_memory_usage("After parsing input file");
    
    // Step 5: Process orders through order book
    std::cout << "\n=== Step 5: Processing Orders Through Order Book ===" << std::endl;
    
    size_t total_orders = parse_result.orders.size();
    size_t processed_orders = 0;
    size_t mbp_updates = 0;
    bool first_clear_ignored = false;
    
    Utils::Timer processing_timer("Order Processing");
    
    for (const auto& order : parse_result.orders) {
        // Special handling for first 'R' action as per requirements
        if (!first_clear_ignored && order.action == Utils::ACTION_CLEAR) {
            std::cout << "Ignoring initial clear action (R) as per requirements" << std::endl;
            first_clear_ignored = true;
            processed_orders++;
            continue;
        }
        
        // Process order through order book
        const OrderBook::MBPRow* mbp_row = order_book->process_order(order);
        
        // If we got an MBP update, write it to output
        if (mbp_row != nullptr) {
            if (!csv_writer->write_mbp_row(*mbp_row)) {
                std::cerr << "Error: Failed to write MBP row to output" << std::endl;
                return 1;
            }
            mbp_updates++;
        }
        
        processed_orders++;
        
        // Progress reporting
        if (processed_orders % 50000 == 0) {
            double progress = (static_cast<double>(processed_orders) / total_orders) * 100.0;
            std::cout << "Progress: " << std::fixed << std::setprecision(1) << progress 
                      << "% (" << processed_orders << "/" << total_orders << " orders, "
                      << mbp_updates << " MBP updates)" << std::endl;
            
            // Memory usage check
            Utils::MemoryTracker::print_memory_usage("Current memory");
            
            // Periodic flush
            csv_writer->flush();
        }
    }
    
    processing_timer.print_elapsed();
    
    // Step 6: Finalize output
    std::cout << "\n=== Step 6: Finalizing Output ===" << std::endl;
    csv_writer->flush();
    
    // Final statistics
    std::cout << "\n=== Final Statistics ===" << std::endl;
    std::cout << "Total orders processed: " << processed_orders << std::endl;
    std::cout << "MBP updates generated: " << mbp_updates << std::endl;
    std::cout << "Update ratio: " << std::fixed << std::setprecision(2) 
              << (static_cast<double>(mbp_updates) / processed_orders * 100.0) << "%" << std::endl;
    
    // Order book statistics
    auto [best_bid, best_ask] = order_book->get_spread();
    auto [bid_levels, ask_levels] = order_book->get_level_counts();
    
    std::cout << "Final book state:" << std::endl;
    std::cout << "  Best bid/ask: " << best_bid << " / " << best_ask << std::endl;
    std::cout << "  Active levels: " << bid_levels << " bids, " << ask_levels << " asks" << std::endl;
    std::cout << "  Total active orders: " << order_book->get_total_orders() << std::endl;
    
    // Memory usage final check
    Utils::MemoryTracker::print_memory_usage("Final memory");
    
    total_timer.print_elapsed();
    
    std::cout << "\n=== Reconstruction Completed Successfully ===" << std::endl;
    std::cout << "Output written to: " << output_filename << std::endl;
    
    return 0;
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    // Print program header
    print_header();
    
    // Parse command line arguments
    if (argc < 2) {
        std::cerr << "Error: Missing required input file argument" << std::endl;
        print_usage(argv[0]);
        return 1;
    }
    
    std::string input_filename = argv[1];
    std::string output_filename;
    
    if (argc >= 3) {
        output_filename = argv[2];
    } else {
        // Default output filename
        output_filename = "output_mbp.csv";
        std::cout << "Using default output filename: " << output_filename << std::endl;
    }
    
    // Validate input file exists
    std::ifstream test_input(input_filename);
    if (!test_input.good()) {
        std::cerr << "Error: Cannot access input file: " << input_filename << std::endl;
        return 1;
    }
    test_input.close();
    
    // Process the reconstruction
    try {
        int result = process_reconstruction(input_filename, output_filename);
        
        if (result == 0) {
            std::cout << "\n🎉 SUCCESS: MBP-10 reconstruction completed!" << std::endl;
            std::cout << "You can now compare the output with the expected mbp.csv file." << std::endl;
        } else {
            std::cout << "\n❌ FAILED: MBP-10 reconstruction encountered errors." << std::endl;
        }
        
        return result;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Fatal error: Unknown exception occurred" << std::endl;
        return 1;
    }
}