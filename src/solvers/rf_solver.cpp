// rf_solver.cpp - Time-Window Relax-and-Fix (RF) 求解算法
//
// RF 算法按时间窗口滚动固定 (y, lambda) 变量:
//   T^fix: 已固定周期 - 变量固定到已存值
//   T^win: 当前窗口 - 变量为整数
//   T^rel: 放松周期 - 变量放松为连续

#include "optimizer.h"
#include "logger.h"

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
// 返回是否找到可行解，若可行则更新 y_solution 和 lambda_solution
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

    // 计算时间窗口边界
    int win_end = min(k + W, T);
    int rel_start = win_end;

    LOG_FMT("[RF] 子问题: k=%d W=%d (固定:[0,%d) 窗口:[%d,%d) 放松:[%d,%d))\n",
            k, W, k, k, win_end, rel_start, T);

    try {
        IloEnv env;
        IloModel model(env);

        // 决策变量
        IloArray<IloNumVarArray> X(env, N);
        IloArray<IloNumVarArray> Y(env, G);
        IloArray<IloNumVarArray> Lambda(env, G);
        IloArray<IloNumVarArray> I(env, F);
        IloArray<IloNumVarArray> P(env, F);
        IloArray<IloNumVarArray> B(env, N);
        IloNumVarArray U(env);

        // X, B 始终为连续变量
        for (int i = 0; i < N; i++) {
            X[i] = IloNumVarArray(env, T, 0, IloInfinity);
            B[i] = IloNumVarArray(env, T, 0, IloInfinity);
        }

        // I, P 始终为连续变量
        for (int f = 0; f < F; f++) {
            I[f] = IloNumVarArray(env, T, 0, IloInfinity);
            P[f] = IloNumVarArray(env, T, 0, IloInfinity);
        }

        // Y, Lambda 按时间区间设置变量类型
        for (int g = 0; g < G; g++) {
            Y[g] = IloNumVarArray(env, T);
            Lambda[g] = IloNumVarArray(env, T);

            for (int t = 0; t < T; t++) {
                if (t < k) {
                    // T^fix: 固定变量（创建为连续，后面加固定约束）
                    Y[g][t] = IloNumVar(env, 0, 1, ILOFLOAT);
                    Lambda[g][t] = IloNumVar(env, 0, 1, ILOFLOAT);
                } else if (t < win_end) {
                    // T^win: 整数变量
                    Y[g][t] = IloNumVar(env, 0, 1, ILOBOOL);
                    Lambda[g][t] = IloNumVar(env, 0, 1, ILOBOOL);
                } else {
                    // T^rel: 放松为连续变量
                    Y[g][t] = IloNumVar(env, 0, 1, ILOFLOAT);
                    Lambda[g][t] = IloNumVar(env, 0, 1, ILOFLOAT);
                }
            }
        }

        // U: 在 RF 循环中放松，最终求解时恢复整数
        if (is_final) {
            U = IloNumVarArray(env, N, 0, 1, ILOBOOL);
        } else {
            U = IloNumVarArray(env, N, 0, 1, ILOFLOAT);
        }

        // 固定 T^fix 区间的 y, lambda
        for (int g = 0; g < G; g++) {
            for (int t = 0; t < k; t++) {
                model.add(Y[g][t] == state.y_bar[g][t]);
                model.add(Lambda[g][t] == state.lambda_bar[g][t]);
            }
        }

        // 目标函数
        IloExpr objective(env);

        for (int i = 0; i < N; i++) {
            for (int t = 0; t < T; t++) {
                objective += lists.cost_x[i] * X[i][t];
                objective += values.b_penalty * B[i][t];
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
            objective += values.u_penalty * U[i];
        }

        model.add(IloMinimize(env, objective));
        objective.end();

        // 约束1: 需求满足
        for (int i = 0; i < N; i++) {
            IloExpr total_production(env);
            for (int t = 0; t < T; t++) {
                total_production += X[i][t];
            }
            model.add(total_production + U[i] * lists.final_demand[i] >= lists.final_demand[i]);
            total_production.end();
        }

        // 约束2: 产能约束
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

        // 约束3: 家族级 Big-M 约束 (含 carryover)
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

        // 约束4: 下游工序流平衡
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

        // 约束5: 下游工序能力
        for (int f = 0; f < F; f++) {
            for (int t = 0; t < T; t++) {
                model.add(P[f][t] <= lists.period_demand[f][t]);
            }
        }

        // 约束6: 时间窗约束
        for (int i = 0; i < N; i++) {
            for (int t = 0; t < T; t++) {
                if (t < lists.ew_x[i] || t > lists.lw_x[i]) {
                    model.add(X[i][t] == 0);
                }
            }
        }

        // 约束7: 欠交定义
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

        // 约束8: 终期欠交与未满足指示
        for (int i = 0; i < N; i++) {
            int last_t = T - 1;
            model.add(lists.final_demand[i] * U[i] >= B[i][last_t]);
        }

        // 约束10: 初始条件 - y_g0 = 0, lambda_g0 = 0
        for (int g = 0; g < G; g++) {
            model.add(Y[g][0] == 0);       // 第一周期不能setup
            model.add(Lambda[g][0] == 0);  // 第一周期不能carryover
        }

        // 约束7: 每期最多一个carryover - sum_g lambda_gt <= 1
        for (int t = 0; t < T; t++) {
            IloExpr sum_lambda(env);
            for (int g = 0; g < G; g++) {
                sum_lambda += Lambda[g][t];
            }
            model.add(sum_lambda <= 1);
            sum_lambda.end();
        }

        // 约束8: Carryover可行性 - y_{g,t-1} + lambda_{g,t-1} - lambda_gt >= 0
        for (int g = 0; g < G; g++) {
            for (int t = 1; t < T; t++) {
                model.add(Y[g][t-1] + Lambda[g][t-1] - Lambda[g][t] >= 0);
            }
        }

        // 约束9: Carryover排他性 - lambda_gt + lambda_{g,t-1} + y_gt - sum_{g'!=g} y_{g't} <= 2
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

        // 配置并求解
        IloCplex cplex(model);
        cplex.setParam(IloCplex::TiLim, kRFSubproblemTimeLimit);
        cplex.setParam(IloCplex::Threads, values.cplex_threads);
        cplex.setParam(IloCplex::Param::MIP::Strategy::File, 3);
        cplex.setParam(IloCplex::Param::WorkDir, values.cplex_workdir.c_str());
        cplex.setParam(IloCplex::Param::WorkMem, values.cplex_workmem);

        // 设置 CPLEX 输出到日志系统（同时输出到终端和文件）
        if (g_logger) {
            cplex.setOut(g_logger->GetTeeStream());
        }
        LOG("\n=============== CPLEX START ===============");

        bool solved = cplex.solve();

        // 求解后断开 CPLEX 输出流
        cplex.setOut(env.getNullStream());
        if (g_logger) {
            g_logger->Flush();
        }
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
            LOG_FMT("[RF] 求解成功: 目标=%.2f CPU时间=%.2fs\n", obj_value, cpu_time);

            if (objective_out) *objective_out = obj_value;
            if (cpu_time_out) *cpu_time_out = cpu_time;

            // 提取解
            y_solution.assign(G, vector<int>(T, 0));
            lambda_solution.assign(G, vector<int>(T, 0));

            for (int g = 0; g < G; g++) {
                for (int t = 0; t < T; t++) {
                    y_solution[g][t] = (cplex.getValue(Y[g][t]) > 0.5) ? 1 : 0;
                    lambda_solution[g][t] = (cplex.getValue(Lambda[g][t]) > 0.5) ? 1 : 0;
                }
            }

            // Save X, I, B, U for final solve
            if (is_final) {
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
            }

            env.end();
            return true;
        } else {
            LOG("[RF] 求解失败或无可行解");
            env.end();
            return false;
        }

    } catch (IloException& e) {
        LOG_FMT("[RF] CPLEX错误: %s\n", e.getMessage());
        return false;
    } catch (...) {
        LOG("[RF] 未知错误");
        return false;
    }
}

// 固定周期 [k, k+S) 的 y, lambda 值
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
    LOG_FMT("[RF] 固定周期 [%d, %d)\n", k, fix_end);
}

// 回滚最近一次固定
static bool Rollback(RFState& state, int& k, int& W) {
    if (state.rollback_stack.empty()) {
        LOG("[RF] 回滚栈为空，无法回滚");
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

    LOG_FMT("[RF] 回滚至周期 %d，窗口扩大至 %d\n", k, W);
    return true;
}

// 最终求解：固定所有 y, lambda，恢复 u 为整数
static bool SolveRFFinal(RFState& state, AllValues& values, AllLists& lists,
                          double& final_objective, double& final_cpu_time) {
    LOG("[RF] 最终求解（固定所有y,lambda）...");

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

        int total_setups = 0;
        int total_carryovers = 0;

        for (int g = 0; g < values.number_of_groups; g++) {
            for (int t = 0; t < T; t++) {
                if (state.y_bar[g][t] == 1) total_setups++;
                if (state.lambda_bar[g][t] == 1) total_carryovers++;
            }
        }

        LOG_FMT("[RF] 总启动数: %d，总跨期数: %d\n", total_setups, total_carryovers);
    }

    return success;
}

// RF 主求解函数
void SolveRF(AllValues& values, AllLists& lists) {
    LOG("[RF] 启动 Relax-and-Fix 算法");
    LOG_FMT("[RF] 参数: W=%d S=%d R=%d\n", kRFWindowSize, kRFFixStep, kRFMaxRetries);

    auto rf_start = chrono::steady_clock::now();

    RFState state;
    InitRFState(state, values);

    int T = values.number_of_periods;
    int k = 0;
    int W = kRFWindowSize;
    double total_cpu_time = 0.0;

    // RF metrics tracking
    int rf_window_expansions = 0;
    int rf_rollbacks = 0;
    int rf_subproblems = 0;

    vector<vector<int>> y_solution, lambda_solution;

    // 主循环
    while (k < T) {
        state.iterations++;
        LOG_FMT("[RF] 迭代 %d: k=%d\n", state.iterations, k);

        double iter_cpu_time = 0.0;
        rf_subproblems++;
        bool feasible = SolveRFSubproblem(k, W, state, values, lists,
                                           y_solution, lambda_solution,
                                           false, nullptr, &iter_cpu_time);
        total_cpu_time += iter_cpu_time;

        if (feasible) {
            FixPeriods(k, kRFFixStep, state, y_solution, lambda_solution, T);
            k += kRFFixStep;
            W = kRFWindowSize;
        } else {
            // 尝试扩展窗口
            bool resolved = false;
            for (int r = 0; r < kRFMaxRetries && !resolved; r++) {
                W++;
                rf_window_expansions++;
                LOG_FMT("[RF] 扩展窗口重试 %d/%d，W=%d\n", r + 1, kRFMaxRetries, W);
                iter_cpu_time = 0.0;
                rf_subproblems++;
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
                rf_rollbacks++;
                if (!Rollback(state, k, W)) {
                    LOG("[RF] 无法继续，算法终止");
                    values.result_step1.objective = -1;
                    values.result_step1.runtime = -1;
                    values.result_step1.cpu_time = total_cpu_time;
                    return;
                }
            }
        }
    }

    // 最终求解
    double final_objective = -1.0;
    double final_cpu_time = 0.0;
    bool final_success = SolveRFFinal(state, values, lists, final_objective, final_cpu_time);
    total_cpu_time += final_cpu_time;

    auto rf_end = chrono::steady_clock::now();
    double rf_time = chrono::duration<double>(rf_end - rf_start).count();

    if (final_success) {
        LOG("[RF] 算法完成");
        LOG_FMT("[RF] 总迭代: %d\n", state.iterations);
        LOG_FMT("[RF] 总耗时: %.3fs\n", rf_time);
        LOG_FMT("[RF] CPU时间: %.3fs\n", total_cpu_time);
        LOG_FMT("[RF] 最终目标: %.2f\n", final_objective);

        lists.small_y = state.y_bar;
        lists.small_l = state.lambda_bar;

        values.result_step1.objective = final_objective;
        values.result_step1.runtime = rf_time;
        values.result_step1.cpu_time = total_cpu_time;
        values.result_step1.gap = 0.0;

        // ========== Calculate metrics ==========
        auto& m = values.metrics;

        // RF-specific metrics
        m.rf_iterations = state.iterations;
        m.rf_window_expansions = rf_window_expansions;
        m.rf_rollbacks = rf_rollbacks;
        m.rf_subproblems = rf_subproblems;
        m.rf_avg_subproblem_time = rf_subproblems > 0
            ? (total_cpu_time - final_cpu_time) / rf_subproblems : 0.0;
        m.rf_final_solve_time = final_cpu_time;

        // Cost breakdown (from saved variables)
        m.cost_production = 0.0;
        m.cost_setup = 0.0;
        m.cost_inventory = 0.0;
        m.cost_backorder = 0.0;
        m.cost_unmet = 0.0;

        for (int i = 0; i < values.number_of_items; ++i) {
            for (int t = 0; t < T; ++t) {
                m.cost_production += lists.cost_x[i] * lists.small_x[i][t];
                m.cost_backorder += values.b_penalty * lists.small_b[i][t];
            }
            m.cost_unmet += values.u_penalty * lists.small_u[i];
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

    } else {
        LOG("[RF] 最终求解失败");
        values.result_step1.objective = -1;
        values.result_step1.runtime = rf_time;
        values.result_step1.cpu_time = total_cpu_time;
    }
}
