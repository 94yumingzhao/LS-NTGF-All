// main.cpp - 生产计划优化系统主程序 (统一版本)
//
// 支持多种求解算法:
// - RF:  Relax-and-Fix 时间窗口滚动固定
// - RFO: RF + Fix-and-Optimize 滑动窗口优化
// - RR:  PP-GCB 三阶段分解算法
//
// 用法: program --algo=RF|RFO|RR [options] [data_file]

#include "optimizer.h"
#include "logger.h"
#include "case_analysis.h"
#include "common.h"
#include <ctime>
#include <string>
#include <iomanip>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#define mkdir(dir) _mkdir(dir)
#else
#include <unistd.h>
#include <sys/stat.h>
#define mkdir(dir) mkdir(dir, 0755)
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif

#include <filesystem>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace fs = std::filesystem;

using namespace std;

// ============================================================================
// 命令行参数结构
// ============================================================================
struct CommandLineArgs {
    AlgorithmType algorithm = AlgorithmType::RF;  // 默认使用 RF
    string input_file = "";
    string output_dir = "./results";
    string log_file = "";
    double time_limit = 30.0;
    int u_penalty = 10000;
    int b_penalty = 100;
    double big_order_threshold = 1000.0;
    bool enable_merge = true;   // 是否启用订单合并
    bool show_help = false;
    // CPLEX parameters
    string cplex_workdir = "D:\\CPLEX_Temp";
    int cplex_workmem = 4096;
    int cplex_threads = 0;
};

// ============================================================================
// 帮助信息
// ============================================================================
void PrintUsage(const char* program) {
    cout << "Usage: " << program << " [options] [data_file]\n";
    cout << "\nAlgorithm Selection:\n";
    cout << "  --algo=RF           Relax-and-Fix (default)\n";
    cout << "  --algo=RFO          RF + Fix-and-Optimize\n";
    cout << "  --algo=RR           PP-GCB 3-stage decomposition\n";
    cout << "\nOptions:\n";
    cout << "  -f, --file <path>       Input data file\n";
    cout << "  -o, --output <dir>      Output directory (default: ./results)\n";
    cout << "  -l, --log <file>        Log file path (default: ./logs/solve.log)\n";
    cout << "  -t, --time <seconds>    CPLEX time limit (default: 30)\n";
    cout << "  --u-penalty <int>       Unmet demand penalty (default: 10000)\n";
    cout << "  --b-penalty <int>       Backorder penalty (default: 100)\n";
    cout << "  --threshold <double>    Big order threshold (default: 1000)\n";
    cout << "  --no-merge              Disable order merging\n";
    cout << "  --cplex-workdir <path>  CPLEX work directory (default: D:\\CPLEX_Temp)\n";
    cout << "  --cplex-workmem <MB>    CPLEX work memory limit (default: 4096)\n";
    cout << "  --cplex-threads <num>   CPLEX thread count, 0=auto (default: 0)\n";
    cout << "  -h, --help              Show this help message\n";
    cout << "\nExamples:\n";
    cout << "  " << program << " --algo=RF data.csv\n";
    cout << "  " << program << " --algo=RFO -t 60 data.csv\n";
    cout << "  " << program << " --algo=RR --output=./out data.csv\n";
}

// ============================================================================
// 解析命令行参数
// ============================================================================
bool ParseArgs(int argc, char* argv[], CommandLineArgs& args) {
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            args.show_help = true;
            return true;
        } else if (arg.rfind("--algo=", 0) == 0) {
            string algo_str = arg.substr(7);
            if (algo_str == "RF" || algo_str == "rf") {
                args.algorithm = AlgorithmType::RF;
            } else if (algo_str == "RFO" || algo_str == "rfo") {
                args.algorithm = AlgorithmType::RFO;
            } else if (algo_str == "RR" || algo_str == "rr") {
                args.algorithm = AlgorithmType::RR;
            } else {
                cerr << "Unknown algorithm: " << algo_str << "\n";
                cerr << "Valid options: RF, RFO, RR\n";
                return false;
            }
        } else if ((arg == "-f" || arg == "--file") && i + 1 < argc) {
            args.input_file = argv[++i];
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            args.output_dir = argv[++i];
        } else if ((arg == "-l" || arg == "--log") && i + 1 < argc) {
            args.log_file = argv[++i];
        } else if ((arg == "-t" || arg == "--time") && i + 1 < argc) {
            args.time_limit = atof(argv[++i]);
        } else if (arg == "--u-penalty" && i + 1 < argc) {
            args.u_penalty = atoi(argv[++i]);
        } else if (arg == "--b-penalty" && i + 1 < argc) {
            args.b_penalty = atoi(argv[++i]);
        } else if (arg == "--threshold" && i + 1 < argc) {
            args.big_order_threshold = atof(argv[++i]);
        } else if (arg == "--no-merge") {
            args.enable_merge = false;
        } else if (arg == "--cplex-workdir" && i + 1 < argc) {
            args.cplex_workdir = argv[++i];
        } else if (arg == "--cplex-workmem" && i + 1 < argc) {
            args.cplex_workmem = atoi(argv[++i]);
        } else if (arg == "--cplex-threads" && i + 1 < argc) {
            args.cplex_threads = atoi(argv[++i]);
        } else if (arg[0] != '-' && args.input_file.empty()) {
            // 位置参数作为输入文件
            args.input_file = arg;
        } else {
            cerr << "Unknown option: " << arg << "\n";
            return false;
        }
    }
    return true;
}

// ============================================================================
// 查找最新 CSV 文件
// ============================================================================
string FindLatestCSVFile(const string& directory) {
    string latest_file = "";

    try {
        const fs::path dir_path(directory);
        if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
            return "";
        }

        auto latest_time = fs::file_time_type::min();

        for (const auto& entry : fs::directory_iterator(dir_path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".csv") {
                auto file_time = fs::last_write_time(entry);
                if (file_time > latest_time) {
                    latest_time = file_time;
                    latest_file = entry.path().string();
                }
            }
        }
    } catch (const std::exception&) {
        return "";
    }

    return latest_file;
}

// ============================================================================
// 输出状态码 (供 GUI 解析)
// ============================================================================
void EmitStatus(const string& status) {
    cout << status << endl;
    cout.flush();
}

// ============================================================================
// 主程序
// ============================================================================
int main(int argc, char* argv[]) {
    // 解析命令行参数
    CommandLineArgs args;
    if (!ParseArgs(argc, argv, args)) {
        PrintUsage(argv[0]);
        return 1;
    }

    if (args.show_help) {
        PrintUsage(argv[0]);
        return 0;
    }

    // 确定数据文件路径
    string data_path = args.input_file;
    if (data_path.empty()) {
        string data_dir = "D:/YM-Code/LS-NTGF-Data-Cap/data/";
        data_path = FindLatestCSVFile(data_dir);

        if (data_path.empty()) {
            data_path = "D:/YM-Code/LS-NTGF-Data-Cap/data/60_N100_T30_F5_G5_1_20251117_032658.csv";
        }
    }

    // 创建输出目录
    string output_dir = args.output_dir;
    string logs_dir = "./logs";

    try {
        fs::create_directories(output_dir);
        fs::create_directories(logs_dir);
    } catch (const std::exception& e) {
        cerr << "[ERROR] Cannot create directories: " << e.what() << "\n";
        return 1;
    }

    // 确定日志文件路径
    string log_file_path = args.log_file;
    if (log_file_path.empty()) {
        log_file_path = logs_dir + "/solve_" + AlgorithmName(args.algorithm);
    }

    // 初始化日志系统
    Logger logger(log_file_path);

    LOG("[系统] 生产计划优化器启动 (统一版本)");
    LOG_FMT("[系统] 算法: %s\n", AlgorithmName(args.algorithm));
    LOG_FMT("[系统] 输入文件: %s\n", data_path.c_str());
    LOG_FMT("[系统] 输出目录: %s\n", output_dir.c_str());
    LOG_FMT("[系统] 时间限制: %.1f秒\n", args.time_limit);

    LOG("\n========================================");
    LOG("  生产计划优化器 v2.0 (统一版本)");
    LOG_FMT("  算法: %s\n", AlgorithmName(args.algorithm));
    LOG("========================================\n");

    AllValues values;
    AllLists lists;

    // 读取数据
    LOG_FMT("[读取] 加载数据: %s\n", data_path.c_str());
    ReadData(values, lists, data_path);

    if (values.number_of_items <= 0) {
        LOG("[错误] 数据加载失败");
        return 1;
    }

    LOG_FMT("[数据] 订单=%d 周期=%d 流向=%d 分组=%d\n",
            values.number_of_items, values.number_of_periods,
            values.number_of_flows, values.number_of_groups);

    // GUI 状态码: 数据加载完成
    EmitStatus("[LOAD:OK:" + to_string(values.number_of_items) + ":" +
               to_string(values.number_of_periods) + ":" +
               to_string(values.number_of_flows) + ":" +
               to_string(values.number_of_groups) + "]");

    // 应用参数
    values.cpx_runtime_limit = args.time_limit;
    values.u_penalty = args.u_penalty;
    values.b_penalty = args.b_penalty;
    values.big_order_threshold = args.big_order_threshold;
    values.cplex_workdir = args.cplex_workdir;
    values.cplex_workmem = args.cplex_workmem;
    values.cplex_threads = args.cplex_threads;

    clock_t case_start = clock();

    // 大订单合并 (可选)
    int original_items = values.number_of_items;
    if (args.enable_merge) {
        LOG("[合并] 合并订单（流向-分组策略）...");
        UpdateBigOrderFG(values, lists);
        LOG_FMT("[合并] 完成: %d -> %d 订单\n", original_items, values.number_of_items);
        EmitStatus("[MERGE:" + to_string(original_items) + ":" +
                   to_string(values.number_of_items) + "]");
    } else {
        LOG("[合并] 跳过订单合并");
        EmitStatus("[MERGE:SKIP]");
    }

    // 根据选择的算法执行求解
    LOG_FMT("[求解] 执行 %s 算法...\n", AlgorithmName(args.algorithm));

    switch (args.algorithm) {
        case AlgorithmType::RF:
            EmitStatus("[STAGE:1:START]");
            SolveRF(values, lists);
            EmitStatus("[STAGE:1:DONE:" +
                       to_string(values.result_step1.objective) + ":" +
                       to_string(values.result_step1.runtime) + ":" +
                       to_string(values.result_step1.gap) + "]");
            break;

        case AlgorithmType::RFO:
            EmitStatus("[STAGE:1:START]");
            SolveRFO(values, lists);
            EmitStatus("[STAGE:1:DONE:" +
                       to_string(values.result_step1.objective) + ":" +
                       to_string(values.result_step1.runtime) + ":" +
                       to_string(values.result_step1.gap) + "]");
            break;

        case AlgorithmType::RR:
            // PP-GCB 三阶段求解
            EmitStatus("[STAGE:1:START]");
            SolveStep1(values, lists);
            EmitStatus("[STAGE:1:DONE:" +
                       to_string(values.result_step1.objective) + ":" +
                       to_string(values.result_step1.runtime) + ":" +
                       to_string(values.result_step1.gap) + "]");

            EmitStatus("[STAGE:2:START]");
            SolveStep2(values, lists);
            EmitStatus("[STAGE:2:DONE:" +
                       to_string(values.result_step2.objective) + ":" +
                       to_string(values.result_step2.runtime) + ":" +
                       to_string(values.result_step2.gap) + "]");

            EmitStatus("[STAGE:3:START]");
            SolveStep3(values, lists);
            EmitStatus("[STAGE:3:DONE:" +
                       to_string(values.result_step3.objective) + ":" +
                       to_string(values.result_step3.runtime) + ":" +
                       to_string(values.result_step3.gap) + "]");
            break;
    }

    // 计算总耗时
    clock_t case_end = clock();
    double total_duration = static_cast<double>(case_end - case_start) / CLOCKS_PER_SEC;

    // 获取最终结果
    double final_objective = -1.0;
    double final_runtime = -1.0;
    double final_gap = -1.0;

    switch (args.algorithm) {
        case AlgorithmType::RF:
        case AlgorithmType::RFO:
            final_objective = values.result_step1.objective;
            final_runtime = values.result_step1.runtime;
            final_gap = values.result_step1.gap;
            break;
        case AlgorithmType::RR:
            final_objective = values.result_step3.objective;
            final_runtime = values.result_step1.runtime + values.result_step2.runtime
                          + values.result_step3.runtime;
            final_gap = values.result_step3.gap;
            break;
    }

    // 输出结果
    LOG("\n========================================");
    LOG("  求解结果汇总");
    LOG("========================================");
    LOG_FMT("  算法:     %s\n", AlgorithmName(args.algorithm));
    LOG_FMT("  目标值:   %.2f\n", final_objective);
    LOG_FMT("  求解时间: %.3fs\n", final_runtime);
    LOG_FMT("  总耗时:   %.3fs\n", total_duration);
    LOG_FMT("  Gap:      %.4f\n", final_gap);
    LOG("========================================");

    // 保存结果
    string timestamp = GetCurrentTimestamp();
    string algo_name_lower = AlgorithmName(args.algorithm);
    for (char& c : algo_name_lower) c = tolower(c);

    string result_file = output_dir + "/" + algo_name_lower + "_result_" + timestamp + ".csv";

    ofstream fout(result_file);
    if (!fout) {
        LOG("[错误] 无法写入结果文件");
        return 1;
    }

    fout << fixed << setprecision(6);
    fout << "Algorithm,Objective,WallTime(s),CPUTime(s),Gap\n";

    if (args.algorithm == AlgorithmType::RR) {
        // RR 输出三阶段详细结果
        fout << "Step1," << values.result_step1.objective << ","
             << values.result_step1.runtime << "," << values.result_step1.cpu_time << ","
             << values.result_step1.gap << "\n";
        fout << "Step2," << values.result_step2.objective << ","
             << values.result_step2.runtime << "," << values.result_step2.cpu_time << ","
             << values.result_step2.gap << "\n";
        fout << "Step3," << values.result_step3.objective << ","
             << values.result_step3.runtime << "," << values.result_step3.cpu_time << ","
             << values.result_step3.gap << "\n";
    } else {
        // RF/RFO 输出单行结果
        fout << AlgorithmName(args.algorithm) << "," << final_objective << ","
             << final_runtime << "," << values.result_step1.cpu_time << ","
             << final_gap << "\n";
    }
    fout.close();

    LOG_FMT("[保存] 结果已保存: %s\n", result_file.c_str());
    LOG_FMT("[完成] 总耗时=%.3fs\n", total_duration);
    LOG("[系统] 程序正常退出");

    // GUI 状态码: 完成
    EmitStatus("[DONE:SUCCESS]");

    return 0;
}
