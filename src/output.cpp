/**
 * @file output.cpp
 * @brief 数据输出模块 - 将决策变量结果导出为CSV格式
 */

#include "optimizer.h"
#include "common.h"
#include <filesystem>
#include <ctime>
#include <chrono>

using namespace std;

// 单元格格式化函数 - 统一处理数值到字符串的转换
auto format_cell = [](double val, int t, int ew, int lw, bool mark_ewlw) -> std::string {
    if (ew != 0 || lw != 0) {
        if (t >= ew && t <= lw) {
            return (val == 0.0 ? "0" : std::to_string((long long)(val + 0.5)));
        } else {
            return (val == 0.0 ? "-" : std::to_string((long long)(val + 0.5)));
        }
    } else {
        return (val == 0.0 ? "-" : std::to_string((long long)(val + 0.5)));
    }
};

// 主输出函数 - 将所有决策变量导出为独立的CSV文件
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
                           bool is_big_order, bool is_split_order, int precision) {
    cout << "[文件输出] 导出决策变量到独立CSV文件 = " << filename << "\n";

    string timestamp = GetCurrentTimestamp();

    // CSV文件头部写入函数
    auto write_header = [&](ofstream& file, const string& variable_name, const string& description) {
        file << "# " << variable_name << " - " << description << "\n";
        file << "# CPLEX: " << cplex.getVersion()
             << " | Status: " << (cplex.getStatus() == IloAlgorithm::Optimal ? "Optimal" : "Feasible") << "\n";
        file << "# Objective: " << fixed << setprecision(2) << cplex.getObjValue()
             << " | Time: " << setprecision(3) << cplex.getTime() << "s"
             << " | Gap: " << setprecision(6) << cplex.getMIPRelativeGap() << "\n";
        file << "#\n";
    };

    string base_name = filename.substr(0, filename.find_last_of('.'));
    string timestamped_base_name = base_name + "_" + timestamp;

    // 1. 生产变量 X[i][t]
    string production_file = timestamped_base_name + "_production.csv";
    ofstream prod_file(production_file);
    if (prod_file.is_open()) {
        write_header(prod_file, "Production Variables", "X[i][t]");
        prod_file << "Item";
        for (int t = 0; t < values.number_of_periods; t++) {
            prod_file << ",Period" << (t + 1);
        }
        prod_file << "\n";

        for (int i = 0; i < values.number_of_items; i++) {
            prod_file << (i + 1);
            for (int t = 0; t < values.number_of_periods; t++) {
                double val = cplex.getValue(X[i][t]);
                std::string cell = format_cell(val, t, lists.ew_x[i], lists.lw_x[i], false);
                prod_file << "," << cell;
            }
            prod_file << "\n";
        }
        prod_file.close();
    } else {
        cout << "[错误] 无法打开文件: " << production_file << endl;
    }

    // 2. 设置变量 Y[g][t]
    string setup_file = timestamped_base_name + "_setup.csv";
    ofstream setup_f(setup_file);
    if (setup_f.is_open()) {
        write_header(setup_f, "Setup Variables", "Y[g][t]");
        setup_f << "Group";
        for (int t = 0; t < values.number_of_periods; t++) {
            setup_f << ",Period" << (t + 1);
        }
        setup_f << "\n";

        for (int g = 0; g < values.number_of_groups; g++) {
            setup_f << (g + 1);
            for (int t = 0; t < values.number_of_periods; t++) {
                int val = (int)cplex.getValue(Y[g][t]);
                std::string cell = format_cell(val, t, 0, 0, false);
                setup_f << "," << cell;
            }
            setup_f << "\n";
        }
        setup_f.close();
    } else {
        cout << "[错误] 无法打开文件: " << setup_file << endl;
    }

    // 3. 库存变量 I[f][t]
    string inventory_file = timestamped_base_name + "_inventory.csv";
    ofstream inv_file(inventory_file);
    if (inv_file.is_open()) {
        write_header(inv_file, "Inventory Variables", "I[f][t]");
        inv_file << "Flow";
        for (int t = 0; t < values.number_of_periods; t++) {
            inv_file << ",Period" << (t + 1);
        }
        inv_file << "\n";

        for (int f = 0; f < values.number_of_flows; f++) {
            inv_file << (f + 1);
            for (int t = 0; t < values.number_of_periods; t++) {
                double val = cplex.getValue(I[f][t]);
                std::string cell = format_cell(val, t, 0, 0, false);
                inv_file << "," << cell;
            }
            inv_file << "\n";
        }
        inv_file.close();
    } else {
        cout << "[错误] 无法打开文件: " << inventory_file << endl;
    }

    // 4. 延期交货变量 B[i][t]
    string backorder_file = timestamped_base_name + "_backorder.csv";
    ofstream back_file(backorder_file);
    if (back_file.is_open()) {
        write_header(back_file, "Backorder Variables", "B[i][t]");
        back_file << "Item";
        for (int t = 0; t < values.number_of_periods; t++) {
            back_file << ",Period" << (t + 1);
        }
        back_file << "\n";

        for (int i = 0; i < values.number_of_items; i++) {
            back_file << (i + 1);
            for (int t = 0; t < values.number_of_periods; t++) {
                double val = cplex.getValue(B[i][t]);
                std::string cell = format_cell(val, t, lists.ew_x[i], lists.lw_x[i], false);
                back_file << "," << cell;
            }
            back_file << "\n";
        }
        back_file.close();
    } else {
        cout << "[错误] 无法打开文件: " << backorder_file << endl;
    }

    // 5. 未满足需求变量 U[i]
    string unmet_file = timestamped_base_name + "_unmet.csv";
    ofstream unmet_f(unmet_file);
    if (unmet_f.is_open()) {
        write_header(unmet_f, "Unmet Demand Variables", "U[i]");
        unmet_f << "Item,UnmetDemand\n";
        for (int i = 0; i < values.number_of_items; i++) {
            int val = (int)cplex.getValue(U[i]);
            std::string cell = (val == 0 ? "-" : std::to_string(val));
            unmet_f << (i + 1) << "," << cell << "\n";
        }
        unmet_f.close();
    } else {
        cout << "[错误] 无法打开文件: " << unmet_file << endl;
    }

    // 6. 结转变量 L[g][t]
    string carryover_file = timestamped_base_name + "_carryover.csv";
    ofstream carry_f(carryover_file);
    if (carry_f.is_open()) {
        write_header(carry_f, "Carryover Variables", "L[g][t]");
        carry_f << "Group";
        for (int t = 0; t < values.number_of_periods; t++) {
            carry_f << ",Period" << (t + 1);
        }
        carry_f << "\n";

        for (int g = 0; g < values.number_of_groups; g++) {
            carry_f << (g + 1);
            for (int t = 0; t < values.number_of_periods; t++) {
                int val = (int)cplex.getValue(L[g][t]);
                std::string cell = format_cell(val, t, 0, 0, false);
                carry_f << "," << cell;
            }
            carry_f << "\n";
        }
        carry_f.close();
    } else {
        cout << "[错误] 无法打开文件: " << carryover_file << endl;
    }

    cout << "[导出成功] 所有决策变量已导出到独立CSV文件\n";
}
