/**
 * system_monitor.cpp - A system resource monitoring utility
 * 
 * Monitors CPU, memory, and disk usage on Linux systems.
 * Compile with: g++ -std=c++17 system_monitor.cpp -o system_monitor
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <filesystem>
#include <algorithm>
#include <numeric>
#include <cstdlib>

namespace fs = std::filesystem;

class SystemMonitor {
private:
    struct CPUStats {
        long user;
        long nice;
        long system;
        long idle;
        long iowait;
        long irq;
        long softirq;
        long steal;
        
        long total() const {
            return user + nice + system + idle + iowait + irq + softirq + steal;
        }
        
        long active() const {
            return user + nice + system + irq + softirq + steal;
        }
    };
    
    struct MemoryStats {
        long total;
        long free;
        long available;
        long buffers;
        long cached;
        
        double used_percent() const {
            return 100.0 * (1.0 - static_cast<double>(available) / total);
        }
    };
    
    CPUStats prev_cpu_stats;
    bool first_cpu_reading = true;
    std::string log_file;
    bool log_to_file = false;

public:
    SystemMonitor(const std::string& log_file = "") : log_file(log_file) {
        if (!log_file.empty()) {
            log_to_file = true;
            std::ofstream ofs(log_file, std::ios::out | std::ios::trunc);
            ofs << "Timestamp,CPU Usage (%),Memory Usage (%),Disk Usage (%)\n";
        }
    }
    
    CPUStats get_cpu_stats() {
        std::ifstream proc_stat("/proc/stat");
        std::string line;
        CPUStats stats{0, 0, 0, 0, 0, 0, 0, 0};
        
        if (std::getline(proc_stat, line)) {
            std::istringstream iss(line);
            std::string cpu_label;
            iss >> cpu_label >> stats.user >> stats.nice >> stats.system >> 
                stats.idle >> stats.iowait >> stats.irq >> stats.softirq >> stats.steal;
        }
        
        return stats;
    }
    
    double get_cpu_usage() {
        CPUStats current = get_cpu_stats();
        double cpu_usage = 0.0;
        
        if (!first_cpu_reading) {
            long prev_total = prev_cpu_stats.total();
            long current_total = current.total();
            long total_delta = current_total - prev_total;
            
            long prev_active = prev_cpu_stats.active();
            long current_active = current.active();
            long active_delta = current_active - prev_active;
            
            if (total_delta > 0) {
                cpu_usage = 100.0 * (static_cast<double>(active_delta) / total_delta);
            }
        } else {
            first_cpu_reading = false;
        }
        
        prev_cpu_stats = current;
        return cpu_usage;
    }
    
    MemoryStats get_memory_stats() {
        std::ifstream proc_meminfo("/proc/meminfo");
        std::string line;
        MemoryStats stats{0, 0, 0, 0, 0};
        
        while (std::getline(proc_meminfo, line)) {
            std::istringstream iss(line);
            std::string key;
            long value;
            std::string unit;
            
            iss >> key >> value >> unit;
            
            if (key == "MemTotal:") stats.total = value;
            else if (key == "MemFree:") stats.free = value;
            else if (key == "MemAvailable:") stats.available = value;
            else if (key == "Buffers:") stats.buffers = value;
            else if (key == "Cached:") stats.cached = value;
        }
        
        return stats;
    }
    
    double get_disk_usage(const std::string& path = "/") {
        try {
            fs::space_info space = fs::space(path);
            double total = static_cast<double>(space.capacity);
            double free = static_cast<double>(space.free);
            
            if (total > 0) {
                return 100.0 * (1.0 - free / total);
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Error getting disk space: " << e.what() << std::endl;
        }
        
        return 0.0;
    }
    
    std::string get_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
    
    void monitor(int interval_seconds = 1, int count = -1) {
        int iterations = 0;
        
        while (count == -1 || iterations < count) {
            double cpu_usage = get_cpu_usage();
            MemoryStats mem_stats = get_memory_stats();
            double disk_usage = get_disk_usage();
            std::string timestamp = get_timestamp();
            
            std::cout << timestamp << " - ";
            std::cout << "CPU: " << std::fixed << std::setprecision(1) << cpu_usage << "%, ";
            std::cout << "Memory: " << std::fixed << std::setprecision(1) << mem_stats.used_percent() << "%, ";
            std::cout << "Disk: " << std::fixed << std::setprecision(1) << disk_usage << "%" << std::endl;
            
            if (log_to_file) {
                std::ofstream ofs(log_file, std::ios::app);
                ofs << timestamp << "," << cpu_usage << "," << mem_stats.used_percent() << "," << disk_usage << "\n";
            }
            
            iterations++;
            if (count == -1 || iterations < count) {
                std::this_thread::sleep_for(std::chrono::seconds(interval_seconds));
            }
        }
    }
};

void print_help() {
    std::cout << "Usage: system_monitor [OPTIONS]\n"
              << "Options:\n"
              << "  -h, --help         Show this help message\n"
              << "  -i, --interval N   Set monitoring interval in seconds (default: 1)\n"
              << "  -c, --count N      Run for N iterations (default: infinite)\n"
              << "  -l, --log FILE     Log results to CSV file\n";
}

int main(int argc, char* argv[]) {
    int interval = 1;
    int count = -1;
    std::string log_file;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_help();
            return 0;
        } else if (arg == "-i" || arg == "--interval") {
            if (i + 1 < argc) {
                interval = std::stoi(argv[++i]);
            }
        } else if (arg == "-c" || arg == "--count") {
            if (i + 1 < argc) {
                count = std::stoi(argv[++i]);
            }
        } else if (arg == "-l" || arg == "--log") {
            if (i + 1 < argc) {
                log_file = argv[++i];
            }
        }
    }
    
    try {
        SystemMonitor monitor(log_file);
        monitor.monitor(interval, count);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
