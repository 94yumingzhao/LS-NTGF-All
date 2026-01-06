// logger.h - 统一日志系统
//
// 功能:
// - 双向输出: 终端(stdout) + 日志文件
// - 线程安全: 支持 CPLEX 多线程求解
// - LOG() 宏自动添加时间戳
// - CPLEX 可通过 GetTeeStream() 获取输出流
//
// 用法:
//   Logger logger("logs/solve");           // 创建日志器
//   LOG("[系统] 启动");                     // 带时间戳的日志
//   cplex.setOut(g_logger->GetTeeStream()); // CPLEX 输出

#ifndef LOGGER_H_
#define LOGGER_H_

#include "tee_stream.h"
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <mutex>
#include <filesystem>
#include <cstdio>
#include <cstdarg>
#include <memory>

class Logger;
extern Logger* g_logger;  // 全局日志器指针

class Logger {
public:
    explicit Logger(const std::string& log_prefix)
        : log_file_path_(log_prefix + ".log")
    {
        // 创建日志目录
        std::filesystem::path log_path(log_file_path_);
        if (log_path.has_parent_path()) {
            std::filesystem::create_directories(log_path.parent_path());
        }

        // 打开日志文件
        log_file_.open(log_file_path_, std::ios::out | std::ios::trunc);
        if (!log_file_.is_open()) {
            std::cerr << "[Logger] 无法打开日志文件: " << log_file_path_ << std::endl;
        }

        // 创建双向输出流 (stdout + 日志文件)
        tee_stream_ = std::make_unique<TeeStream>(std::cout, log_file_);
        g_logger = this;
    }

    ~Logger() {
        Flush();
        g_logger = nullptr;
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 获取双向输出流，供 CPLEX 使用
    // 线程安全，可用于 CPLEX 多线程求解
    std::ostream& GetTeeStream() {
        return *tee_stream_;
    }

    // 写入带时间戳的日志
    void Write(const std::string& msg) {
        std::string timestamped = "[" + GetTimestamp() + "] " + msg;
        *tee_stream_ << timestamped;
        tee_stream_->flush();
    }

    // 格式化写入带时间戳的日志
    void WriteFormat(const char* fmt, ...) {
        char buffer[4096];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        std::string timestamped = "[" + GetTimestamp() + "] " + buffer;
        *tee_stream_ << timestamped;
        tee_stream_->flush();
    }

    // 写入原始消息（无时间戳，用于 CPLEX 日志等）
    void WriteRaw(const std::string& msg) {
        *tee_stream_ << msg;
        tee_stream_->flush();
    }

    // 强制刷新所有缓冲区
    void Flush() {
        if (tee_stream_) {
            tee_stream_->flush();
        }
        if (log_file_.is_open()) {
            log_file_.flush();
        }
    }

    std::string GetLogFilePath() const { return log_file_path_; }

private:
    std::string GetTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t_val = std::chrono::system_clock::to_time_t(now);

        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t_val);
#else
        localtime_r(&time_t_val, &tm_buf);
#endif

        std::stringstream ss;
        ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    std::string log_file_path_;
    std::ofstream log_file_;
    std::unique_ptr<TeeStream> tee_stream_;
};

// 日志宏

// 写入带时间戳的日志（自动换行）
#define LOG(msg) do { \
    if (g_logger) { \
        g_logger->Write(std::string(msg) + "\n"); \
    } \
} while(0)

// 写入带时间戳的日志（不自动换行）
#define LOG_NO_NL(msg) do { \
    if (g_logger) { \
        g_logger->Write(msg); \
    } \
} while(0)

// 格式化写入带时间戳的日志
#define LOG_FMT(fmt, ...) do { \
    if (g_logger) { \
        g_logger->WriteFormat(fmt, ##__VA_ARGS__); \
    } \
} while(0)

// 写入原始消息（无时间戳）
#define LOG_RAW(msg) do { \
    if (g_logger) { \
        g_logger->WriteRaw(msg); \
    } \
} while(0)

// 格式化已用时间为 [MM:SS.s] 格式
inline std::string FormatElapsed(double elapsed_sec) {
    int minutes = static_cast<int>(elapsed_sec) / 60;
    double seconds = elapsed_sec - minutes * 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "[%02d:%04.1f]", minutes, seconds);
    return std::string(buf);
}

#endif  // LOGGER_H_
