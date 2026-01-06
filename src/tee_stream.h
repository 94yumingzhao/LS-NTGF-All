// tee_stream.h - 双向输出流（同时输出到终端和文件）
//
// 主要用于捕获 CPLEX 求解器日志，使其同时显示在终端/GUI 和写入日志文件。
// 线程安全设计，支持 CPLEX 多线程求解。

#ifndef TEE_STREAM_H_
#define TEE_STREAM_H_

#include <iostream>
#include <streambuf>
#include <mutex>

// 线程安全的双向流缓冲区
// 所有写入操作都会同时输出到两个目标流
class TeeStreambuf : public std::streambuf {
public:
    TeeStreambuf(std::streambuf* buf1, std::streambuf* buf2)
        : buf1_(buf1), buf2_(buf2) {}

    TeeStreambuf(const TeeStreambuf&) = delete;
    TeeStreambuf& operator=(const TeeStreambuf&) = delete;

protected:
    // 单字符写入
    virtual int overflow(int c) override {
        if (c == EOF) {
            return !EOF;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        int r1 = buf1_->sputc(static_cast<char>(c));
        int r2 = buf2_->sputc(static_cast<char>(c));
        return (r1 == EOF || r2 == EOF) ? EOF : c;
    }

    // 批量写入（CPLEX 主要使用这个方法输出日志）
    virtual std::streamsize xsputn(const char* s, std::streamsize n) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::streamsize r1 = buf1_->sputn(s, n);
        std::streamsize r2 = buf2_->sputn(s, n);
        return (r1 < n || r2 < n) ? std::min(r1, r2) : n;
    }

    // 同步/刷新缓冲区
    virtual int sync() override {
        std::lock_guard<std::mutex> lock(mutex_);
        int r1 = buf1_->pubsync();
        int r2 = buf2_->pubsync();
        return (r1 == 0 && r2 == 0) ? 0 : -1;
    }

private:
    std::streambuf* buf1_;  // 第一个目标（通常是 stdout）
    std::streambuf* buf2_;  // 第二个目标（通常是日志文件）
    mutable std::mutex mutex_;  // 保护并发写入
};

// 双向输出流
// 用法: TeeStream tee(std::cout, log_file); cplex.setOut(tee);
class TeeStream : public std::ostream {
public:
    TeeStream(std::ostream& os1, std::ostream& os2)
        : std::ostream(&tee_buf_)
        , tee_buf_(os1.rdbuf(), os2.rdbuf()) {}

    TeeStream(const TeeStream&) = delete;
    TeeStream& operator=(const TeeStream&) = delete;

private:
    TeeStreambuf tee_buf_;
};

#endif  // TEE_STREAM_H_
