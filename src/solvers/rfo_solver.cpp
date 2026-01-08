// rfo_solver.cpp - RFO (RF + FO) 求解算法
//
// RFO = Relax-and-Fix + Fix-and-Optimize
// 第一阶段 RF: 滚动时间窗口构造初始可行解
// 第二阶段 FO: 滑动窗口局部优化改进解质量

#include "optimizer.h"
#include "logger.h"

// ============================================================================
// RF (Relax-and-Fix) 部分
// ============================================================================

// 初始化 RF 状态
static void InitRFState(RFState& state, const AllValues& values) {
    int G = values.number_of_groups;
    int T = values.number_of_periods;

    state.y_bar.assign(G, vector<int>(T, 0));
    state.lambda_bar.assign(G, vector<int>(T, 0));
    state.period_fixed.assign(T, false);
    state.rollback_stack.clear();
    state.current_k = 0;
    state.current_W = kRFWindowSize;
    state.iterations = 0;
}

// 求解 RF 子问题 SP(k, W)
static bool SolveRFSubproblem(
    int k, int W,
    const RFState& state,
    AllValues& values,
    AllLists& lists,
    vector<vector<int>>& y_solution,
    vector<vector<int>>& lambda_solution,
    bool is_final = false,
    double* objective_out = nullptr,
    double* cpu_time_out = nullptr)
{
    int G = values.number_of_groups;
    int T = values.number_of_periods;
    int N = values.number_of_items;
    int F = values.number_of_flows;

    int win_end = min(k + W, T);
    int rel_start = win_end;

    LOG_FMT("[RF] 子问题: k=%d W=%d (固定:[0,%d) 窗口:[%d,%d) 放松:[%d,%d))\n",
            k, W, k, k, win_end, rel_start, T);

    try {
        IloEnv env;
        IloModel model(env);

        IloArray<IloNumVarArray> X(env, N);
        IloArray<IloNumVarArray> Y(env, G);
        IloArray<IloNumVarArray> Lambda(env, G);
        IloArray<IloNumVarArray> I(env, F);
        IloArray<IloNumVarArray> P(env, F);
        IloArray<IloNumVarArray> B(env, N);
        IloNumVarArray U(env);

        for (int i = 0; i < N; i++) {
            X[i] = IloNumVarArray(env, T, 0, IloInfinity);
            B[i] = IloNumVarArray(env, T, 0, IloInfinity);
        }

        for (int f = 0; f < F; f++) {
            I[f] = IloNumVarArray(env, T, 0, IloInfinity);
            P[f] = IloNumVarArray(env, T, 0, IloInfinity);
        }

        for (int g = 0; g < G; g++) {
            Y[g] = IloNumVarArray(env, T);
            Lambda[g] = IloNumVarArray(env, T);

            for (int t = 0; t < T; t++) {
                if (t < k) {
                    Y[g][t] = IloNumVar(env, 0, 1, ILOFLOAT);
                    Lambda[g][t] = IloNumVar(env, 0, 1, ILOFLOAT);
                } else if (t < win_end) {
                    Y[g][t] = IloNumVar(env, 0, 1, ILOBOOL);
                    Lambda[g][t] = IloNumVar(env, 0, 1, ILOBOOL);
                } else {
                    Y[g][t] = IloNumVar(env, 0, 1, ILOFLOAT);
                    Lambda[g][t] = IloNumVar(env, 0, 1, ILOFLOAT);
                }
            }
        }

        if (is_final) {
            U = IloNumVarArray(env, N, 0, 1, ILOBOOL);
        } else {
            U = IloNumVarArray(env, N, 0, 1, ILOFLOAT);
        }

        for (int g = 0; g < G; g++) {
            for (int t = 0; t < k; t++) {
                model.add(Y[g][t] == state.y_bar[g][t]);
                model.add(Lambda[g][t] == state.lambda_bar[g][t]);
            }
        }

        IloExpr objective(env);
        for (int i = 0; i < N; i++) {
            for (int t = 0; t < T; t++) {
                objective += lists.cost_x[i] * X[i][t];
            }
            // 欠交惩罚 (仅 t >= l_i)
            for (int t = lists.lw_x[i]; t < T; t++) {
                objective += lists.cost_b[i] * B[i][t];
            }
        }
        for (int g = 0; g < G; g++) {
            for (int t = 0; t < T; t++) {
                objective += lists.cost_y[g] * Y[g][t];
            }
        }
        for (int f = 0; f < F; f++) {
            for (int t = 0; t < T; t++) {
                objective += lists.cost_i[f] * I[f][t];
            }
        }
        for (int i = 0; i < N; i++) {
            objective += lists.cost_u[i] * U[i];
        }
        model.add(IloMinimize(env, objective));
        objective.end();

        // 需求满足
        for (int i = 0; i < N; i++) {
            IloExpr total_production(env);
            for (int t = 0; t < T; t++) {
                total_production += X[i][t];
            }
            model.add(total_production + U[i] * lists.final_demand[i] >= lists.final_demand[i]);
            total_production.end();
        }

        // 产能约束
        for (int t = 0; t < T; t++) {
            IloExpr capacity(env);
            for (int i = 0; i < N; i++) {
                capacity += lists.usage_x[i] * X[i][t];
            }
            for (int g = 0; g < G; g++) {
                capacity += lists.usage_y[g] * Y[g][t];
            }
            model.add(capacity <= values.machine_capacity);
            capacity.end();
        }

        // 家族 Big-M 约束
        for (int g = 0; g < G; g++) {
            for (int t = 0; t < T; t++) {
                IloExpr family_production(env);
                for (int i = 0; i < N; i++) {
                    if (lists.group_flag[i][g]) {
                        family_production += lists.usage_x[i] * X[i][t];
                    }
                }
                model.add(family_production <= values.machine_capacity * (Y[g][t] + Lambda[g][t]));
                family_production.end();
            }
        }

        // 下游流平衡
        for (int f = 0; f < F; f++) {
            for (int t = 0; t < T; t++) {
                IloExpr flow_production(env);
                for (int i = 0; i < N; i++) {
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

        // 下游能力
        for (int f = 0; f < F; f++) {
            for (int t = 0; t < T; t++) {
                model.add(P[f][t] <= lists.period_demand[f][t]);
            }
        }

        // 最早生产期约束 (仅约束 t < e_i)
        for (int i = 0; i < N; i++) {
            for (int t = 0; t < T; t++) {
                if (t < lists.ew_x[i]) {
                    model.add(X[i][t] == 0);
                }
            }
        }

        // 欠交定义
        for (int i = 0; i < N; i++) {
            for (int t = 0; t < T; t++) {
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
        for (int i = 0; i < N; i++) {
            int last_t = T - 1;
            model.add(lists.final_demand[i] * U[i] >= B[i][last_t]);
        }

        // 约束10: 初始条件 - lambda_g0 = 0 (第一周期无跨期)
        for (int g = 0; g < G; g++) {
            model.add(Lambda[g][0] == 0);
        }
        // 约束7: 每周期最多一个carryover
        for (int t = 0; t < T; t++) {
            IloExpr sum_lambda(env);
            for (int g = 0; g < G; g++) {
                sum_lambda += Lambda[g][t];
            }
            model.add(sum_lambda <= 1);
            sum_lambda.end();
        }
        // 约束8: Carryover可行性
        for (int g = 0; g < G; g++) {
            for (int t = 1; t < T; t++) {
                model.add(Y[g][t-1] + Lambda[g][t-1] - Lambda[g][t] >= 0);
            }
        }
        // 约束9: Carryover排他性
        for (int g = 0; g < G; g++) {
            for (int t = 1; t < T; t++) {
                IloExpr sum_other_y(env);
                for (int g2 = 0; g2 < G; g2++) {
                    if (g2 != g) {
                        sum_other_y += Y[g2][t];
                    }
                }
                model.add(Lambda[g][t] + Lambda[g][t-1] + Y[g][t] - sum_other_y <= 2);
                sum_other_y.end();
            }
        }

        IloCplex cplex(model);
        cplex.setParam(IloCplex::TiLim, kRFSubproblemTimeLimit);
        cplex.setParam(IloCplex::Threads, values.cplex_threads);
        cplex.setParam(IloCplex::Param::MIP::Strategy::File, 3);
        cplex.setParam(IloCplex::Param::WorkDir, values.cplex_workdir.c_str());
        cplex.setParam(IloCplex::Param::WorkMem, values.cplex_workmem);

        // CPLEX 日志输出到双向流
        if (g_logger) {
            cplex.setOut(g_logger->GetTeeStream());
        }
        LOG("\n=============== CPLEX START ===============");

        bool solved = cplex.solve();

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

        if (solved && has_incumbent) {
            double obj_value = cplex.getObjValue();
            double cpu_time = cplex.getTime();
            LOG_FMT("  [RF] 求解成功: 目标=%.2f\n", obj_value);

            if (objective_out != nullptr) *objective_out = obj_value;
            if (cpu_time_out != nullptr) *cpu_time_out = cpu_time;

            y_solution.assign(G, vector<int>(T, 0));
            lambda_solution.assign(G, vector<int>(T, 0));

            for (int g = 0; g < G; g++) {
                for (int t = 0; t < T; t++) {
                    y_solution[g][t] = (cplex.getValue(Y[g][t]) > 0.5) ? 1 : 0;
                    lambda_solution[g][t] = (cplex.getValue(Lambda[g][t]) > 0.5) ? 1 : 0;
                }
            }

            env.end();
            return true;
        } else {
            LOG("  [RF] 求解失败或无可行解");
            env.end();
            return false;
        }

    } catch (IloException& e) {
        LOG_FMT("  [RF] CPLEX错误: %s\n", e.getMessage());
        return false;
    } catch (...) {
        LOG("  [RF] 未知错误");
        return false;
    }
}

// 固定周期
static void FixPeriods(int k, int S, RFState& state,
                       const vector<vector<int>>& y_solution,
                       const vector<vector<int>>& lambda_solution,
                       int T)
{
    int fix_end = min(k + S, T);
    int G = static_cast<int>(state.y_bar.size());

    for (int t = k; t < fix_end; t++) {
        for (int g = 0; g < G; g++) {
            state.y_bar[g][t] = y_solution[g][t];
            state.lambda_bar[g][t] = lambda_solution[g][t];
        }
        state.period_fixed[t] = true;
    }

    state.rollback_stack.push_back({k, fix_end});
    LOG_FMT("  [RF] 固定周期 [%d, %d)\n", k, fix_end);
}

// 回滚
static bool Rollback(RFState& state, int& k, int& W) {
    if (state.rollback_stack.empty()) {
        LOG("  [RF] 回滚栈为空");
        return false;
    }

    auto last_fix = state.rollback_stack.back();
    state.rollback_stack.pop_back();

    int start_t = last_fix.first;
    int end_t = last_fix.second;
    int G = static_cast<int>(state.y_bar.size());

    for (int t = start_t; t < end_t; t++) {
        for (int g = 0; g < G; g++) {
            state.y_bar[g][t] = 0;
            state.lambda_bar[g][t] = 0;
        }
        state.period_fixed[t] = false;
    }

    k = start_t;
    W = kRFWindowSize + 2;

    LOG_FMT("  [RF] 回滚至周期 %d\n", k);
    return true;
}

// RF 最终求解
static bool SolveRFFinal(RFState& state, AllValues& values, AllLists& lists,
                          double& final_objective, double& final_cpu_time) {
    LOG("\n[RF] 最终求解...");

    int T = values.number_of_periods;
    vector<vector<int>> y_solution, lambda_solution;
    double objective = -1.0;
    double cpu_time = 0.0;

    bool success = SolveRFSubproblem(T, 0, state, values, lists,
                                      y_solution, lambda_solution, true,
                                      &objective, &cpu_time);

    if (success) {
        final_objective = objective;
        final_cpu_time = cpu_time;
    }

    return success;
}

// RF 主循环
static bool RunRFPhase(AllValues& values, AllLists& lists, RFState& state,
                       double& rf_objective, double& rf_cpu_time) {
    LOG("\n[RF] 启动 Relax-and-Fix 阶段");
    LOG_FMT("[RF] 参数: W=%d S=%d R=%d\n", kRFWindowSize, kRFFixStep, kRFMaxRetries);

    InitRFState(state, values);

    int T = values.number_of_periods;
    int k = 0;
    int W = kRFWindowSize;
    double total_cpu_time = 0.0;

    vector<vector<int>> y_solution, lambda_solution;

    while (k < T) {
        state.iterations++;
        LOG_FMT("\n[RF] 迭代 %d: k=%d\n", state.iterations, k);

        double iter_cpu_time = 0.0;
        bool feasible = SolveRFSubproblem(k, W, state, values, lists,
                                           y_solution, lambda_solution,
                                           false, nullptr, &iter_cpu_time);
        total_cpu_time += iter_cpu_time;

        if (feasible) {
            FixPeriods(k, kRFFixStep, state, y_solution, lambda_solution, T);
            k += kRFFixStep;
            W = kRFWindowSize;
        } else {
            bool resolved = false;
            for (int r = 0; r < kRFMaxRetries && !resolved; r++) {
                W++;
                LOG_FMT("  [RF] 扩展窗口重试 %d/%d\n", r + 1, kRFMaxRetries);
                iter_cpu_time = 0.0;
                resolved = SolveRFSubproblem(k, W, state, values, lists,
                                              y_solution, lambda_solution,
                                              false, nullptr, &iter_cpu_time);
                total_cpu_time += iter_cpu_time;
            }

            if (resolved) {
                FixPeriods(k, kRFFixStep, state, y_solution, lambda_solution, T);
                k += kRFFixStep;
                W = kRFWindowSize;
            } else {
                if (!Rollback(state, k, W)) {
                    LOG("[RF] 算法终止");
                    rf_cpu_time = total_cpu_time;
                    return false;
                }
            }
        }
    }

    double final_obj = -1.0, final_cpu = 0.0;
    bool final_success = SolveRFFinal(state, values, lists, final_obj, final_cpu);
    total_cpu_time += final_cpu;

    rf_objective = final_obj;
    rf_cpu_time = total_cpu_time;

    return final_success;
}

// ============================================================================
// FO (Fix-and-Optimize) 部分
// ============================================================================

// 初始化 FO 状态
static void InitFOState(FOState& fo_state, const RFState& rf_state,
                        double initial_objective) {
    fo_state.y_current = rf_state.y_bar;
    fo_state.lambda_current = rf_state.lambda_bar;
    fo_state.current_objective = initial_objective;
    fo_state.rounds_completed = 0;
    fo_state.windows_improved = 0;
}

// 求解 FO 邻域子问题 NSP(a)
// 窗口 WND+(a) 内的 (y, lambda) 为整数，窗口外固定
static bool SolveFOSubproblem(
    int a,  // 窗口起点
    const FOState& fo_state,
    AllValues& values,
    AllLists& lists,
    vector<vector<int>>& y_solution,
    vector<vector<int>>& lambda_solution,
    double* objective_out = nullptr,
    double* cpu_time_out = nullptr)
{
    int G = values.number_of_groups;
    int T = values.number_of_periods;
    int N = values.number_of_items;
    int F = values.number_of_flows;

    // 计算扩展窗口 WND+(a)
    int wnd_start = max(0, a - kFOBoundaryBuffer);
    int wnd_end = min(T, a + kFOWindowSize + kFOBoundaryBuffer);

    LOG_FMT("  [FO] 子问题: a=%d WND+=[%d,%d)\n", a, wnd_start, wnd_end);

    try {
        IloEnv env;
        IloModel model(env);

        IloArray<IloNumVarArray> X(env, N);
        IloArray<IloNumVarArray> Y(env, G);
        IloArray<IloNumVarArray> Lambda(env, G);
        IloArray<IloNumVarArray> I(env, F);
        IloArray<IloNumVarArray> P(env, F);
        IloArray<IloNumVarArray> B(env, N);
        IloNumVarArray U(env, N, 0, 1, ILOBOOL);  // FO中u为整数

        for (int i = 0; i < N; i++) {
            X[i] = IloNumVarArray(env, T, 0, IloInfinity);
            B[i] = IloNumVarArray(env, T, 0, IloInfinity);
        }

        for (int f = 0; f < F; f++) {
            I[f] = IloNumVarArray(env, T, 0, IloInfinity);
            P[f] = IloNumVarArray(env, T, 0, IloInfinity);
        }

        // Y, Lambda: 窗口内整数，窗口外固定
        for (int g = 0; g < G; g++) {
            Y[g] = IloNumVarArray(env, T);
            Lambda[g] = IloNumVarArray(env, T);

            for (int t = 0; t < T; t++) {
                if (t >= wnd_start && t < wnd_end) {
                    // 窗口内: 整数变量
                    Y[g][t] = IloNumVar(env, 0, 1, ILOBOOL);
                    Lambda[g][t] = IloNumVar(env, 0, 1, ILOBOOL);
                } else {
                    // 窗口外: 固定为当前值
                    Y[g][t] = IloNumVar(env, 0, 1, ILOFLOAT);
                    Lambda[g][t] = IloNumVar(env, 0, 1, ILOFLOAT);
                }
            }
        }

        // 固定窗口外的变量
        for (int g = 0; g < G; g++) {
            for (int t = 0; t < T; t++) {
                if (t < wnd_start || t >= wnd_end) {
                    model.add(Y[g][t] == fo_state.y_current[g][t]);
                    model.add(Lambda[g][t] == fo_state.lambda_current[g][t]);
                }
            }
        }

        // 目标函数
        IloExpr objective(env);
        for (int i = 0; i < N; i++) {
            for (int t = 0; t < T; t++) {
                objective += lists.cost_x[i] * X[i][t];
            }
            // 欠交惩罚 (仅 t >= l_i)
            for (int t = lists.lw_x[i]; t < T; t++) {
                objective += lists.cost_b[i] * B[i][t];
            }
        }
        for (int g = 0; g < G; g++) {
            for (int t = 0; t < T; t++) {
                objective += lists.cost_y[g] * Y[g][t];
            }
        }
        for (int f = 0; f < F; f++) {
            for (int t = 0; t < T; t++) {
                objective += lists.cost_i[f] * I[f][t];
            }
        }
        for (int i = 0; i < N; i++) {
            objective += lists.cost_u[i] * U[i];
        }
        model.add(IloMinimize(env, objective));
        objective.end();

        // 约束 (与 RF 相同)
        // 需求满足
        for (int i = 0; i < N; i++) {
            IloExpr total_production(env);
            for (int t = 0; t < T; t++) {
                total_production += X[i][t];
            }
            model.add(total_production + U[i] * lists.final_demand[i] >= lists.final_demand[i]);
            total_production.end();
        }

        // 产能约束
        for (int t = 0; t < T; t++) {
            IloExpr capacity(env);
            for (int i = 0; i < N; i++) {
                capacity += lists.usage_x[i] * X[i][t];
            }
            for (int g = 0; g < G; g++) {
                capacity += lists.usage_y[g] * Y[g][t];
            }
            model.add(capacity <= values.machine_capacity);
            capacity.end();
        }

        // 家族 Big-M 约束
        for (int g = 0; g < G; g++) {
            for (int t = 0; t < T; t++) {
                IloExpr family_production(env);
                for (int i = 0; i < N; i++) {
                    if (lists.group_flag[i][g]) {
                        family_production += lists.usage_x[i] * X[i][t];
                    }
                }
                model.add(family_production <= values.machine_capacity * (Y[g][t] + Lambda[g][t]));
                family_production.end();
            }
        }

        // 下游流平衡
        for (int f = 0; f < F; f++) {
            for (int t = 0; t < T; t++) {
                IloExpr flow_production(env);
                for (int i = 0; i < N; i++) {
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

        // 下游能力
        for (int f = 0; f < F; f++) {
            for (int t = 0; t < T; t++) {
                model.add(P[f][t] <= lists.period_demand[f][t]);
            }
        }

        // 最早生产期约束 (仅约束 t < e_i)
        for (int i = 0; i < N; i++) {
            for (int t = 0; t < T; t++) {
                if (t < lists.ew_x[i]) {
                    model.add(X[i][t] == 0);
                }
            }
        }

        // 欠交定义
        for (int i = 0; i < N; i++) {
            for (int t = 0; t < T; t++) {
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
        for (int i = 0; i < N; i++) {
            int last_t = T - 1;
            model.add(lists.final_demand[i] * U[i] >= B[i][last_t]);
        }

        // 约束10: 初始条件 - lambda_g0 = 0 (第一周期无跨期)
        for (int g = 0; g < G; g++) {
            model.add(Lambda[g][0] == 0);
        }
        // 约束7: 每周期最多一个carryover
        for (int t = 0; t < T; t++) {
            IloExpr sum_lambda(env);
            for (int g = 0; g < G; g++) {
                sum_lambda += Lambda[g][t];
            }
            model.add(sum_lambda <= 1);
            sum_lambda.end();
        }
        // 约束8: Carryover可行性
        for (int g = 0; g < G; g++) {
            for (int t = 1; t < T; t++) {
                model.add(Y[g][t-1] + Lambda[g][t-1] - Lambda[g][t] >= 0);
            }
        }
        // 约束9: Carryover排他性
        for (int g = 0; g < G; g++) {
            for (int t = 1; t < T; t++) {
                IloExpr sum_other_y(env);
                for (int g2 = 0; g2 < G; g2++) {
                    if (g2 != g) {
                        sum_other_y += Y[g2][t];
                    }
                }
                model.add(Lambda[g][t] + Lambda[g][t-1] + Y[g][t] - sum_other_y <= 2);
                sum_other_y.end();
            }
        }

        IloCplex cplex(model);
        cplex.setParam(IloCplex::TiLim, kFOSubproblemTimeLimit);
        cplex.setParam(IloCplex::Threads, values.cplex_threads);
        cplex.setParam(IloCplex::Param::MIP::Strategy::File, 3);
        cplex.setParam(IloCplex::Param::WorkDir, values.cplex_workdir.c_str());
        cplex.setParam(IloCplex::Param::WorkMem, values.cplex_workmem);

        // CPLEX 日志输出到双向流
        if (g_logger) {
            cplex.setOut(g_logger->GetTeeStream());
        }
        LOG("\n=============== CPLEX START ===============");

        bool solved = cplex.solve();

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

        if (solved && has_incumbent) {
            double obj_value = cplex.getObjValue();
            double cpu_time = cplex.getTime();
            LOG_FMT("  [FO] 求解成功: 目标=%.2f\n", obj_value);

            if (objective_out != nullptr) *objective_out = obj_value;
            if (cpu_time_out != nullptr) *cpu_time_out = cpu_time;

            y_solution.assign(G, vector<int>(T, 0));
            lambda_solution.assign(G, vector<int>(T, 0));

            for (int g = 0; g < G; g++) {
                for (int t = 0; t < T; t++) {
                    y_solution[g][t] = (cplex.getValue(Y[g][t]) > 0.5) ? 1 : 0;
                    lambda_solution[g][t] = (cplex.getValue(Lambda[g][t]) > 0.5) ? 1 : 0;
                }
            }

            env.end();
            return true;
        } else {
            LOG("  [FO] 求解失败");
            env.end();
            return false;
        }

    } catch (IloException& e) {
        LOG_FMT("  [FO] CPLEX错误: %s\n", e.getMessage());
        return false;
    } catch (...) {
        LOG("  [FO] 未知错误");
        return false;
    }
}

// FO 主循环
static void RunFOPhase(AllValues& values, AllLists& lists,
                       const RFState& rf_state, double rf_objective,
                       FOState& fo_state, double& fo_cpu_time) {
    LOG("\n[FO] 启动 Fix-and-Optimize 阶段");
    LOG_FMT("[FO] 参数: W_o=%d S_o=%d H=%d Delta=%d\n",
            kFOWindowSize, kFOStep, kFOMaxRounds, kFOBoundaryBuffer);

    InitFOState(fo_state, rf_state, rf_objective);
    fo_cpu_time = 0.0;

    int T = values.number_of_periods;

    for (int h = 1; h <= kFOMaxRounds; h++) {
        LOG_FMT("\n[FO] 轮次 %d/%d\n", h, kFOMaxRounds);

        bool improved_in_round = false;
        int windows_in_round = 0;

        // 滑动窗口
        for (int a = 0; a < T; a += kFOStep) {
            windows_in_round++;
            vector<vector<int>> y_solution, lambda_solution;
            double obj = -1.0, cpu = 0.0;

            bool feasible = SolveFOSubproblem(a, fo_state, values, lists,
                                               y_solution, lambda_solution,
                                               &obj, &cpu);
            fo_cpu_time += cpu;

            if (feasible && obj < fo_state.current_objective - 1e-6) {
                // 严格改进
                double improvement = fo_state.current_objective - obj;
                LOG_FMT("  [FO] 改进! %.2f -> %.2f (减少 %.2f)\n",
                        fo_state.current_objective, obj, improvement);

                fo_state.y_current = y_solution;
                fo_state.lambda_current = lambda_solution;
                fo_state.current_objective = obj;
                fo_state.windows_improved++;
                improved_in_round = true;
            }
        }

        fo_state.rounds_completed = h;
        LOG_FMT("[FO] 轮次 %d 完成: 窗口数=%d 当前目标=%.2f\n",
                h, windows_in_round, fo_state.current_objective);

        if (!improved_in_round) {
            LOG("[FO] 无改进，提前终止");
            break;
        }
    }
}

// FO 最终收尾求解
static bool SolveFOFinal(FOState& fo_state, AllValues& values, AllLists& lists,
                          double& final_objective, double& final_cpu_time) {
    LOG("\n[FO] 最终收尾求解...");

    int G = values.number_of_groups;
    int T = values.number_of_periods;
    int N = values.number_of_items;
    int F = values.number_of_flows;

    try {
        IloEnv env;
        IloModel model(env);

        IloArray<IloNumVarArray> X(env, N);
        IloArray<IloNumVarArray> Y(env, G);
        IloArray<IloNumVarArray> Lambda(env, G);
        IloArray<IloNumVarArray> I(env, F);
        IloArray<IloNumVarArray> P(env, F);
        IloArray<IloNumVarArray> B(env, N);
        IloNumVarArray U(env, N, 0, 1, ILOBOOL);

        for (int i = 0; i < N; i++) {
            X[i] = IloNumVarArray(env, T, 0, IloInfinity);
            B[i] = IloNumVarArray(env, T, 0, IloInfinity);
        }

        for (int f = 0; f < F; f++) {
            I[f] = IloNumVarArray(env, T, 0, IloInfinity);
            P[f] = IloNumVarArray(env, T, 0, IloInfinity);
        }

        // 固定所有 (y, lambda)
        for (int g = 0; g < G; g++) {
            Y[g] = IloNumVarArray(env, T);
            Lambda[g] = IloNumVarArray(env, T);
            for (int t = 0; t < T; t++) {
                Y[g][t] = IloNumVar(env, 0, 1, ILOFLOAT);
                Lambda[g][t] = IloNumVar(env, 0, 1, ILOFLOAT);
                model.add(Y[g][t] == fo_state.y_current[g][t]);
                model.add(Lambda[g][t] == fo_state.lambda_current[g][t]);
            }
        }

        // 目标函数
        IloExpr objective(env);
        for (int i = 0; i < N; i++) {
            for (int t = 0; t < T; t++) {
                objective += lists.cost_x[i] * X[i][t];
            }
            // 欠交惩罚 (仅 t >= l_i)
            for (int t = lists.lw_x[i]; t < T; t++) {
                objective += lists.cost_b[i] * B[i][t];
            }
        }
        for (int g = 0; g < G; g++) {
            for (int t = 0; t < T; t++) {
                objective += lists.cost_y[g] * Y[g][t];
            }
        }
        for (int f = 0; f < F; f++) {
            for (int t = 0; t < T; t++) {
                objective += lists.cost_i[f] * I[f][t];
            }
        }
        for (int i = 0; i < N; i++) {
            objective += lists.cost_u[i] * U[i];
        }
        model.add(IloMinimize(env, objective));
        objective.end();

        // 约束 (同上)
        for (int i = 0; i < N; i++) {
            IloExpr total_production(env);
            for (int t = 0; t < T; t++) {
                total_production += X[i][t];
            }
            model.add(total_production + U[i] * lists.final_demand[i] >= lists.final_demand[i]);
            total_production.end();
        }

        for (int t = 0; t < T; t++) {
            IloExpr capacity(env);
            for (int i = 0; i < N; i++) {
                capacity += lists.usage_x[i] * X[i][t];
            }
            for (int g = 0; g < G; g++) {
                capacity += lists.usage_y[g] * Y[g][t];
            }
            model.add(capacity <= values.machine_capacity);
            capacity.end();
        }

        for (int g = 0; g < G; g++) {
            for (int t = 0; t < T; t++) {
                IloExpr family_production(env);
                for (int i = 0; i < N; i++) {
                    if (lists.group_flag[i][g]) {
                        family_production += lists.usage_x[i] * X[i][t];
                    }
                }
                model.add(family_production <= values.machine_capacity * (Y[g][t] + Lambda[g][t]));
                family_production.end();
            }
        }

        for (int f = 0; f < F; f++) {
            for (int t = 0; t < T; t++) {
                IloExpr flow_production(env);
                for (int i = 0; i < N; i++) {
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

        for (int f = 0; f < F; f++) {
            for (int t = 0; t < T; t++) {
                model.add(P[f][t] <= lists.period_demand[f][t]);
            }
        }

        for (int i = 0; i < N; i++) {
            for (int t = 0; t < T; t++) {
                if (t < lists.ew_x[i] || t > lists.lw_x[i]) {
                    model.add(X[i][t] == 0);
                }
            }
        }

        for (int i = 0; i < N; i++) {
            for (int t = 0; t < T; t++) {
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

        for (int i = 0; i < N; i++) {
            int last_t = T - 1;
            model.add(lists.final_demand[i] * U[i] >= B[i][last_t]);
        }

        // 约束10: 初始条件 - lambda_g0 = 0 (第一周期无跨期)
        for (int g = 0; g < G; g++) {
            model.add(Lambda[g][0] == 0);
        }
        // 约束7: 每周期最多一个carryover
        for (int t = 0; t < T; t++) {
            IloExpr sum_lambda(env);
            for (int g = 0; g < G; g++) {
                sum_lambda += Lambda[g][t];
            }
            model.add(sum_lambda <= 1);
            sum_lambda.end();
        }
        // 约束8: Carryover可行性
        for (int g = 0; g < G; g++) {
            for (int t = 1; t < T; t++) {
                model.add(Y[g][t-1] + Lambda[g][t-1] - Lambda[g][t] >= 0);
            }
        }
        // 约束9: Carryover排他性
        for (int g = 0; g < G; g++) {
            for (int t = 1; t < T; t++) {
                IloExpr sum_other_y(env);
                for (int g2 = 0; g2 < G; g2++) {
                    if (g2 != g) {
                        sum_other_y += Y[g2][t];
                    }
                }
                model.add(Lambda[g][t] + Lambda[g][t-1] + Y[g][t] - sum_other_y <= 2);
                sum_other_y.end();
            }
        }

        IloCplex cplex(model);
        cplex.setParam(IloCplex::TiLim, kRFSubproblemTimeLimit);
        cplex.setParam(IloCplex::Threads, values.cplex_threads);
        cplex.setParam(IloCplex::Param::MIP::Strategy::File, 3);
        cplex.setParam(IloCplex::Param::WorkDir, values.cplex_workdir.c_str());
        cplex.setParam(IloCplex::Param::WorkMem, values.cplex_workmem);

        // CPLEX 日志输出到双向流
        if (g_logger) {
            cplex.setOut(g_logger->GetTeeStream());
        }
        LOG("\n=============== CPLEX START ===============");

        bool solved = cplex.solve();

        // 求解后关闭CPLEX输出并刷新
        cplex.setOut(env.getNullStream());
        if (g_logger) g_logger->Flush();
        LOG("=============== CPLEX END =================");
        LOG_RAW("\n");

        if (solved) {
            final_objective = cplex.getObjValue();
            final_cpu_time = cplex.getTime();
            LOG_FMT("[FO] 最终目标: %.2f\n", final_objective);

            // Save X, I, B, U to AllLists for JSON output
            lists.small_x.resize(N);
            lists.small_b.resize(N);
            lists.small_u.resize(N);
            lists.small_i.resize(F);

            for (int i = 0; i < N; i++) {
                lists.small_x[i].resize(T);
                lists.small_b[i].resize(T);
                for (int t = 0; t < T; t++) {
                    lists.small_x[i][t] = cplex.getValue(X[i][t]);
                    lists.small_b[i][t] = cplex.getValue(B[i][t]);
                }
                lists.small_u[i] = cplex.getValue(U[i]);
            }

            for (int f = 0; f < F; f++) {
                lists.small_i[f].resize(T);
                for (int t = 0; t < T; t++) {
                    lists.small_i[f][t] = cplex.getValue(I[f][t]);
                }
            }

            env.end();
            return true;
        } else {
            env.end();
            return false;
        }

    } catch (IloException& e) {
        LOG_FMT("[FO] CPLEX错误: %s\n", e.getMessage());
        return false;
    } catch (...) {
        LOG("[FO] 未知错误");
        return false;
    }
}

// ============================================================================
// RFO 主入口
// ============================================================================

void SolveRFO(AllValues& values, AllLists& lists) {
    LOG("\n========================================");
    LOG("[RFO] 启动 RFO (RF + FO) 算法");
    LOG("========================================");

    auto rfo_start = chrono::steady_clock::now();

    // 阶段1: RF 构造初始解
    RFState rf_state;
    double rf_objective = -1.0;
    double rf_cpu_time = 0.0;

    bool rf_success = RunRFPhase(values, lists, rf_state, rf_objective, rf_cpu_time);

    if (!rf_success) {
        LOG("[RFO] RF阶段失败，算法终止");
        values.result_step1.objective = -1;
        values.result_step1.runtime = -1;
        values.result_step1.cpu_time = rf_cpu_time;
        return;
    }

    LOG_FMT("\n[RFO] RF阶段完成: 目标=%.2f CPU时间=%.2f秒\n", rf_objective, rf_cpu_time);

    // 阶段2: FO 改进解
    FOState fo_state;
    double fo_cpu_time = 0.0;

    RunFOPhase(values, lists, rf_state, rf_objective, fo_state, fo_cpu_time);

    LOG_FMT("\n[RFO] FO阶段完成: 目标=%.2f 改进窗口=%d CPU时间=%.2f秒\n",
            fo_state.current_objective, fo_state.windows_improved, fo_cpu_time);

    // 阶段3: 最终收尾
    double final_objective = -1.0;
    double final_cpu_time = 0.0;

    bool final_success = SolveFOFinal(fo_state, values, lists,
                                       final_objective, final_cpu_time);

    auto rfo_end = chrono::steady_clock::now();
    double rfo_wall_time = chrono::duration<double>(rfo_end - rfo_start).count();
    double total_cpu_time = rf_cpu_time + fo_cpu_time + final_cpu_time;

    // 存储结果
    lists.small_y = fo_state.y_current;
    lists.small_l = fo_state.lambda_current;

    if (final_success) {
        values.result_step1.objective = final_objective;
    } else {
        values.result_step1.objective = fo_state.current_objective;
    }
    values.result_step1.runtime = rfo_wall_time;
    values.result_step1.cpu_time = total_cpu_time;
    values.result_step1.gap = 0.0;

    // 计算改进
    double improvement = rf_objective - values.result_step1.objective;
    double improvement_pct = (rf_objective > 0) ?
                             (improvement / rf_objective * 100.0) : 0.0;

    // ========== Calculate metrics ==========
    auto& m = values.metrics;
    int T = values.number_of_periods;

    // RFO-specific metrics
    m.rfo_rf_objective = rf_objective;
    m.rfo_rf_time = rf_cpu_time;
    m.rfo_fo_rounds = fo_state.rounds_completed;
    m.rfo_fo_windows_improved = fo_state.windows_improved;
    m.rfo_fo_improvement = improvement;
    m.rfo_fo_improvement_pct = improvement_pct;
    m.rfo_fo_time = fo_cpu_time;
    m.rfo_final_solve_time = final_cpu_time;

    // Cost breakdown (from saved variables)
    m.cost_production = 0.0;
    m.cost_setup = 0.0;
    m.cost_inventory = 0.0;
    m.cost_backorder = 0.0;
    m.cost_unmet = 0.0;

    for (int i = 0; i < values.number_of_items; ++i) {
        for (int t = 0; t < T; ++t) {
            m.cost_production += lists.cost_x[i] * lists.small_x[i][t];
            m.cost_backorder += lists.cost_b[i] * lists.small_b[i][t];
        }
        m.cost_unmet += lists.cost_u[i] * lists.small_u[i];
    }

    for (int g = 0; g < values.number_of_groups; ++g) {
        for (int t = 0; t < T; ++t) {
            m.cost_setup += lists.cost_y[g] * lists.small_y[g][t];
        }
    }

    for (int f = 0; f < values.number_of_flows; ++f) {
        for (int t = 0; t < T; ++t) {
            m.cost_inventory += lists.cost_i[f] * lists.small_i[f][t];
        }
    }

    // Setup/Carryover statistics
    m.total_setups = 0;
    m.total_carryovers = 0;
    m.saved_setup_cost = 0.0;

    for (int g = 0; g < values.number_of_groups; ++g) {
        for (int t = 0; t < T; ++t) {
            if (lists.small_y[g][t] == 1) m.total_setups++;
            if (lists.small_l[g][t] == 1) {
                m.total_carryovers++;
                m.saved_setup_cost += lists.cost_y[g];
            }
        }
    }

    // Demand fulfillment
    m.unmet_count = 0;
    m.total_backorder = 0.0;
    m.total_demand = 0.0;
    int on_time_count = 0;

    for (int i = 0; i < values.number_of_items; ++i) {
        m.total_demand += lists.final_demand[i];
        if (lists.small_u[i] > 0.5) {
            m.unmet_count++;
        } else {
            int lw = lists.lw_x[i];
            if (lw < T && lists.small_b[i][lw] < 0.5) {
                on_time_count++;
            }
        }
        int T_last = T - 1;
        m.total_backorder += lists.small_b[i][T_last];
    }

    m.unmet_rate = values.number_of_items > 0
        ? (double)m.unmet_count / values.number_of_items : 0.0;
    m.on_time_rate = values.number_of_items > 0
        ? (double)on_time_count / values.number_of_items : 0.0;

    // Capacity utilization
    m.capacity_util_by_period.resize(T);
    m.capacity_util_avg = 0.0;
    m.capacity_util_max = 0.0;

    for (int t = 0; t < T; ++t) {
        double usage = 0.0;
        for (int i = 0; i < values.number_of_items; ++i) {
            usage += lists.usage_x[i] * lists.small_x[i][t];
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
    m.capacity_util_avg /= T;

    LOG("\n========================================");
    LOG("[RFO] 算法完成");
    LOG("========================================");
    LOG_FMT("[RFO] RF目标:   %.2f\n", rf_objective);
    LOG_FMT("[RFO] 最终目标: %.2f\n", values.result_step1.objective);
    LOG_FMT("[RFO] 改进:     %.2f (%.2f%%)\n", improvement, improvement_pct);
    LOG_FMT("[RFO] 总耗时:   %.2f秒\n", rfo_wall_time);
    LOG_FMT("[RFO] CPU时间:  %.2f秒\n", total_cpu_time);
}
