// big_order.cpp - 大订单处理模块
// 包含大订单的合并、求解、拆分、验证等功能

#include "optimizer.h"
#include <map>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <climits>

// 基于相似性标准将小订单合并为大订单
// 合并键: (流向, 分组, 最早时间, 最晚时间) 四元组
void UpdateBigOrder(AllValues& values, AllLists& lists) {
    cout << "\n[大订单] 开始订单合并...\n";

    values.original_number_of_items = values.number_of_items;
    cout << "原始订单数: " << values.original_number_of_items << "\n";

    lists.big_order_list.clear();
    lists.big_ew_x.clear();
    lists.big_lw_x.clear();
    lists.big_flow_flag.clear();
    lists.big_group_flag.clear();
    lists.big_final_demand.clear();
    lists.usage_big_x.clear();
    lists.cost_big_x.clear();

    struct OrderKey {
        int flow_idx;
        int group_idx;
        int early_time;
        int late_time;

        bool operator<(const OrderKey& other) const {
            if (flow_idx != other.flow_idx) return flow_idx < other.flow_idx;
            if (group_idx != other.group_idx) return group_idx < other.group_idx;
            if (early_time != other.early_time) return early_time < other.early_time;
            return late_time < other.late_time;
        }
    };

    map<OrderKey, vector<int>> order_groups;

    for (int i = 0; i < values.number_of_items; i++) {
        if (i >= static_cast<int>(lists.ew_x.size()) ||
            i >= static_cast<int>(lists.lw_x.size()) ||
            i >= static_cast<int>(lists.final_demand.size())) {
            cout << "[警告] 订单 " << i << " 不完整，跳过\n";
            continue;
        }

        int flow_idx = -1;
        for (int f = 0; f < values.number_of_flows; f++) {
            if (f < static_cast<int>(lists.flow_flag[i].size()) && lists.flow_flag[i][f] == 1) {
                flow_idx = f;
                break;
            }
        }

        int group_idx = -1;
        for (int g = 0; g < values.number_of_groups; g++) {
            if (g < static_cast<int>(lists.group_flag[i].size()) && lists.group_flag[i][g] == 1) {
                group_idx = g;
                break;
            }
        }

        if (flow_idx < 0 || group_idx < 0) {
            cout << "[警告] 订单 " << i << " 流向/分组无效，跳过\n";
            continue;
        }

        OrderKey key = {flow_idx, group_idx, lists.ew_x[i], lists.lw_x[i]};
        order_groups[key].push_back(i);
    }

    cout << "分组数: " << order_groups.size() << "\n";

    int big_order_id = 0;
    for (const auto& group : order_groups) {
        BigOrder big_order;
        big_order.big_order_id = big_order_id++;
        big_order.order_ids = group.second;
        big_order.flow_index = group.first.flow_idx;
        big_order.group_index = group.first.group_idx;
        big_order.early_time = group.first.early_time;
        big_order.late_time = group.first.late_time;

        int total_demand = 0;
        double total_cost = 0.0;
        int max_usage = 0;

        for (int order_id : group.second) {
            total_demand += lists.final_demand[order_id];
            total_cost += lists.cost_x[order_id] * lists.final_demand[order_id];
            max_usage = max(max_usage, lists.usage_x[order_id]);
        }

        big_order.demand = total_demand;
        big_order.production_usage = max_usage;
        big_order.production_cost = (total_demand > 0) ? total_cost / total_demand : 0.0;

        lists.big_order_list.push_back(big_order);
        lists.big_ew_x.push_back(big_order.early_time);
        lists.big_lw_x.push_back(big_order.late_time);
        lists.big_final_demand.push_back(big_order.demand);
        lists.usage_big_x.push_back(big_order.production_usage);
        lists.cost_big_x.push_back(big_order.production_cost);

        cout << "  大订单 " << big_order.big_order_id
             << ": " << group.second.size() << " 订单"
             << " (流向=" << big_order.flow_index
             << " 分组=" << big_order.group_index
             << " 需求=" << big_order.demand << ")\n";
    }

    lists.big_flow_flag.resize(big_order_id);
    lists.big_group_flag.resize(big_order_id);

    for (int i = 0; i < big_order_id; i++) {
        lists.big_flow_flag[i].resize(values.number_of_flows, 0);
        lists.big_group_flag[i].resize(values.number_of_groups, 0);

        if (lists.big_order_list[i].flow_index >= 0) {
            lists.big_flow_flag[i][lists.big_order_list[i].flow_index] = 1;
        }
        if (lists.big_order_list[i].group_index >= 0) {
            lists.big_group_flag[i][lists.big_order_list[i].group_index] = 1;
        }
    }

    // 备份原始数据
    lists.original_ew_x = lists.ew_x;
    lists.original_lw_x = lists.lw_x;
    lists.original_flow_flag = lists.flow_flag;
    lists.original_group_flag = lists.group_flag;
    lists.original_final_demand = lists.final_demand;
    lists.original_usage_x = lists.usage_x;
    lists.original_cost_x = lists.cost_x;
    lists.original_period_demand = lists.period_demand;

    // 替换为大订单数据
    values.number_of_items = big_order_id;
    lists.ew_x = lists.big_ew_x;
    lists.lw_x = lists.big_lw_x;
    lists.flow_flag = lists.big_flow_flag;
    lists.group_flag = lists.big_group_flag;
    lists.final_demand = lists.big_final_demand;
    lists.usage_x = lists.usage_big_x;
    lists.cost_x = lists.cost_big_x;

    // 重建周期需求
    lists.period_demand.clear();
    lists.period_demand.resize(values.number_of_flows);
    for (int f = 0; f < values.number_of_flows; f++) {
        lists.period_demand[f].resize(values.number_of_periods, 0);
    }

    for (int i = 0; i < values.number_of_items; i++) {
        BigOrder& bo = lists.big_order_list[i];
        int flow_idx = bo.flow_index;

        if (flow_idx >= 0 && flow_idx < values.number_of_flows) {
            int total_periods = lists.lw_x[i] - lists.ew_x[i] + 1;
            if (total_periods > 0) {
                int demand_per_period = lists.final_demand[i] / total_periods;
                int remaining = lists.final_demand[i] % total_periods;

                for (int t = lists.ew_x[i]; t <= lists.lw_x[i] && t < values.number_of_periods; t++) {
                    lists.period_demand[flow_idx][t] += demand_per_period;
                    if (remaining > 0) {
                        lists.period_demand[flow_idx][t]++;
                        remaining--;
                    }
                }
            }
        }
    }

    cout << "[大订单] 完成: " << values.original_number_of_items
         << " -> " << values.number_of_items << " 大订单\n";
}

// 基于流向-分组组合的订单合并（更激进的合并策略）
void UpdateBigOrderFG(AllValues& values, AllLists& lists) {
    cout << "\n[大订单FG] 开始流向-分组合并...\n";

    values.original_number_of_items = values.number_of_items;
    cout << "原始订单数: " << values.original_number_of_items << "\n";
    cout << "最大大订单数: " << values.number_of_flows * values.number_of_groups << "\n";

    lists.big_order_list.clear();
    lists.big_ew_x.clear();
    lists.big_lw_x.clear();
    lists.big_flow_flag.clear();
    lists.big_group_flag.clear();
    lists.big_final_demand.clear();
    lists.usage_big_x.clear();
    lists.cost_big_x.clear();

    struct FGKey {
        int flow_idx;
        int group_idx;

        bool operator<(const FGKey& other) const {
            if (flow_idx != other.flow_idx) return flow_idx < other.flow_idx;
            return group_idx < other.group_idx;
        }
    };

    map<FGKey, vector<int>> fg_groups;

    for (int i = 0; i < values.number_of_items; i++) {
        int flow_idx = -1;
        for (int f = 0; f < values.number_of_flows; f++) {
            if (lists.flow_flag[i][f] == 1) {
                flow_idx = f;
                break;
            }
        }

        int group_idx = -1;
        for (int g = 0; g < values.number_of_groups; g++) {
            if (lists.group_flag[i][g] == 1) {
                group_idx = g;
                break;
            }
        }

        if (flow_idx >= 0 && group_idx >= 0) {
            FGKey key = {flow_idx, group_idx};
            fg_groups[key].push_back(i);
        }
    }

    cout << "流向-分组组合数: " << fg_groups.size() << "\n";

    int big_order_id = 0;
    for (const auto& group : fg_groups) {
        BigOrder big_order;
        big_order.big_order_id = big_order_id++;
        big_order.order_ids = group.second;
        big_order.flow_index = group.first.flow_idx;
        big_order.group_index = group.first.group_idx;

        int min_early = INT_MAX;
        int max_late = INT_MIN;
        int total_demand = 0;
        double total_cost = 0.0;
        int max_usage = 0;

        for (int order_id : group.second) {
            min_early = min(min_early, lists.ew_x[order_id]);
            max_late = max(max_late, lists.lw_x[order_id]);
            total_demand += lists.final_demand[order_id];
            total_cost += lists.cost_x[order_id] * lists.final_demand[order_id];
            max_usage = max(max_usage, lists.usage_x[order_id]);
        }

        big_order.early_time = min_early;
        big_order.late_time = max_late;
        big_order.demand = total_demand;
        big_order.production_usage = max_usage;
        big_order.production_cost = (total_demand > 0) ? total_cost / total_demand : 0.0;

        lists.big_order_list.push_back(big_order);
        lists.big_ew_x.push_back(big_order.early_time);
        lists.big_lw_x.push_back(big_order.late_time);
        lists.big_final_demand.push_back(big_order.demand);
        lists.usage_big_x.push_back(big_order.production_usage);
        lists.cost_big_x.push_back(big_order.production_cost);

        cout << "  大订单 " << big_order.big_order_id
             << ": F" << big_order.flow_index << "-G" << big_order.group_index
             << " " << group.second.size() << " 订单"
             << " (需求=" << big_order.demand << ")\n";
    }

    lists.big_flow_flag.resize(big_order_id);
    lists.big_group_flag.resize(big_order_id);

    for (int i = 0; i < big_order_id; i++) {
        lists.big_flow_flag[i].resize(values.number_of_flows, 0);
        lists.big_group_flag[i].resize(values.number_of_groups, 0);

        if (lists.big_order_list[i].flow_index >= 0) {
            lists.big_flow_flag[i][lists.big_order_list[i].flow_index] = 1;
        }
        if (lists.big_order_list[i].group_index >= 0) {
            lists.big_group_flag[i][lists.big_order_list[i].group_index] = 1;
        }
    }

    // 备份原始数据
    lists.original_ew_x = lists.ew_x;
    lists.original_lw_x = lists.lw_x;
    lists.original_flow_flag = lists.flow_flag;
    lists.original_group_flag = lists.group_flag;
    lists.original_final_demand = lists.final_demand;
    lists.original_usage_x = lists.usage_x;
    lists.original_cost_x = lists.cost_x;
    lists.original_period_demand = lists.period_demand;

    // 替换为大订单数据
    values.number_of_items = big_order_id;
    lists.ew_x = lists.big_ew_x;
    lists.lw_x = lists.big_lw_x;
    lists.flow_flag = lists.big_flow_flag;
    lists.group_flag = lists.big_group_flag;
    lists.final_demand = lists.big_final_demand;
    lists.usage_x = lists.usage_big_x;
    lists.cost_x = lists.cost_big_x;

    // 重建周期需求
    lists.period_demand.clear();
    lists.period_demand.resize(values.number_of_flows);
    for (int f = 0; f < values.number_of_flows; f++) {
        lists.period_demand[f].resize(values.number_of_periods, 0);
    }

    for (int i = 0; i < values.number_of_items; i++) {
        BigOrder& bo = lists.big_order_list[i];
        int flow_idx = bo.flow_index;

        if (flow_idx >= 0 && flow_idx < values.number_of_flows) {
            int total_periods = lists.lw_x[i] - lists.ew_x[i] + 1;
            if (total_periods > 0) {
                int demand_per_period = lists.final_demand[i] / total_periods;
                int remaining = lists.final_demand[i] % total_periods;

                for (int t = lists.ew_x[i]; t <= lists.lw_x[i] && t < values.number_of_periods; t++) {
                    lists.period_demand[flow_idx][t] += demand_per_period;
                    if (remaining > 0) {
                        lists.period_demand[flow_idx][t]++;
                        remaining--;
                    }
                }
            }
        }
    }

    cout << "[大订单FG] 完成: " << values.original_number_of_items
         << " -> " << values.number_of_items << " 大订单\n";
}

// 大订单求解
void SolveBigOrder(AllValues& values, AllLists& lists) {
    cout << "\n[大订单求解器] 启动...\n";

    try {
        IloEnv env;
        IloModel model(env);

        IloArray<IloNumVarArray> X(env, values.number_of_items);
        IloArray<IloNumVarArray> Y(env, values.number_of_items);
        IloArray<IloNumVarArray> I(env, values.number_of_items);
        IloArray<IloNumVarArray> B(env, values.number_of_items);

        for (int i = 0; i < values.number_of_items; i++) {
            X[i] = IloNumVarArray(env, values.number_of_periods, 0, IloInfinity);
            Y[i] = IloNumVarArray(env, values.number_of_periods, 0, 1, IloNumVar::Bool);
            I[i] = IloNumVarArray(env, values.number_of_periods, 0, IloInfinity);
            B[i] = IloNumVarArray(env, values.number_of_periods, 0, IloInfinity);
        }

        IloExpr objective(env);

        for (int i = 0; i < values.number_of_items; i++) {
            for (int t = 0; t < values.number_of_periods; t++) {
                objective += lists.cost_x[i] * X[i][t];
                objective += lists.cost_b[i] * B[i][t];

                for (int g = 0; g < values.number_of_groups; g++) {
                    if (lists.group_flag[i][g]) {
                        objective += lists.cost_y[g] * Y[i][t];
                    }
                }

                for (int f = 0; f < values.number_of_flows; f++) {
                    if (lists.flow_flag[i][f]) {
                        objective += lists.cost_i[f] * I[i][t];
                    }
                }
            }
        }

        model.add(IloMinimize(env, objective));
        objective.end();

        for (int i = 0; i < values.number_of_items; i++) {
            for (int t = 0; t < values.number_of_periods; t++) {
                if (t == 0) {
                    model.add(I[i][t] - B[i][t] == X[i][t] - lists.period_demand[0][t]);
                } else {
                    model.add(I[i][t] - B[i][t] == I[i][t-1] - B[i][t-1] + X[i][t] - lists.period_demand[0][t]);
                }
            }
        }

        for (int t = 0; t < values.number_of_periods; t++) {
            IloExpr capacity(env);
            for (int i = 0; i < values.number_of_items; i++) {
                capacity += lists.usage_x[i] * X[i][t];
                for (int g = 0; g < values.number_of_groups; g++) {
                    if (lists.group_flag[i][g]) {
                        capacity += lists.usage_y[g] * Y[i][t];
                    }
                }
            }
            model.add(capacity <= values.machine_capacity);
            capacity.end();
        }

        for (int i = 0; i < values.number_of_items; i++) {
            for (int t = 0; t < values.number_of_periods; t++) {
                model.add(X[i][t] <= values.machine_capacity * Y[i][t]);
                if (t < lists.ew_x[i] || t > lists.lw_x[i]) {
                    model.add(X[i][t] == 0);
                }
            }
        }

        IloCplex cplex(model);
        cplex.setParam(IloCplex::TiLim, values.cpx_runtime_limit);
        cplex.setParam(IloCplex::Threads, values.cplex_threads);
        cplex.setParam(IloCplex::Param::MIP::Strategy::File, 3);
        cplex.setParam(IloCplex::Param::WorkDir, values.cplex_workdir.c_str());
        cplex.setParam(IloCplex::Param::WorkMem, values.cplex_workmem);

        auto start = chrono::steady_clock::now();
        bool solved = cplex.solve();
        auto end = chrono::steady_clock::now();
        double wall_time = chrono::duration<double>(end - start).count();

        bool has_solution = false;
        try {
            cplex.getObjValue();
            has_solution = true;
        } catch (...) {
            has_solution = false;
        }

        if (solved || has_solution) {
            cout << "  目标=" << cplex.getObjValue() << "\n";
            cout << "  耗时=" << wall_time << "s\n";
            cout << "  间隙=" << cplex.getMIPRelativeGap() << "\n";

            values.result_big_order.objective = cplex.getObjValue();
            values.result_big_order.runtime = wall_time;
            values.result_big_order.gap = cplex.getMIPRelativeGap();
        } else {
            cout << "[失败] 无可行解\n";
            values.result_big_order.objective = -1.0;
            values.result_big_order.runtime = wall_time;
            values.result_big_order.gap = -1.0;
        }

        env.end();

    } catch (IloException& e) {
        cout << "[错误] CPLEX: " << e << "\n";
        values.result_big_order.objective = -1.0;
        values.result_big_order.runtime = -1.0;
        values.result_big_order.gap = -1.0;
    } catch (...) {
        cout << "[错误] 未知异常\n";
        values.result_big_order.objective = -1.0;
        values.result_big_order.runtime = -1.0;
        values.result_big_order.gap = -1.0;
    }
}

// 结果拆分
void SplitBigOrderResults(AllValues& values, AllLists& lists,
                          IloArray<IloNumVarArray>& X,
                          IloArray<IloNumVarArray>& B,
                          IloArray<IloNumVarArray>& Y,
                          IloArray<IloNumVarArray>& L,
                          IloArray<IloNumVarArray>& I,
                          IloCplex& cplex) {

    cout << "\n[拆分] 将大订单结果分配至小订单...\n";

    vector<vector<double>> big_x(values.number_of_items);
    vector<vector<double>> big_b(values.number_of_items);
    vector<vector<int>> big_y(values.number_of_items);
    vector<vector<int>> big_l(values.number_of_items);
    vector<vector<double>> big_i(values.number_of_items);

    for (int i = 0; i < values.number_of_items; i++) {
        big_x[i].resize(values.number_of_periods);
        big_b[i].resize(values.number_of_periods);
        big_y[i].resize(values.number_of_periods);
        big_l[i].resize(values.number_of_periods);
        big_i[i].resize(values.number_of_periods);

        for (int t = 0; t < values.number_of_periods; t++) {
            big_x[i][t] = cplex.getValue(X[i][t]);
            big_b[i][t] = cplex.getValue(B[i][t]);
            big_y[i][t] = (int)cplex.getValue(Y[i][t]);
            big_l[i][t] = (int)cplex.getValue(L[i][t]);
            big_i[i][t] = cplex.getValue(I[i][t]);
        }
    }

    int original_items = values.original_number_of_items;

    lists.small_x.clear();
    lists.small_b.clear();
    lists.small_y.clear();
    lists.small_l.clear();
    lists.small_i.clear();
    lists.small_u.clear();

    lists.small_x.resize(original_items);
    lists.small_b.resize(original_items);
    lists.small_y.resize(original_items);
    lists.small_l.resize(original_items);
    lists.small_i.resize(original_items);
    lists.small_u.resize(original_items, 0.0);

    for (int i = 0; i < original_items; i++) {
        lists.small_x[i].resize(values.number_of_periods, 0.0);
        lists.small_b[i].resize(values.number_of_periods, 0.0);
        lists.small_y[i].resize(values.number_of_periods, 0);
        lists.small_l[i].resize(values.number_of_periods, 0);
        lists.small_i[i].resize(values.number_of_periods, 0.0);
    }

    for (size_t big_idx = 0; big_idx < lists.big_order_list.size(); big_idx++) {
        const BigOrder& bo = lists.big_order_list[big_idx];

        int total_demand = 0;
        for (int small_idx : bo.order_ids) {
            if (small_idx < (int)lists.original_final_demand.size()) {
                total_demand += lists.original_final_demand[small_idx];
            }
        }

        cout << "  大订单 " << big_idx << " -> " << bo.order_ids.size() << " 订单\n";

        for (int small_idx : bo.order_ids) {
            if (small_idx >= (int)lists.original_final_demand.size()) continue;

            double proportion = (total_demand > 0) ?
                static_cast<double>(lists.original_final_demand[small_idx]) / total_demand :
                1.0 / bo.order_ids.size();

            bool is_primary = true;
            for (int other : bo.order_ids) {
                if (other != small_idx && other < (int)lists.original_final_demand.size() &&
                    lists.original_final_demand[other] > lists.original_final_demand[small_idx]) {
                    is_primary = false;
                    break;
                }
            }

            for (int t = 0; t < values.number_of_periods; t++) {
                lists.small_x[small_idx][t] = big_x[big_idx][t] * proportion;
                lists.small_b[small_idx][t] = big_b[big_idx][t] * proportion;
                lists.small_i[small_idx][t] = big_i[big_idx][t] * proportion;
                lists.small_y[small_idx][t] = is_primary ? big_y[big_idx][t] : 0;
                lists.small_l[small_idx][t] = is_primary ? big_l[big_idx][t] : 0;
            }
        }
    }

    values.number_of_items = original_items;

    cout << "[拆分] 完成，已分配至 " << original_items << " 原始订单\n";
}

// 恢复原始订单数据
void RestoreOriginalOrderData(AllValues& values, AllLists& lists) {
    cout << "[恢复] 恢复原始订单数据...\n";

    values.number_of_items = values.original_number_of_items;

    lists.ew_x = lists.original_ew_x;
    lists.lw_x = lists.original_lw_x;
    lists.flow_flag = lists.original_flow_flag;
    lists.group_flag = lists.original_group_flag;
    lists.final_demand = lists.original_final_demand;
    lists.usage_x = lists.original_usage_x;
    lists.cost_x = lists.original_cost_x;
    lists.period_demand = lists.original_period_demand;

    cout << "[恢复] 完成 - " << values.number_of_items << " 订单\n";
}

// 大订单拆分（早期设计）
void SplitBigOrder(AllValues& values, AllLists& lists) {
    cout << "\n[拆分大订单] 拆分大型订单...\n";

    for (int i = 0; i < values.number_of_items; i++) {
        double total_demand = 0.0;
        for (int t = 0; t < values.number_of_periods; t++) {
            total_demand += lists.period_demand[0][t];
        }

        if (total_demand > values.big_order_threshold) {
            cout << "  订单 " << i << " 过大: " << total_demand << "\n";
            int num_splits = static_cast<int>(ceil(total_demand / values.big_order_threshold));
            double split_size = total_demand / num_splits;

            for (int t = 0; t < values.number_of_periods; t++) {
                lists.period_demand[0][t] = split_size * lists.period_demand[0][t] / total_demand;
            }
        }
    }

    cout << "[拆分大订单] 完成\n";
}

// 大订单验证
void VerifyBigOrder(AllValues& values, AllLists& lists) {
    cout << "\n[验证大订单] 验证中...\n";

    for (int i = 0; i < values.number_of_items; i++) {
        double total_demand = 0.0;
        for (int t = 0; t < values.number_of_periods; t++) {
            total_demand += lists.period_demand[0][t];
        }

        if (total_demand > values.big_order_threshold) {
            cout << "  [警告] 订单 " << i << " 仍超阈值: " << total_demand << "\n";
        }
    }

    for (int t = 0; t < values.number_of_periods; t++) {
        double total_usage = 0.0;
        for (int i = 0; i < values.number_of_items; i++) {
            total_usage += lists.usage_x[i] * lists.period_demand[0][t];
        }

        if (total_usage > values.machine_capacity) {
            cout << "  [警告] 周期 " << t << " 超产能: " << total_usage << "\n";
        }
    }

    cout << "[验证大订单] 完成\n";
}
