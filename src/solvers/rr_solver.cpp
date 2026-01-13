// rr_solver.cpp - RR (Relax-and-Recover) 三阶段求解算法
//
// RR (Relax-and-Recover) 算法:
//   Stage 1: 固定 lambda=0, 放大产能, 求解启动结构 y*
//   Stage 2: 固定 y*, 求解 lambda-子模型优化跨期变量 lambda*
//   Stage 3: 固定 y* 和 lambda*, 恢复真实产能, 求解最终生产计划

#include "optimizer.h"
#include "logger.h"

// Stage 1: 固定 lambda=0, 放大产能, 求解 y* 启动结构
void SolveStep1(AllValues& values, AllLists& lists) {
    LOG("\n[阶段1] 求解启动结构（扩大产能）...");
    LOG_FMT("  产能放大系数 = %.2fx，连续启动奖励 = %.1f\n",
            values.rr_capacity, values.rr_bonus);

    try {
        IloEnv env;
        IloModel model(env);

        // 决策变量
        IloArray<IloNumVarArray> X(env, values.number_of_items);
        IloArray<IloNumVarArray> Y(env, values.number_of_groups);
        IloArray<IloNumVarArray> I(env, values.number_of_flows);
        IloArray<IloNumVarArray> P(env, values.number_of_flows);
        IloArray<IloNumVarArray> B(env, values.number_of_items);
        IloNumVarArray U(env, values.number_of_items, 0, 1, ILOBOOL);

        for (int i = 0; i < values.number_of_items; i++) {
            X[i] = IloNumVarArray(env, values.number_of_periods, 0, IloInfinity);
            B[i] = IloNumVarArray(env, values.number_of_periods, 0, IloInfinity);
        }

        for (int g = 0; g < values.number_of_groups; g++) {
            Y[g] = IloNumVarArray(env, values.number_of_periods, 0, 1, ILOBOOL);
        }

        // Z[g][t] = Y[g][t-1] * Y[g][t] 的线性化辅助变量（连续启动指示）
        IloArray<IloNumVarArray> Z(env, values.number_of_groups);
        for (int g = 0; g < values.number_of_groups; g++) {
            Z[g] = IloNumVarArray(env, values.number_of_periods, 0, 1, ILOBOOL);
        }

        for (int f = 0; f < values.number_of_flows; f++) {
            I[f] = IloNumVarArray(env, values.number_of_periods, 0, IloInfinity);
            P[f] = IloNumVarArray(env, values.number_of_periods, 0, IloInfinity);
        }

        // 目标函数
        IloExpr objective(env);

        for (int i = 0; i < values.number_of_items; i++) {
            for (int t = 0; t < values.number_of_periods; t++) {
                objective += lists.cost_x[i] * X[i][t];
            }
            // 欠交惩罚 (仅 t >= l_i)
            for (int t = lists.lw_x[i]; t < values.number_of_periods; t++) {
                objective += lists.cost_b[i] * B[i][t];
            }
        }

        for (int g = 0; g < values.number_of_groups; g++) {
            for (int t = 0; t < values.number_of_periods; t++) {
                objective += lists.cost_y[g] * Y[g][t];
            }
        }

        for (int f = 0; f < values.number_of_flows; f++) {
            for (int t = 0; t < values.number_of_periods; t++) {
                objective += lists.cost_i[f] * I[f][t];
            }
        }

        for (int i = 0; i < values.number_of_items; i++) {
            objective += lists.cost_u[i] * U[i];
        }

        // 连续启动奖励（负值减少目标函数，鼓励连续启动）
        for (int g = 0; g < values.number_of_groups; g++) {
            for (int t = 1; t < values.number_of_periods; t++) {
                objective -= values.rr_bonus * Z[g][t];  // 使用动态参数
            }
        }

        model.add(IloMinimize(env, objective));
        objective.end();

        // 需求满足约束
        for (int i = 0; i < values.number_of_items; i++) {
            IloExpr total_production(env);
            for (int t = 0; t < values.number_of_periods; t++) {
                total_production += X[i][t];
            }
            model.add(total_production + U[i] * lists.final_demand[i] >= lists.final_demand[i]);
            total_production.end();
        }

        // 产能约束（放大）
        double capacity_big = values.machine_capacity * values.rr_capacity;  // 使用动态参数
        for (int t = 0; t < values.number_of_periods; t++) {
            IloExpr capacity(env);
            for (int i = 0; i < values.number_of_items; i++) {
                capacity += lists.usage_x[i] * X[i][t];
            }
            for (int g = 0; g < values.number_of_groups; g++) {
                capacity += lists.usage_y[g] * Y[g][t];
            }
            model.add(capacity <= capacity_big);
            capacity.end();
        }

        // 产品大类级 Big-M 约束
        for (int g = 0; g < values.number_of_groups; g++) {
            for (int t = 0; t < values.number_of_periods; t++) {
                IloExpr family_production(env);
                for (int i = 0; i < values.number_of_items; i++) {
                    if (lists.group_flag[i][g]) {
                        family_production += lists.usage_x[i] * X[i][t];
                    }
                }
                model.add(family_production <= capacity_big * Y[g][t]);
                family_production.end();
            }
        }

        // Z[g][t] = Y[g][t-1] * Y[g][t] 线性化约束
        for (int g = 0; g < values.number_of_groups; g++) {
            model.add(Z[g][0] == 0);  // t=0 没有前一周期
            for (int t = 1; t < values.number_of_periods; t++) {
                model.add(Z[g][t] <= Y[g][t - 1]);
                model.add(Z[g][t] <= Y[g][t]);
                model.add(Z[g][t] >= Y[g][t - 1] + Y[g][t] - 1);
            }
        }

        // 下游工序流平衡
        for (int f = 0; f < values.number_of_flows; f++) {
            for (int t = 0; t < values.number_of_periods; t++) {
                IloExpr flow_production(env);
                for (int i = 0; i < values.number_of_items; i++) {
                    flow_production += lists.flow_flag[i][f] * X[i][t];
                }

                if (t == 0) {
                    model.add(flow_production - P[f][t] - I[f][t] == 0);
                } else {
                    model.add(flow_production + I[f][t-1] - P[f][t] - I[f][t] == 0);
                }

                flow_production.end();
            }
        }

        // 下游工序能力
        for (int f = 0; f < values.number_of_flows; f++) {
            for (int t = 0; t < values.number_of_periods; t++) {
                model.add(P[f][t] <= lists.period_demand[f][t]);
            }
        }

        // 最早生产期约束 (仅约束 t < e_i)
        for (int i = 0; i < values.number_of_items; i++) {
            for (int t = 0; t < values.number_of_periods; t++) {
                if (t < lists.ew_x[i]) {
                    model.add(X[i][t] == 0);
                }
            }
        }

        // 欠交定义
        for (int i = 0; i < values.number_of_items; i++) {
            for (int t = 0; t < values.number_of_periods; t++) {
                if (t >= lists.lw_x[i]) {
                    IloExpr cumulative_production(env);
                    for (int tau = 0; tau <= t; tau++) {
                        cumulative_production += X[i][tau];
                    }
                    model.add(B[i][t] == lists.final_demand[i] - cumulative_production);
                    cumulative_production.end();
                } else {
                    model.add(B[i][t] == 0);
                }
            }
        }

        // 终期欠交与未满足指示
        for (int i = 0; i < values.number_of_items; i++) {
            int T = values.number_of_periods - 1;
            model.add(lists.final_demand[i] * U[i] >= B[i][T]);
        }

        // 求解
        IloCplex cplex(model);
        cplex.setParam(IloCplex::TiLim, values.cpx_runtime_limit);
        cplex.setParam(IloCplex::Threads, values.cplex_threads);
        cplex.setParam(IloCplex::Param::MIP::Strategy::File, 3);
        cplex.setParam(IloCplex::Param::WorkDir, values.cplex_workdir.c_str());
        cplex.setParam(IloCplex::Param::WorkMem, values.cplex_workmem);

        // CPLEX 日志输出到双向流
        if (g_logger) {
            cplex.setOut(g_logger->GetTeeStream());
        }
        LOG("\n=============== CPLEX START ===============");

        auto step1_start = chrono::steady_clock::now();
        bool has_solution = cplex.solve();
        auto step1_end = chrono::steady_clock::now();
        double step1_wall_time = chrono::duration<double>(step1_end - step1_start).count();

        // 求解后关闭CPLEX输出并刷新
        cplex.setOut(env.getNullStream());
        if (g_logger) g_logger->Flush();
        LOG("=============== CPLEX END =================");
        LOG_RAW("\n");

        bool has_incumbent = false;
        try {
            cplex.getObjValue();
            has_incumbent = true;
        } catch (...) {
            has_incumbent = false;
        }

        if (has_solution || has_incumbent || cplex.getStatus() == IloAlgorithm::Feasible ||
            cplex.getStatus() == IloAlgorithm::Optimal) {

            if (has_incumbent) {
                const char* status_str = (cplex.getStatus() == IloAlgorithm::Optimal) ? "最优" : "可行";
                LOG_FMT("[阶段1] %s 目标=%.2f 时间=%.2f秒\n", status_str, cplex.getObjValue(), step1_wall_time);

                values.result_step1.objective = cplex.getObjValue();
                values.result_step1.runtime = step1_wall_time;
                values.result_step1.cpu_time = cplex.getTime();
                values.result_step1.gap = cplex.getMIPRelativeGap();

                // 存储决策变量结果
                for (int i = 0; i < values.number_of_items; i++) {
                    vector<double> x_row, b_row;
                    for (int t = 0; t < values.number_of_periods; t++) {
                        x_row.push_back(cplex.getValue(X[i][t]));
                        b_row.push_back(cplex.getValue(B[i][t]));
                    }
                    lists.small_x.push_back(x_row);
                    lists.small_b.push_back(b_row);
                }

                for (int g = 0; g < values.number_of_groups; g++) {
                    vector<int> y_row;
                    for (int t = 0; t < values.number_of_periods; t++) {
                        y_row.push_back(cplex.getValue(Y[g][t]));
                    }
                    lists.small_y.push_back(y_row);
                }

                for (int f = 0; f < values.number_of_flows; f++) {
                    vector<double> i_row;
                    for (int t = 0; t < values.number_of_periods; t++) {
                        i_row.push_back(cplex.getValue(I[f][t]));
                    }
                    lists.small_i.push_back(i_row);
                }

                for (int i = 0; i < values.number_of_items; i++) {
                    lists.small_u.push_back(cplex.getValue(U[i]));
                }

            } else {
                LOG("[阶段1] 未找到可行解");
                values.result_step1.objective = -1;
                values.result_step1.runtime = step1_wall_time;
                values.result_step1.cpu_time = cplex.getTime();
                values.result_step1.gap = -1;
            }
        } else {
            LOG("[阶段1] 求解器失败");
            values.result_step1.objective = -1;
            values.result_step1.runtime = step1_wall_time;
            values.result_step1.cpu_time = cplex.getTime();
            values.result_step1.gap = -1;
        }

        env.end();

    } catch (IloException& e) {
        LOG_FMT("[阶段1] CPLEX错误: %s\n", e.getMessage());
    } catch (...) {
        LOG("[阶段1] 未知错误");
    }
}

// Stage 2: 固定 y*, 求解 lambda-子模型
void SolveStep2(AllValues& values, AllLists& lists) {
    LOG("\n[阶段2] 求解跨期子模型（固定y*）...");

    if (values.result_step1.objective == -1 || lists.small_y.empty()) {
        LOG("[阶段2] 跳过 - 阶段1失败");
        values.result_step2.objective = -1;
        values.result_step2.runtime = -1;
        values.result_step2.gap = -1;
        return;
    }

    try {
        IloEnv env;
        IloModel model(env);

        IloArray<IloNumVarArray> Y(env, values.number_of_groups);
        IloArray<IloNumVarArray> Lambda(env, values.number_of_groups);

        for (int g = 0; g < values.number_of_groups; ++g) {
            Y[g] = IloNumVarArray(env, values.number_of_periods, 0, 1, ILOBOOL);
            Lambda[g] = IloNumVarArray(env, values.number_of_periods, 0, 1, ILOBOOL);
        }

        // 目标: 最大化跨期总和
        IloExpr objective(env);
        for (int g = 0; g < values.number_of_groups; ++g) {
            for (int t = 0; t < values.number_of_periods; ++t) {
                objective += Lambda[g][t];
            }
        }
        model.add(IloMaximize(env, objective));
        objective.end();

        // 固定 y* 到 Stage 1 结果
        for (int g = 0; g < values.number_of_groups; ++g) {
            for (int t = 0; t < values.number_of_periods; ++t) {
                model.add(Y[g][t] == lists.small_y[g][t]);
            }
        }

        // 初始条件: 第一个周期没有跨期
        for (int g = 0; g < values.number_of_groups; ++g) {
            model.add(Lambda[g][0] == 0);
        }

        // 约束 (a): 每个周期最多一个跨期
        for (int t = 0; t < values.number_of_periods; ++t) {
            IloExpr sum_lambda(env);
            for (int g = 0; g < values.number_of_groups; ++g) {
                sum_lambda += Lambda[g][t];
            }
            model.add(sum_lambda <= 1);
            sum_lambda.end();
        }

        // 约束 (b): Carryover 只能在连续激活的周期间发生
        for (int g = 0; g < values.number_of_groups; ++g) {
            for (int t = 1; t < values.number_of_periods; ++t) {
                model.add(2 * Lambda[g][t] <= Y[g][t - 1] + Y[g][t]);
            }
        }

        // 约束 (c): 防止跨期与其他族的启动冲突
        int num_groups = values.number_of_groups;
        for (int g = 0; g < num_groups; ++g) {
            for (int t = 2; t < values.number_of_periods; ++t) {
                IloExpr sum_other_setups(env);
                for (int g_prime = 0; g_prime < num_groups; ++g_prime) {
                    if (g_prime != g) {
                        sum_other_setups += Y[g_prime][t - 1];
                    }
                }
                model.add(Lambda[g][t-1] + Lambda[g][t] <= 2.0 - sum_other_setups / num_groups);
                sum_other_setups.end();
            }
        }

        // 求解
        IloCplex cplex(model);
        cplex.setParam(IloCplex::TiLim, values.cpx_runtime_limit);
        cplex.setParam(IloCplex::Threads, values.cplex_threads);
        cplex.setParam(IloCplex::Param::MIP::Strategy::File, 3);
        cplex.setParam(IloCplex::Param::WorkDir, values.cplex_workdir.c_str());
        cplex.setParam(IloCplex::Param::WorkMem, values.cplex_workmem);

        // CPLEX 日志输出到双向流
        if (g_logger) {
            cplex.setOut(g_logger->GetTeeStream());
        }
        LOG("\n=============== CPLEX START ===============");

        auto step2_start = chrono::steady_clock::now();
        bool has_solution = cplex.solve();
        auto step2_end = chrono::steady_clock::now();
        double step2_wall_time = chrono::duration<double>(step2_end - step2_start).count();

        // 求解后关闭CPLEX输出并刷新
        cplex.setOut(env.getNullStream());
        if (g_logger) g_logger->Flush();
        LOG("=============== CPLEX END =================");
        LOG_RAW("\n");

        bool has_incumbent = false;
        try {
            cplex.getObjValue();
            has_incumbent = true;
        } catch (...) {
            has_incumbent = false;
        }

        if (has_solution || has_incumbent || cplex.getStatus() == IloAlgorithm::Feasible ||
            cplex.getStatus() == IloAlgorithm::Optimal) {

            if (has_incumbent) {
                values.result_step2.objective = cplex.getObjValue();
                values.result_step2.runtime = step2_wall_time;
                values.result_step2.cpu_time = cplex.getTime();
                values.result_step2.gap = cplex.getMIPRelativeGap();

                for (int g = 0; g < values.number_of_groups; ++g) {
                    vector<int> lambda_row;
                    for (int t = 0; t < values.number_of_periods; ++t) {
                        lambda_row.push_back(cplex.getValue(Lambda[g][t]));
                    }
                    lists.small_l.push_back(lambda_row);
                }

                int total_carryovers = 0;
                for (int g = 0; g < values.number_of_groups; ++g) {
                    for (int t = 0; t < values.number_of_periods; ++t) {
                        if (cplex.getValue(Lambda[g][t]) > 0.5) {
                            total_carryovers++;
                        }
                    }
                }
                LOG_FMT("[阶段2] 发现 %d 个跨期机会\n", total_carryovers);

            } else {
                LOG("[阶段2] 未找到可行解");
                values.result_step2.objective = -1;
                values.result_step2.runtime = step2_wall_time;
                values.result_step2.cpu_time = cplex.getTime();
                values.result_step2.gap = -1;
            }
        } else {
            LOG("[阶段2] 求解器失败");
            values.result_step2.objective = -1;
            values.result_step2.runtime = step2_wall_time;
            values.result_step2.cpu_time = cplex.getTime();
            values.result_step2.gap = -1;
        }

        env.end();

    } catch (IloException& e) {
        LOG_FMT("[阶段2] CPLEX错误: %s\n", e.getMessage());
    } catch (...) {
        LOG("[阶段2] 未知错误");
    }
}

// Stage 3: 固定 y* 和 lambda*, 恢复真实产能, 终算
void SolveStep3(AllValues& values, AllLists& lists) {
    LOG("\n[阶段3] 最终求解（固定y*和lambda*）...");

    if (values.result_step1.objective == -1 || values.result_step2.objective == -1) {
        LOG("[阶段3] 跳过 - 前序阶段失败");
        values.result_step3.objective = -1;
        values.result_step3.runtime = -1;
        values.result_step3.gap = -1;
        return;
    }

    if (lists.small_y.empty() || lists.small_l.empty()) {
        LOG("[阶段3] 跳过 - 缺少y*或lambda*数据");
        values.result_step3.objective = -1;
        values.result_step3.runtime = -1;
        values.result_step3.gap = -1;
        return;
    }

    if (lists.small_y.size() < static_cast<size_t>(values.number_of_groups) ||
        lists.small_l.size() < static_cast<size_t>(values.number_of_groups)) {
        LOG("[阶段3] 维度不匹配");
        values.result_step3.objective = -1;
        values.result_step3.runtime = -1;
        values.result_step3.gap = -1;
        return;
    }

    for (int g = 0; g < values.number_of_groups; ++g) {
        if (lists.small_y[g].size() < static_cast<size_t>(values.number_of_periods) ||
            lists.small_l[g].size() < static_cast<size_t>(values.number_of_periods)) {
            LOG_FMT("[阶段3] 分组 %d 周期不匹配\n", g);
            values.result_step3.objective = -1;
            values.result_step3.runtime = -1;
            values.result_step3.gap = -1;
            return;
        }
    }

    try {
        IloEnv env;
        IloModel model(env);

        IloArray<IloNumVarArray> X(env, values.number_of_items);
        IloArray<IloNumVarArray> Y(env, values.number_of_groups);
        IloArray<IloNumVarArray> Lambda(env, values.number_of_groups);
        IloArray<IloNumVarArray> I(env, values.number_of_flows);
        IloArray<IloNumVarArray> B(env, values.number_of_items);
        IloArray<IloNumVarArray> P(env, values.number_of_flows);
        IloNumVarArray U(env, values.number_of_items, 0, 1, ILOBOOL);

        for (int i = 0; i < values.number_of_items; ++i) {
            X[i] = IloNumVarArray(env, values.number_of_periods, 0, IloInfinity);
            B[i] = IloNumVarArray(env, values.number_of_periods, 0, IloInfinity);
        }

        for (int g = 0; g < values.number_of_groups; ++g) {
            Y[g] = IloNumVarArray(env, values.number_of_periods, 0, 1, ILOBOOL);
            Lambda[g] = IloNumVarArray(env, values.number_of_periods, 0, 1, ILOBOOL);
        }

        for (int f = 0; f < values.number_of_flows; ++f) {
            I[f] = IloNumVarArray(env, values.number_of_periods, 0, IloInfinity);
            P[f] = IloNumVarArray(env, values.number_of_periods, 0, IloInfinity);
        }

        // 目标函数
        IloExpr objective(env);

        for (int i = 0; i < values.number_of_items; ++i) {
            for (int t = 0; t < values.number_of_periods; ++t) {
                objective += lists.cost_x[i] * X[i][t];
            }
            // 欠交惩罚 (仅 t >= l_i)
            for (int t = lists.lw_x[i]; t < values.number_of_periods; ++t) {
                objective += lists.cost_b[i] * B[i][t];
            }
        }

        for (int g = 0; g < values.number_of_groups; ++g) {
            for (int t = 0; t < values.number_of_periods; ++t) {
                objective += lists.cost_y[g] * Y[g][t];
            }
        }

        for (int f = 0; f < values.number_of_flows; ++f) {
            for (int t = 0; t < values.number_of_periods; ++t) {
                objective += lists.cost_i[f] * I[f][t];
            }
        }

        for (int i = 0; i < values.number_of_items; ++i) {
            objective += lists.cost_u[i] * U[i];
        }

        model.add(IloMinimize(env, objective));
        objective.end();

        // 固定 y* (如果 lambda*=1 则 y=0)
        for (int g = 0; g < values.number_of_groups; ++g) {
            for (int t = 0; t < values.number_of_periods; ++t) {
                if (lists.small_l[g][t] == 1) {
                    model.add(Y[g][t] == 0);
                } else {
                    model.add(Y[g][t] == lists.small_y[g][t]);
                }
            }
        }

        // 固定 lambda*
        for (int g = 0; g < values.number_of_groups; ++g) {
            for (int t = 0; t < values.number_of_periods; ++t) {
                model.add(Lambda[g][t] == lists.small_l[g][t]);
            }
        }

        // 需求满足约束
        for (int i = 0; i < values.number_of_items; ++i) {
            IloExpr total_production(env);
            for (int t = 0; t < values.number_of_periods; ++t) {
                total_production += X[i][t];
            }
            model.add(total_production + U[i] * lists.final_demand[i] >= lists.final_demand[i]);
            total_production.end();
        }

        // 产能约束（真实）
        for (int t = 0; t < values.number_of_periods; ++t) {
            IloExpr capacity(env);
            for (int i = 0; i < values.number_of_items; ++i) {
                capacity += lists.usage_x[i] * X[i][t];
            }
            for (int g = 0; g < values.number_of_groups; ++g) {
                capacity += lists.usage_y[g] * Y[g][t];
            }
            model.add(capacity <= values.machine_capacity);
            capacity.end();
        }

        // 产品大类级 Setup 约束（含 carryover）
        for (int g = 0; g < values.number_of_groups; ++g) {
            for (int t = 0; t < values.number_of_periods; ++t) {
                IloExpr family_production(env);
                for (int i = 0; i < values.number_of_items; ++i) {
                    if (lists.group_flag[i][g]) {
                        family_production += lists.usage_x[i] * X[i][t];
                    }
                }
                model.add(family_production <= values.machine_capacity * (Y[g][t] + Lambda[g][t]));
                family_production.end();
            }
        }

        // 下游工序流平衡
        for (int f = 0; f < values.number_of_flows; ++f) {
            for (int t = 0; t < values.number_of_periods; ++t) {
                IloExpr flow_production(env);
                for (int i = 0; i < values.number_of_items; ++i) {
                    if (lists.flow_flag[i][f]) {
                        flow_production += X[i][t];
                    }
                }

                if (t == 0) {
                    model.add(flow_production - P[f][t] - I[f][t] == 0);
                } else {
                    model.add(flow_production + I[f][t-1] - P[f][t] - I[f][t] == 0);
                }

                flow_production.end();
            }
        }

        // 下游工序能力
        for (int f = 0; f < values.number_of_flows; ++f) {
            for (int t = 0; t < values.number_of_periods; ++t) {
                model.add(P[f][t] <= lists.period_demand[f][t]);
            }
        }

        // 时间窗约束
        for (int i = 0; i < values.number_of_items; ++i) {
            for (int t = 0; t < values.number_of_periods; ++t) {
                if (t < lists.ew_x[i] || t > lists.lw_x[i]) {
                    model.add(X[i][t] == 0);
                }
            }
        }

        // 欠交定义
        for (int i = 0; i < values.number_of_items; ++i) {
            for (int t = 0; t < values.number_of_periods; ++t) {
                if (t >= lists.lw_x[i]) {
                    IloExpr cumulative_production(env);
                    for (int tau = 0; tau <= t; tau++) {
                        cumulative_production += X[i][tau];
                    }
                    model.add(B[i][t] == lists.final_demand[i] - cumulative_production);
                    cumulative_production.end();
                } else {
                    model.add(B[i][t] == 0);
                }
            }
        }

        // 终期欠交与未满足指示
        for (int i = 0; i < values.number_of_items; ++i) {
            int T = values.number_of_periods - 1;
            model.add(lists.final_demand[i] * U[i] >= B[i][T]);
        }

        // 求解
        IloCplex cplex(model);
        cplex.setParam(IloCplex::TiLim, values.cpx_runtime_limit);
        cplex.setParam(IloCplex::Threads, values.cplex_threads);
        cplex.setParam(IloCplex::Param::MIP::Strategy::File, 3);
        cplex.setParam(IloCplex::Param::WorkDir, values.cplex_workdir.c_str());
        cplex.setParam(IloCplex::Param::WorkMem, values.cplex_workmem);

        // CPLEX 日志输出到双向流
        if (g_logger) {
            cplex.setOut(g_logger->GetTeeStream());
        }
        LOG("\n=============== CPLEX START ===============");

        auto step3_start = chrono::steady_clock::now();
        bool has_solution = cplex.solve();
        auto step3_end = chrono::steady_clock::now();
        double step3_wall_time = chrono::duration<double>(step3_end - step3_start).count();

        // 求解后关闭CPLEX输出并刷新
        cplex.setOut(env.getNullStream());
        if (g_logger) g_logger->Flush();
        LOG("=============== CPLEX END =================");
        LOG_RAW("\n");

        bool has_incumbent = false;
        try {
            cplex.getObjValue();
            has_incumbent = true;
        } catch (...) {
            has_incumbent = false;
        }

        if (has_solution || has_incumbent || cplex.getStatus() == IloAlgorithm::Feasible ||
            cplex.getStatus() == IloAlgorithm::Optimal) {

            if (has_incumbent) {
                const char* status_str = (cplex.getStatus() == IloAlgorithm::Optimal) ? "最优" : "可行";
                LOG_FMT("[阶段3] %s 目标=%.2f 时间=%.2f秒\n", status_str, cplex.getObjValue(), step3_wall_time);

                values.result_step3.objective = cplex.getObjValue();
                values.result_step3.runtime = step3_wall_time;
                values.result_step3.cpu_time = cplex.getTime();
                values.result_step3.gap = cplex.getMIPRelativeGap();

                int total_carryovers_used = 0;
                double saved_setup_cost = 0.0;
                for (int g = 0; g < values.number_of_groups; ++g) {
                    for (int t = 0; t < values.number_of_periods; ++t) {
                        if (lists.small_l[g][t] == 1) {
                            total_carryovers_used++;
                            saved_setup_cost += lists.cost_y[g];
                        }
                    }
                }
                LOG_FMT("[阶段3] 使用 %d 个跨期，节省启动成本 %.2f\n", total_carryovers_used, saved_setup_cost);

                // Save decision variables to AllLists for JSON output
                lists.small_x.resize(values.number_of_items);
                lists.small_b.resize(values.number_of_items);
                lists.small_u.resize(values.number_of_items);
                lists.small_i.resize(values.number_of_flows);

                for (int i = 0; i < values.number_of_items; ++i) {
                    lists.small_x[i].resize(values.number_of_periods);
                    lists.small_b[i].resize(values.number_of_periods);
                    for (int t = 0; t < values.number_of_periods; ++t) {
                        lists.small_x[i][t] = cplex.getValue(X[i][t]);
                        lists.small_b[i][t] = cplex.getValue(B[i][t]);
                    }
                    lists.small_u[i] = cplex.getValue(U[i]);
                }

                for (int f = 0; f < values.number_of_flows; ++f) {
                    lists.small_i[f].resize(values.number_of_periods);
                    for (int t = 0; t < values.number_of_periods; ++t) {
                        lists.small_i[f][t] = cplex.getValue(I[f][t]);
                    }
                }

                // ========== Calculate metrics ==========
                auto& m = values.metrics;

                // Cost breakdown
                m.cost_production = 0.0;
                m.cost_setup = 0.0;
                m.cost_inventory = 0.0;
                m.cost_backorder = 0.0;
                m.cost_unmet = 0.0;

                for (int i = 0; i < values.number_of_items; ++i) {
                    for (int t = 0; t < values.number_of_periods; ++t) {
                        m.cost_production += lists.cost_x[i] * cplex.getValue(X[i][t]);
                        m.cost_backorder += lists.cost_b[i] * cplex.getValue(B[i][t]);
                    }
                    m.cost_unmet += lists.cost_u[i] * cplex.getValue(U[i]);
                }

                for (int g = 0; g < values.number_of_groups; ++g) {
                    for (int t = 0; t < values.number_of_periods; ++t) {
                        m.cost_setup += lists.cost_y[g] * lists.small_y[g][t];
                    }
                }

                for (int f = 0; f < values.number_of_flows; ++f) {
                    for (int t = 0; t < values.number_of_periods; ++t) {
                        m.cost_inventory += lists.cost_i[f] * cplex.getValue(I[f][t]);
                    }
                }

                // Setup/Carryover statistics
                m.total_setups = 0;
                m.total_carryovers = total_carryovers_used;
                m.saved_setup_cost = saved_setup_cost;

                for (int g = 0; g < values.number_of_groups; ++g) {
                    for (int t = 0; t < values.number_of_periods; ++t) {
                        if (lists.small_y[g][t] == 1) m.total_setups++;
                    }
                }

                // Demand fulfillment
                m.unmet_count = 0;
                m.total_backorder = 0.0;
                m.total_demand = 0.0;
                int on_time_count = 0;

                for (int i = 0; i < values.number_of_items; ++i) {
                    m.total_demand += lists.final_demand[i];
                    if (cplex.getValue(U[i]) > 0.5) {
                        m.unmet_count++;
                    } else {
                        // Check if delivered on time (no backorder at due date)
                        int lw = lists.lw_x[i];
                        if (lw < values.number_of_periods && cplex.getValue(B[i][lw]) < 0.5) {
                            on_time_count++;
                        }
                    }
                    // Sum backorder at final period
                    int T_last = values.number_of_periods - 1;
                    m.total_backorder += cplex.getValue(B[i][T_last]);
                }

                m.unmet_rate = values.number_of_items > 0
                    ? (double)m.unmet_count / values.number_of_items : 0.0;
                m.on_time_rate = values.number_of_items > 0
                    ? (double)on_time_count / values.number_of_items : 0.0;

                // Capacity utilization
                m.capacity_util_by_period.resize(values.number_of_periods);
                m.capacity_util_avg = 0.0;
                m.capacity_util_max = 0.0;

                for (int t = 0; t < values.number_of_periods; ++t) {
                    double usage = 0.0;
                    for (int i = 0; i < values.number_of_items; ++i) {
                        usage += lists.usage_x[i] * cplex.getValue(X[i][t]);
                    }
                    for (int g = 0; g < values.number_of_groups; ++g) {
                        usage += lists.usage_y[g] * lists.small_y[g][t];
                    }
                    double util = values.machine_capacity > 0
                        ? usage / values.machine_capacity : 0.0;
                    m.capacity_util_by_period[t] = util;
                    m.capacity_util_avg += util;
                    if (util > m.capacity_util_max) m.capacity_util_max = util;
                }
                m.capacity_util_avg /= values.number_of_periods;

                // CPLEX solver stats
                m.cplex_nodes = cplex.getNnodes();
                m.cplex_iterations = cplex.getNiterations();

                // RR-specific metrics
                m.rr_step1_objective = values.result_step1.objective;
                m.rr_step1_time = values.result_step1.runtime;
                m.rr_step2_time = values.result_step2.runtime;
                m.rr_step3_objective = values.result_step3.objective;
                m.rr_step3_time = step3_wall_time;

                // Count Step1 setups
                m.rr_step1_setups = 0;
                for (int g = 0; g < values.number_of_groups; ++g) {
                    for (int t = 0; t < values.number_of_periods; ++t) {
                        if (lists.small_y[g][t] == 1) m.rr_step1_setups++;
                    }
                }

                // Step2 carryovers (from small_l)
                m.rr_step2_carryovers = 0;
                for (int g = 0; g < values.number_of_groups; ++g) {
                    for (int t = 0; t < values.number_of_periods; ++t) {
                        if (lists.small_l[g][t] == 1) m.rr_step2_carryovers++;
                    }
                }

                // Gap between Step3 and Step1
                if (m.rr_step1_objective > 0) {
                    m.rr_step3_gap_to_step1 = (m.rr_step3_objective - m.rr_step1_objective)
                                              / m.rr_step1_objective;
                }

                // Carryover utilization
                m.rr_carryover_utilization = m.rr_step2_carryovers > 0
                    ? (double)m.total_carryovers / m.rr_step2_carryovers : 0.0;

            } else {
                LOG("[阶段3] 未找到可行解");
                values.result_step3.objective = -1;
                values.result_step3.runtime = step3_wall_time;
                values.result_step3.cpu_time = cplex.getTime();
                values.result_step3.gap = -1;
            }
        } else {
            LOG("[阶段3] 求解器失败");
            values.result_step3.objective = -1;
            values.result_step3.runtime = step3_wall_time;
            values.result_step3.cpu_time = cplex.getTime();
            values.result_step3.gap = -1;
        }

        env.end();

    } catch (IloException& e) {
        LOG_FMT("[阶段3] CPLEX错误: %s\n", e.getMessage());
    } catch (...) {
        LOG("[阶段3] 未知错误");
    }
}
