// optimizer.h - 核心配置、数据结构和接口定义
// 定义生产计划优化系统的业务常量、数据结构和函数接口
// 支持多种求解算法: RF, RFO, PP-GCB (RR)

#ifndef OPTIMIZER_H_
#define OPTIMIZER_H_

#include "common.h"
#include <cstdlib>

// CPLEX 头文件
#if __has_include(<ilcplex/ilocplex.h>)
#include <ilcplex/ilocplex.h>
#else
#pragma message("Warning: CPLEX header not found. Make sure CPLEX_DIR is set correctly in CMake.")

#ifndef IL_STD
class IloEnv;
class IloModel;
class IloCplex;
class IloExpr;
class IloNumVar;
template<class T> class IloArray;
class IloNumVarArray {
public:
    IloNumVarArray() {}
    template<typename... Args> IloNumVarArray(Args&&...) {}
};
#endif

#ifndef ILOBOOL
#define ILOBOOL 0
#endif
#ifndef ILOFLOAT
#define ILOFLOAT 1
#endif
#ifndef IloInfinity
#define IloInfinity (1e20)
#endif
#endif

// ============================================================================
// 算法类型枚举
// ============================================================================
enum class AlgorithmType {
    RF,     // Relax-and-Fix: 时间窗口滚动固定
    RFO,    // RF + Fix-and-Optimize: RF + 滑动窗口优化
    RR      // PP-GCB: 三阶段分解 (原 LS-NTGF-RR)
};

// 算法名称转换
inline const char* AlgorithmName(AlgorithmType algo) {
    switch (algo) {
        case AlgorithmType::RF:  return "RF";
        case AlgorithmType::RFO: return "RFO";
        case AlgorithmType::RR:  return "RR";
        default: return "Unknown";
    }
}

// ============================================================================
// 业务常量
// ============================================================================
constexpr bool kValidate = false;

constexpr const char* kLogsDir = "logs/";
constexpr const char* kResultsDir = "results/";
constexpr const char* kDataDir = "D:/YM-Code/LS-NTGF-Data-Cap/data/";
constexpr const char* kCplexResultFile = "cplex_final_result.csv";
constexpr const char* kBigOrderResultFile = "big_order_result.csv";
constexpr const char* kStep3BigOrderResultFile = "big_order_step3_result.csv";
constexpr const char* kAlgoComparisonFile = "algorithm_comparison.csv";

// 向后兼容宏
#define LOGS_DIR kLogsDir
#define RESULTS_DIR kResultsDir
#define OUTPUT_DIR kResultsDir
#define DATA_DIR kDataDir
#define CPLEX_RESULT_FILE kCplexResultFile
#define BIG_ORDER_RESULT_FILE kBigOrderResultFile
#define STEP3_BIG_ORDER_RESULT_FILE kStep3BigOrderResultFile
#define ALGO_COMPARISON_FILE kAlgoComparisonFile

// CPLEX 求解器配置
constexpr double kDefaultCplexTimeLimit = 30.0;
constexpr double DEFAULT_CPLEX_TIME_LIMIT = kDefaultCplexTimeLimit;

// ============================================================================
// RF 算法超参数
// ============================================================================
constexpr int kRFWindowSize = 6;       // W: 窗口长度
constexpr int kRFFixStep = 1;          // S: 固定步长
constexpr int kRFMaxRetries = 3;       // R: 最大扩展重试次数
constexpr double kRFSubproblemTimeLimit = 60.0;  // 子问题时间限制

// ============================================================================
// FO 算法超参数 (用于 RFO)
// ============================================================================
constexpr int kFOWindowSize = 8;       // W_o: FO窗口长度
constexpr int kFOStep = 3;             // S_o: FO步长
constexpr int kFOMaxRounds = 2;        // H: 最大优化轮数
constexpr int kFOBoundaryBuffer = 1;   // Delta: 边界缓冲
constexpr double kFOSubproblemTimeLimit = 30.0;  // FO子问题时间限制

// ============================================================================
// 数据结构
// ============================================================================

// 算法求解结果
struct AlgoResult {
    double objective = -1.0;
    double runtime = -1.0;
    double cpu_time = -1.0;
    double gap = -1.0;
};

// RF 算法状态
struct RFState {
    vector<vector<int>> y_bar;           // 已固定的 y 值 [g][t]
    vector<vector<int>> lambda_bar;      // 已固定的 lambda 值 [g][t]
    vector<bool> period_fixed;           // 周期是否已固定 [t]
    vector<pair<int,int>> rollback_stack; // 回滚栈 (start_t, end_t)
    int current_k = 0;                   // 当前起始周期
    int current_W = kRFWindowSize;       // 当前窗口大小
    int iterations = 0;                  // 迭代次数
};

// FO 算法状态 (用于 RFO)
struct FOState {
    vector<vector<int>> y_current;       // 当前最优 y 值 [g][t]
    vector<vector<int>> lambda_current;  // 当前最优 lambda 值 [g][t]
    double current_objective;            // 当前目标值
    int rounds_completed;                // 已完成轮数
    int windows_improved;                // 改进的窗口数
};

// 大订单结构体
struct BigOrder {
    int big_order_id = -1;
    vector<int> order_ids;
    int flow_index = -1;
    int group_index = -1;
    int demand = -1;
    int early_time = -1;
    int late_time = -1;
    int production_usage = -1;
    double production_cost = -1.0;
};

// 全局参数配置
struct AllValues {
    // 算法求解结果
    AlgoResult result_cpx;
    AlgoResult result_step1;
    AlgoResult result_step2;
    AlgoResult result_step3;
    AlgoResult result_big_order;

    // 问题规模参数
    int number_of_items = -1;
    int number_of_periods = -1;
    int number_of_groups = -1;
    int number_of_flows = -1;

    // 生产参数
    int machine_capacity = -1;
    int u_penalty = 10000;
    int b_penalty = 100;

    // 求解器配置
    int case_index = 0;
    double cpx_runtime_limit = DEFAULT_CPLEX_TIME_LIMIT;
    double big_order_threshold = 1000.0;

    // CPLEX参数
    std::string cplex_workdir = "D:\\CPLEX_Temp";
    int cplex_workmem = 4096;
    int cplex_threads = 0;

    // 辅助数据
    vector<int> unmet_penalty_list;
    int original_number_of_items = -1;
};

// 数据存储结构体
struct AllLists {
    // 决策变量结果
    vector<vector<double>> small_x;
    vector<vector<double>> small_b;
    vector<double> small_u;
    vector<vector<int>> small_y;
    vector<vector<int>> small_l;
    vector<vector<double>> small_i;

    // 成本参数
    vector<double> cost_x;
    vector<int> cost_y;
    vector<double> cost_i;

    // 资源使用参数
    vector<int> usage_x;
    vector<int> usage_y;

    // 时间窗口约束
    vector<int> ew_x;
    vector<int> lw_x;

    // 订单属性标记
    vector<vector<int>> flow_flag;
    vector<vector<int>> group_flag;

    // 需求数据
    vector<vector<int>> period_demand;
    vector<int> final_demand;

    // 临时变量
    vector<vector<int>> y_temp;
    vector<vector<int>> l_temp;

    // 大订单相关数据
    vector<BigOrder> big_order_list;
    vector<int> big_ew_x;
    vector<int> big_lw_x;
    vector<vector<int>> big_flow_flag;
    vector<vector<int>> big_group_flag;
    vector<int> big_final_demand;
    vector<int> usage_big_x;
    vector<double> cost_big_x;

    // 原始订单数据备份
    vector<int> original_ew_x;
    vector<int> original_lw_x;
    vector<vector<int>> original_flow_flag;
    vector<vector<int>> original_group_flag;
    vector<int> original_final_demand;
    vector<int> original_usage_x;
    vector<double> original_cost_x;
    vector<vector<int>> original_period_demand;
};

// ============================================================================
// 工具函数
// ============================================================================
void SplitString(const string& input, vector<string>& output, const string& delimiter);
void ParseCommaSeparatedValues(const string& line, vector<double>& values, int data_type);

// ============================================================================
// 数据输入/输出
// ============================================================================
void ReadData(AllValues& values, AllLists& lists, const string& path);
void OutputDecisionVarsCSV(const string& filename,
                           const AllValues& values,
                           const AllLists& lists,
                           IloCplex& cplex,
                           IloArray<IloNumVarArray>& X,
                           IloArray<IloNumVarArray>& Y,
                           IloArray<IloNumVarArray>& L,
                           IloArray<IloNumVarArray>& I,
                           IloArray<IloNumVarArray>& B,
                           IloNumVarArray& U,
                           bool is_step1, bool is_step2, bool is_step3,
                           bool is_big_order, bool is_split_order, int precision);
void OutputDecisionVarsCSVCompat(const string& filename,
                                 const AllValues& values,
                                 const AllLists& lists,
                                 IloCplex& cplex,
                                 IloArray<IloNumVarArray>& X,
                                 IloArray<IloNumVarArray>& Y,
                                 IloArray<IloNumVarArray>& L,
                                 IloArray<IloNumVarArray>& I,
                                 IloArray<IloNumVarArray>& B,
                                 IloNumVarArray& U,
                                 bool is_step1, bool is_step2, bool is_step3,
                                 bool is_big_order, bool is_split_order,
                                 int precision);

// ============================================================================
// 核心求解算法
// ============================================================================

// CPLEX 直接求解
void SolveCplexLotSizing(AllValues& values, AllLists& lists, const string& output_dir = "");

// PP-GCB 三阶段分解 (用于 RR 算法)
void SolveStep1(AllValues& values, AllLists& lists);
void SolveStep2(AllValues& values, AllLists& lists);
void SolveStep3(AllValues& values, AllLists& lists);

// RF (Relax-and-Fix) 算法
void SolveRF(AllValues& values, AllLists& lists);

// RFO (RF + Fix-and-Optimize) 算法
void SolveRFO(AllValues& values, AllLists& lists);

// ============================================================================
// 大订单处理
// ============================================================================
void UpdateBigOrder(AllValues& values, AllLists& lists);
void UpdateBigOrderFG(AllValues& values, AllLists& lists);
void SolveBigOrder(AllValues& values, AllLists& lists);
void SplitBigOrderResults(AllValues& values, AllLists& lists,
                          IloArray<IloNumVarArray>& X,
                          IloArray<IloNumVarArray>& B,
                          IloArray<IloNumVarArray>& Y,
                          IloArray<IloNumVarArray>& L,
                          IloArray<IloNumVarArray>& I,
                          IloCplex& cplex);
void RestoreOriginalOrderData(AllValues& values, AllLists& lists);

// ============================================================================
// 模型验证
// ============================================================================
void ValidateModel(AllValues& values, AllLists& lists, const string& solution_file);
void ValidateModelBigOrder(AllValues& values, AllLists& lists, const string& solution_file);

#endif  // OPTIMIZER_H_
