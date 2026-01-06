// common.h - 公共头文件
// 包含全局使用的头文件、类型定义、常量、工具函数等

#ifndef COMMON_H_
#define COMMON_H_

// 标准库头文件
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <map>
#include <set>
#include <queue>
#include <stack>
#include <limits>
#include <memory>
#include <functional>
#include <random>
#include <numeric>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <exception>

using namespace std;

// 类型定义
typedef vector<int> IntVector;
typedef vector<double> DoubleVector;
typedef vector<vector<int>> IntMatrix;
typedef vector<vector<double>> DoubleMatrix;

// 全局常量
constexpr double kEpsilon = 1e-6;
constexpr int kMaxIterations = 1000;
constexpr double kInfinityValue = 1e9;

// 向后兼容
constexpr double EPSILON = kEpsilon;
constexpr int MAX_ITERATIONS = kMaxIterations;
constexpr double INFINITY_VALUE = kInfinityValue;

// 通用 constexpr 函数
template<typename T>
constexpr T Min(T a, T b) {
    return (a < b) ? a : b;
}

template<typename T>
constexpr T Max(T a, T b) {
    return (a > b) ? a : b;
}

template<typename T>
constexpr T Abs(T a) {
    return (a < 0) ? -a : a;
}

// 向后兼容宏
#define MIN(a, b) ::Min(a, b)
#define MAX(a, b) ::Max(a, b)
#define ABS(a) ::Abs(a)

// 数值比较工具函数
constexpr bool IsEqual(double a, double b, double epsilon = EPSILON) {
    return Abs(a - b) < epsilon;
}

constexpr bool IsZero(double a, double epsilon = EPSILON) {
    return Abs(a) < epsilon;
}

inline double Round(double value, int precision = 6) {
    double factor = pow(10.0, precision);
    return round(value * factor) / factor;
}

// 时间工具函数
inline string GetCurrentTimestamp() {
    auto now = chrono::system_clock::now();
    auto time_t = chrono::system_clock::to_time_t(now);
    auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;

    stringstream ss;
    ss << put_time(localtime(&time_t), "%Y%m%d_%H%M%S");
    ss << "_" << setfill('0') << setw(3) << ms.count();
    return ss.str();
}

// 字符串工具函数
inline string ToString(int value) {
    return to_string(value);
}

inline string ToString(double value, int precision = 6) {
    stringstream ss;
    ss << fixed << setprecision(precision) << value;
    return ss.str();
}

// 向量工具函数
template<typename T>
inline void ClearVector(vector<T>& vec) {
    vec.clear();
    vec.shrink_to_fit();
}

// 矩阵工具函数
template<typename T>
inline void ResizeMatrix(vector<vector<T>>& matrix, size_t rows, size_t cols) {
    matrix.resize(rows);
    for (auto& row : matrix) {
        row.resize(cols);
    }
}

template<typename T>
inline void ClearMatrix(vector<vector<T>>& matrix) {
    for (auto& row : matrix) {
        row.clear();
    }
    matrix.clear();
    matrix.shrink_to_fit();
}

// 调试工具宏
#ifdef _DEBUG
#define DEBUG_PRINT(x) cout << "[DEBUG] " << x << endl
#define DEBUG_PRINT_VAR(var) cout << "[DEBUG] " << #var << " = " << var << endl
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINT_VAR(var)
#endif

// 错误处理宏
#define CHECK_CONDITION(condition, message) \
    if (!(condition)) { \
        cerr << "[错误] " << message << endl; \
        exit(1); \
    }

#define CHECK_FILE_OPEN(file, filename) \
    if (!file.is_open()) { \
        cerr << "[错误] 无法打开文件: " << filename << endl; \
        exit(1); \
    }

#endif  // COMMON_H_
