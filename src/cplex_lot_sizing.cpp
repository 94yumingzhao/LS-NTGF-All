/**
 * @file cplex_lot_sizing.cpp
 * @brief CPLEX直接求解 - PP-GCB规范的MILP模型
 */

#include "optimizer.h"
#include "common.h"
#include <chrono>
#include <ctime>

// PP-GCB完整模型求解
// 决策变量: x_it, y_gt, lambda_gt, I_ft, P_ft, b_it, u_i
void SolveCplexLotSizing(AllValues& values, AllLists& lists, const string& output_dir) {
    cout << "[CPLEX求解-PP-GCB] 启动求解器...\n";
    cout << "[模型规模] 产能=" << values.machine_capacity
         << " | 订单数=" << values.number_of_items
         << " | 时段数=" << values.number_of_periods
         << " | 组数=" << values.number_of_groups
         << " | 流程数=" << values.number_of_flows << "\n";

    try {
        auto wall_start = std::chrono::steady_clock::now();
        IloEnv env;
        IloModel model(env);

        // 决策变量定义
        IloArray<IloNumVarArray> X(env, values.number_of_items);      // x_it: 生产量
        IloArray<IloNumVarArray> Y(env, values.number_of_groups);     // y_gt: setup
        IloArray<IloNumVarArray> Lambda(env, values.number_of_groups); // lambda_gt: carryover
        IloArray<IloNumVarArray> I(env, values.number_of_flows);      // I_ft: 库存
        IloArray<IloNumVarArray> P(env, values.number_of_flows);      // P_ft: 处理量
        IloArray<IloNumVarArray> B(env, values.number_of_items);      // b_it: 欠交量
        IloNumVarArray U(env, values.number_of_items, 0, 1, ILOBOOL); // u_i: 未满足

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

        // 目标函数: 最小化总成本
        IloExpr objective(env);

        for (int i = 0; i < values.number_of_items; ++i) {
            for (int t = 0; t < values.number_of_periods; ++t) {
                objective += lists.cost_x[i] * X[i][t];
            }
        }

        for (int i = 0; i < values.number_of_items; ++i) {
            for (int t = lists.lw_x[i]; t < values.number_of_periods; ++t) {
                objective += values.b_penalty * B[i][t];
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
            objective += values.u_penalty * U[i];
        }

        model.add(IloMinimize(env, objective));
        objective.end();

        // 约束(2): 流程平衡 - sum k_if*x_it + I_f,t-1 - P_ft - I_ft = 0
        for (int f = 0; f < values.number_of_flows; ++f) {
            for (int t = 0; t < values.number_of_periods; ++t) {
                IloExpr production_flow(env);
                for (int i = 0; i < values.number_of_items; ++i) {
                    production_flow += lists.flow_flag[i][f] * X[i][t];
                }

                if (t == 0) {
                    model.add(production_flow - P[f][t] - I[f][t] == 0);
                } else {
                    model.add(I[f][t-1] + production_flow - P[f][t] - I[f][t] == 0);
                }
                production_flow.end();
            }
        }

        // 约束(3): 下游工序处理能力 - P_ft <= D_ft
        for (int f = 0; f < values.number_of_flows; ++f) {
            for (int t = 0; t < values.number_of_periods; ++t) {
                model.add(P[f][t] <= lists.period_demand[f][t]);
            }
        }

        // 约束(4.1): 终期欠交与未满足指示 - d_i * u_i >= b_i,T
        for (int i = 0; i < values.number_of_items; ++i) {
            int T_final = values.number_of_periods - 1;
            model.add(lists.final_demand[i] * U[i] >= B[i][T_final]);
        }

        // 约束(5): 产能约束 - sum s_x[i]*x_it + sum s_y[g]*y_gt <= C_t
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

        // 约束(6.1): 家族生产需要setup或carryover
        // sum_{i:h_ig=1} s_x[i]*x_it <= C_t*(y_gt + lambda_gt)
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

        // 约束(7): 每期最多一个carryover - sum_g lambda_gt <= 1
        for (int t = 0; t < values.number_of_periods; ++t) {
            IloExpr sum_lambda(env);
            for (int g = 0; g < values.number_of_groups; ++g) {
                sum_lambda += Lambda[g][t];
            }
            model.add(sum_lambda <= 1);
            sum_lambda.end();
        }

        // 约束(8): Carryover延续性 - y_gt + lambda_gt - lambda_g,t-1 >= 0
        for (int g = 0; g < values.number_of_groups; ++g) {
            for (int t = 1; t < values.number_of_periods; ++t) {
                model.add(Y[g][t] + Lambda[g][t] - Lambda[g][t-1] >= 0);
            }
        }

        // 约束(9): Carryover结构限制 - 2*lambda_gt <= y_g,t-1 + y_gt
        for (int g = 0; g < values.number_of_groups; ++g) {
            for (int t = 1; t < values.number_of_periods; ++t) {
                model.add(2 * Lambda[g][t] <= Y[g][t-1] + Y[g][t]);
            }
        }

        // 约束(10): 初始状态 - y_g0 = 0, lambda_g0 = 0
        for (int g = 0; g < values.number_of_groups; ++g) {
            model.add(Y[g][0] == 0);
            model.add(Lambda[g][0] == 0);
        }

        // 约束(13): 时间窗约束
        for (int i = 0; i < values.number_of_items; ++i) {
            for (int t = 0; t < values.number_of_periods; ++t) {
                if (t < lists.ew_x[i] || t > lists.lw_x[i]) {
                    model.add(X[i][t] == 0);
                }
            }
        }

        // 约束(14): 欠交动态定义 - d_i - sum_{tau<=t} x_itau = b_it for t >= l_i
        for (int i = 0; i < values.number_of_items; ++i) {
            for (int t = lists.lw_x[i]; t < values.number_of_periods; ++t) {
                IloExpr cumulative_production(env);
                for (int tau = 0; tau <= t; ++tau) {
                    cumulative_production += X[i][tau];
                }
                model.add(lists.final_demand[i] - cumulative_production == B[i][t]);
                cumulative_production.end();
            }
        }

        // 求解器配置
        IloCplex cplex(model);
        cplex.setParam(IloCplex::TiLim, values.cpx_runtime_limit);
        cplex.setParam(IloCplex::Threads, values.cplex_threads);
        cplex.setParam(IloCplex::Param::MIP::Strategy::File, 3);
        cplex.setParam(IloCplex::Param::WorkDir, values.cplex_workdir.c_str());
        cplex.setParam(IloCplex::Param::WorkMem, values.cplex_workmem);

        cout << "[PP-GCB] 开始求解完整模型...\n";
        bool has_solution = cplex.solve();
        auto wall_end = std::chrono::steady_clock::now();
        double wall_seconds = std::chrono::duration<double>(wall_end - wall_start).count();

        // 求解结果处理
        bool has_incumbent = false;
        try {
            cplex.getObjValue();
            has_incumbent = true;
        } catch (...) {
            has_incumbent = false;
        }

        if (has_solution || has_incumbent ||
            cplex.getStatus() == IloAlgorithm::Feasible ||
            cplex.getStatus() == IloAlgorithm::Optimal) {

            if (has_incumbent) {
                string status_str;
                if (cplex.getStatus() == IloAlgorithm::Optimal) {
                    status_str = "Optimal";
                } else if (cplex.getStatus() == IloAlgorithm::Feasible) {
                    status_str = "Feasible";
                } else {
                    status_str = "Interrupted with solution";
                }

                cout << "[PP-GCB求解结果] 状态=" << status_str
                     << " | 目标值=" << cplex.getObjValue()
                     << " | 时间=" << wall_seconds << "秒"
                     << " | Gap=" << cplex.getMIPRelativeGap() << endl;

                values.result_cpx.objective = cplex.getObjValue();
                values.result_cpx.runtime = wall_seconds;
                values.result_cpx.cpu_time = cplex.getTime();
                values.result_cpx.gap = cplex.getMIPRelativeGap();

                // 成本分解
                double total_prod_cost = 0.0;
                double total_setup_cost = 0.0;
                double total_inv_cost = 0.0;
                double total_backorder_penalty = 0.0;
                double total_unmet_penalty = 0.0;
                int carryover_count = 0;

                for (int i = 0; i < values.number_of_items; ++i) {
                    for (int t = 0; t < values.number_of_periods; ++t) {
                        total_prod_cost += lists.cost_x[i] * cplex.getValue(X[i][t]);
                    }
                    for (int t = lists.lw_x[i]; t < values.number_of_periods; ++t) {
                        total_backorder_penalty += values.b_penalty * cplex.getValue(B[i][t]);
                    }
                    total_unmet_penalty += values.u_penalty * cplex.getValue(U[i]);
                }

                for (int g = 0; g < values.number_of_groups; ++g) {
                    for (int t = 0; t < values.number_of_periods; ++t) {
                        total_setup_cost += lists.cost_y[g] * cplex.getValue(Y[g][t]);
                        if (cplex.getValue(Lambda[g][t]) > 0.5) carryover_count++;
                    }
                }

                for (int f = 0; f < values.number_of_flows; ++f) {
                    for (int t = 0; t < values.number_of_periods; ++t) {
                        total_inv_cost += lists.cost_i[f] * cplex.getValue(I[f][t]);
                    }
                }

                cout << "[成本分解-PP-GCB]\n";
                cout << "  生产成本: " << total_prod_cost << "\n";
                cout << "  启动成本: " << total_setup_cost << "\n";
                cout << "  库存成本: " << total_inv_cost << "\n";
                cout << "  欠交惩罚: " << total_backorder_penalty << "\n";
                cout << "  未满足惩罚: " << total_unmet_penalty << "\n";
                cout << "  跨期次数: " << carryover_count << "\n";

                // 输出决策变量
                if (!output_dir.empty() || true) {
                    string csv_path = output_dir.empty() ? string(OUTPUT_DIR) : output_dir;
                    if (csv_path.back() != '/' && csv_path.back() != '\\') {
                        csv_path += "/";
                    }
                    csv_path += "ppgcb_full_result.csv";

                    OutputDecisionVarsCSV(
                        csv_path, values, lists, cplex, X, Y, Lambda, I, B, U,
                        false, false, false, false, false, 6
                    );
                }
            } else {
                cout << "[PP-GCB求解中断] 未找到可行解\n";
                values.result_cpx.objective = -1;
                values.result_cpx.runtime = wall_seconds;
                values.result_cpx.cpu_time = cplex.getTime();
            }
        } else {
            cout << "[PP-GCB求解失败] 状态=" << cplex.getStatus() << "\n";
            values.result_cpx.objective = -1;
            values.result_cpx.runtime = wall_seconds;
            values.result_cpx.cpu_time = cplex.getTime();
        }

        env.end();

    } catch (IloException& e) {
        cerr << "[PP-GCB错误] CPLEX异常: " << e.getMessage() << endl;
    } catch (...) {
        cerr << "[PP-GCB错误] 未知异常\n";
    }
}
