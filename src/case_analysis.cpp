// case_analysis.cpp - 案例分析模块
// 实现需求紧密度分析、订单分布统计、资源使用分析等综合分析功能

#include "case_analysis.h"
#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>

using namespace std;

// 输出案例需求紧密度分析报告
void PrintCaseAnalysis(const AllValues& values, const AllLists& lists) {
    int total_demand = 0;
    for (size_t i = 0; i < lists.final_demand.size(); ++i) {
        total_demand += lists.final_demand[i];
    }

    int total_capacity = values.machine_capacity * values.number_of_periods;
    double demand_tightness = total_capacity > 0
        ? static_cast<double>(total_demand) / total_capacity
        : 0.0;
    double avg_demand = values.number_of_periods > 0
        ? static_cast<double>(total_demand) / values.number_of_periods
        : 0.0;

    vector<int> period_sum(values.number_of_periods, 0);
    for (size_t i = 0; i < lists.period_demand.size(); ++i) {
        for (size_t t = 0; t < lists.period_demand[i].size(); ++t) {
            period_sum[t] += lists.period_demand[i][t];
        }
    }

    int peak_demand = 0;
    int peak_period = -1;
    for (int t = 0; t < values.number_of_periods; ++t) {
        if (period_sum[t] > peak_demand) {
            peak_demand = period_sum[t];
            peak_period = t + 1;
        }
    }

    double peak_avg_ratio = avg_demand > 0
        ? static_cast<double>(peak_demand) / avg_demand
        : 0.0;

    double sum = 0.0, sq_sum = 0.0;
    for (int t = 0; t < values.number_of_periods; ++t) {
        sum += period_sum[t];
        sq_sum += period_sum[t] * period_sum[t];
    }
    double mean = values.number_of_periods > 0 ? sum / values.number_of_periods : 0.0;
    double variance = values.number_of_periods > 0
        ? (sq_sum / values.number_of_periods - mean * mean)
        : 0.0;
    double cv = mean > 0 ? sqrt(variance) / mean : 0.0;

    cout << "[分析] 案例" << values.case_index << " 需求紧密度:\n";
    cout << "  需求紧密度 = " << fixed << setprecision(3) << demand_tightness << "\n";
    cout << "  总需求量 = " << total_demand << "\n";
    cout << "  总产能 = " << values.machine_capacity << " x " << values.number_of_periods
         << " = " << total_capacity << "\n";
    cout << "  平均需求 = " << setprecision(1) << avg_demand << "\n";
    cout << "  峰值需求 = " << peak_demand << " (周期" << peak_period << ")\n";
    cout << "  峰值/平均 = " << setprecision(2) << peak_avg_ratio << "\n";
    cout << "  变异系数 = " << setprecision(3) << cv << "\n";
    cout.unsetf(ios::fixed);
}

// 执行全面的案例分析
void PerformComprehensiveAnalysis(const AllValues& values,
                                   const AllLists& lists,
                                   const string& data_file_path) {
    cout << "\n[综合分析]\n";

    // 基本信息
    cout << "\n[案例概览]\n";
    cout << "  文件 = " << data_file_path << "\n";
    cout << "  编号 = " << values.case_index << "\n";
    cout << "  产能 = " << values.machine_capacity << "/周期\n";
    cout << "  订单数 = " << values.number_of_items << "\n";
    cout << "  时段数 = " << values.number_of_periods << "\n";
    cout << "  组数 = " << values.number_of_groups << "\n";
    cout << "  流程数 = " << values.number_of_flows << "\n";
    cout << "  未满足惩罚 = " << values.u_penalty << "\n";
    cout << "  延期惩罚 = " << values.b_penalty << "\n";
    cout << "  大订单阈值 = " << fixed << setprecision(0) << values.big_order_threshold << "\n";
    cout.unsetf(ios::fixed);

    // 需求分布
    cout << "\n[需求分布]\n";

    int total_demand = 0;
    for (size_t i = 0; i < lists.final_demand.size(); ++i) {
        total_demand += lists.final_demand[i];
    }

    int total_capacity = values.machine_capacity * values.number_of_periods;
    double demand_tightness = total_capacity > 0
        ? static_cast<double>(total_demand) / total_capacity
        : 0.0;
    double avg_demand = values.number_of_periods > 0
        ? static_cast<double>(total_demand) / values.number_of_periods
        : 0.0;

    vector<int> period_sum(values.number_of_periods, 0);
    for (size_t i = 0; i < lists.period_demand.size(); ++i) {
        for (size_t t = 0; t < lists.period_demand[i].size(); ++t) {
            period_sum[t] += lists.period_demand[i][t];
        }
    }

    int peak_demand = 0;
    int peak_period = -1;
    for (int t = 0; t < values.number_of_periods; ++t) {
        if (period_sum[t] > peak_demand) {
            peak_demand = period_sum[t];
            peak_period = t + 1;
        }
    }

    double peak_avg_ratio = avg_demand > 0
        ? static_cast<double>(peak_demand) / avg_demand
        : 0.0;

    double sum = 0.0, sq_sum = 0.0;
    for (int t = 0; t < values.number_of_periods; ++t) {
        sum += period_sum[t];
        sq_sum += period_sum[t] * period_sum[t];
    }
    double mean = values.number_of_periods > 0 ? sum / values.number_of_periods : 0.0;
    double variance = values.number_of_periods > 0
        ? (sq_sum / values.number_of_periods - mean * mean)
        : 0.0;
    double cv = mean > 0 ? sqrt(variance) / mean : 0.0;

    cout << "  需求紧密度 = " << fixed << setprecision(3) << demand_tightness << "\n";
    cout << "  总需求 = " << total_demand << "\n";
    cout << "  总产能 = " << total_capacity << "\n";
    cout << "  平均需求 = " << setprecision(1) << avg_demand << "\n";
    cout << "  峰值 = " << peak_demand << " (周期" << peak_period << ")\n";
    cout << "  峰值/平均 = " << setprecision(2) << peak_avg_ratio << "\n";
    cout << "  变异系数 = " << setprecision(3) << cv << "\n";
    cout << "  产能利用率 = " << setprecision(1) << (demand_tightness * 100) << "%\n";
    cout.unsetf(ios::fixed);

    // 订单分布
    cout << "\n[订单分布]\n";

    vector<int> flow_counts(values.number_of_flows, 0);
    vector<int> flow_demand(values.number_of_flows, 0);
    for (int i = 0; i < values.number_of_items; i++) {
        for (int f = 0; f < values.number_of_flows; f++) {
            if (lists.flow_flag[i][f] == 1) {
                flow_counts[f]++;
                flow_demand[f] += lists.final_demand[i];
                break;
            }
        }
    }

    cout << "  按流向:\n";
    for (int f = 0; f < values.number_of_flows; f++) {
        cout << "    流向" << (f + 1) << ": " << flow_counts[f] << "订单, " << flow_demand[f] << "需求\n";
    }

    vector<int> group_counts(values.number_of_groups, 0);
    vector<int> group_demand(values.number_of_groups, 0);
    for (int i = 0; i < values.number_of_items; i++) {
        for (int g = 0; g < values.number_of_groups; g++) {
            if (lists.group_flag[i][g] == 1) {
                group_counts[g]++;
                group_demand[g] += lists.final_demand[i];
                break;
            }
        }
    }

    cout << "  按分组:\n";
    for (int g = 0; g < values.number_of_groups; g++) {
        cout << "    分组" << (g + 1) << ": " << group_counts[g] << "订单, " << group_demand[g] << "需求\n";
    }

    // 订单统计
    cout << "\n[订单统计]\n";

    int min_demand = INT_MAX;
    int max_demand = INT_MIN;
    for (int i = 0; i < values.number_of_items; i++) {
        min_demand = min(min_demand, lists.final_demand[i]);
        max_demand = max(max_demand, lists.final_demand[i]);
    }

    double avg_order_demand = static_cast<double>(total_demand) / values.number_of_items;
    cout << "  平均需求 = " << fixed << setprecision(2) << avg_order_demand << "\n";
    cout << "  最小需求 = " << min_demand << "\n";
    cout << "  最大需求 = " << max_demand << "\n";
    cout << "  需求范围 = " << (max_demand - min_demand) << "\n";
    cout.unsetf(ios::fixed);

    // 资源需求
    cout << "\n[资源需求]\n";

    int total_production_usage = 0;
    int total_setup_usage = 0;
    double total_production_cost = 0.0;
    int total_setup_cost = 0;

    for (int i = 0; i < values.number_of_items; i++) {
        total_production_usage += lists.usage_x[i] * lists.final_demand[i];
        total_production_cost += lists.cost_x[i] * lists.final_demand[i];
    }

    for (size_t i = 0; i < lists.usage_y.size(); i++) {
        total_setup_usage += lists.usage_y[i];
        total_setup_cost += lists.cost_y[i];
    }

    cout << "  生产资源 = " << total_production_usage << "\n";
    cout << "  启动资源 = " << total_setup_usage << "\n";
    cout << "  生产成本 = " << fixed << setprecision(2) << total_production_cost << "\n";
    cout << "  启动成本 = " << total_setup_cost << "\n";
    cout << "  单订单成本 = "
         << (values.number_of_items > 0 ? total_production_cost / values.number_of_items : 0)
         << "\n";
    cout.unsetf(ios::fixed);

    // 时间约束
    cout << "\n[时间约束]\n";

    int min_ew = INT_MAX;
    int max_lw = INT_MIN;
    double avg_window_size = 0.0;
    int tight_windows = 0;

    for (int i = 0; i < values.number_of_items; i++) {
        min_ew = min(min_ew, lists.ew_x[i]);
        max_lw = max(max_lw, lists.lw_x[i]);
        int window_size = lists.lw_x[i] - lists.ew_x[i] + 1;
        avg_window_size += window_size;
        if (window_size <= 3) {
            tight_windows++;
        }
    }

    avg_window_size /= values.number_of_items;

    cout << "  最早时间 = " << min_ew << "\n";
    cout << "  最晚时间 = " << max_lw << "\n";
    cout << "  平均窗口 = " << fixed << setprecision(1) << avg_window_size << "周期\n";
    cout << "  紧窗口(<=3) = " << tight_windows << " ("
         << setprecision(1) << (100.0 * tight_windows / values.number_of_items) << "%)\n";
    cout.unsetf(ios::fixed);

    // 周期负载
    cout << "\n[周期负载]\n";

    int overcapacity_periods = 0;
    for (int t = 0; t < values.number_of_periods; t++) {
        cout << "  周期" << (t + 1) << ": " << period_sum[t];
        if (period_sum[t] > values.machine_capacity) {
            cout << " [超产能]";
            overcapacity_periods++;
        }
        cout << "\n";
    }

    cout << "  超产能时段 = " << overcapacity_periods << "/" << values.number_of_periods
         << " (" << fixed << setprecision(1)
         << (100.0 * overcapacity_periods / values.number_of_periods) << "%)\n";
    cout.unsetf(ios::fixed);
}

// 批量案例分析
void AnalyzeCase() {
    cout << "[案例分析] 批量分析功能待实现\n";
}
