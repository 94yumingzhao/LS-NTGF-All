/**
 * @file output.cpp
 * @brief Solution output module - export results to JSON format
 */

#include "optimizer.h"
#include "common.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>

using namespace std;

// JSON string escaping helper
static string JsonEscape(const string& s) {
    string result;
    for (char c : s) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

// Output solution to JSON file
void OutputSolutionJSON(const string& filepath,
                        const string& algorithm,
                        const string& input_file,
                        const AllValues& values,
                        const AllLists& lists,
                        IloCplex& cplex,
                        IloArray<IloNumVarArray>& X,
                        IloArray<IloNumVarArray>& Y,
                        IloArray<IloNumVarArray>& L,
                        IloArray<IloNumVarArray>& I,
                        IloArray<IloNumVarArray>& B,
                        IloNumVarArray& U,
                        const vector<AlgoResult>* steps) {

    cout << "[Output] Exporting solution to JSON: " << filepath << "\n";

    ofstream fout(filepath);
    if (!fout) {
        cout << "[Error] Cannot open file: " << filepath << endl;
        return;
    }

    fout << fixed;

    // Calculate unmet rate
    int unmet_count = 0;
    for (int i = 0; i < values.number_of_items; i++) {
        if (cplex.getValue(U[i]) > 0.5) {
            unmet_count++;
        }
    }
    double unmet_rate = values.number_of_items > 0
        ? (double)unmet_count / values.number_of_items
        : 0.0;

    // Start JSON
    fout << "{\n";

    // Summary section
    fout << "  \"summary\": {\n";
    fout << "    \"algorithm\": \"" << JsonEscape(algorithm) << "\",\n";
    fout << "    \"input_file\": \"" << JsonEscape(input_file) << "\",\n";
    fout << "    \"cplex_version\": \"" << cplex.getVersion() << "\",\n";
    fout << "    \"status\": \"" << (cplex.getStatus() == IloAlgorithm::Optimal ? "Optimal" : "Feasible") << "\",\n";
    fout << setprecision(2);
    fout << "    \"objective\": " << cplex.getObjValue() << ",\n";
    fout << setprecision(3);
    fout << "    \"solve_time\": " << cplex.getTime() << ",\n";
    fout << setprecision(6);
    fout << "    \"gap\": " << cplex.getMIPRelativeGap() << ",\n";
    fout << setprecision(4);
    fout << "    \"unmet_count\": " << unmet_count << ",\n";
    fout << "    \"unmet_rate\": " << unmet_rate;

    // Steps (for RR algorithm)
    if (steps != nullptr && !steps->empty()) {
        fout << ",\n    \"steps\": [\n";
        for (size_t s = 0; s < steps->size(); s++) {
            const auto& step = (*steps)[s];
            fout << "      {";
            fout << "\"step\": " << (s + 1) << ", ";
            fout << setprecision(2);
            fout << "\"objective\": " << step.objective << ", ";
            fout << setprecision(3);
            fout << "\"time\": " << step.runtime << ", ";
            fout << "\"cpu_time\": " << step.cpu_time << ", ";
            fout << setprecision(6);
            fout << "\"gap\": " << step.gap << "}";
            if (s + 1 < steps->size()) fout << ",";
            fout << "\n";
        }
        fout << "    ]";
    }
    fout << "\n  },\n";

    // Problem section
    fout << "  \"problem\": {\n";
    fout << "    \"N\": " << values.number_of_items << ",\n";
    fout << "    \"T\": " << values.number_of_periods << ",\n";
    fout << "    \"F\": " << values.number_of_flows << ",\n";
    fout << "    \"G\": " << values.number_of_groups << ",\n";
    fout << "    \"capacity\": " << values.machine_capacity << "\n";
    fout << "  },\n";

    // Variables section
    fout << "  \"variables\": {\n";

    // X[i][t] - Production
    fout << "    \"X\": {\n";
    fout << "      \"description\": \"Production quantity\",\n";
    fout << "      \"dimensions\": [" << values.number_of_items << ", " << values.number_of_periods << "],\n";
    fout << "      \"data\": [\n";
    for (int i = 0; i < values.number_of_items; i++) {
        fout << "        [";
        for (int t = 0; t < values.number_of_periods; t++) {
            double val = cplex.getValue(X[i][t]);
            fout << setprecision(0) << val;
            if (t + 1 < values.number_of_periods) fout << ", ";
        }
        fout << "]";
        if (i + 1 < values.number_of_items) fout << ",";
        fout << "\n";
    }
    fout << "      ]\n";
    fout << "    },\n";

    // Y[g][t] - Setup
    fout << "    \"Y\": {\n";
    fout << "      \"description\": \"Setup decision\",\n";
    fout << "      \"dimensions\": [" << values.number_of_groups << ", " << values.number_of_periods << "],\n";
    fout << "      \"data\": [\n";
    for (int g = 0; g < values.number_of_groups; g++) {
        fout << "        [";
        for (int t = 0; t < values.number_of_periods; t++) {
            int val = (int)cplex.getValue(Y[g][t]);
            fout << val;
            if (t + 1 < values.number_of_periods) fout << ", ";
        }
        fout << "]";
        if (g + 1 < values.number_of_groups) fout << ",";
        fout << "\n";
    }
    fout << "      ]\n";
    fout << "    },\n";

    // L[g][t] - Carryover
    fout << "    \"L\": {\n";
    fout << "      \"description\": \"Setup carryover\",\n";
    fout << "      \"dimensions\": [" << values.number_of_groups << ", " << values.number_of_periods << "],\n";
    fout << "      \"data\": [\n";
    for (int g = 0; g < values.number_of_groups; g++) {
        fout << "        [";
        for (int t = 0; t < values.number_of_periods; t++) {
            int val = (int)cplex.getValue(L[g][t]);
            fout << val;
            if (t + 1 < values.number_of_periods) fout << ", ";
        }
        fout << "]";
        if (g + 1 < values.number_of_groups) fout << ",";
        fout << "\n";
    }
    fout << "      ]\n";
    fout << "    },\n";

    // I[f][t] - Inventory
    fout << "    \"I\": {\n";
    fout << "      \"description\": \"Inventory level\",\n";
    fout << "      \"dimensions\": [" << values.number_of_flows << ", " << values.number_of_periods << "],\n";
    fout << "      \"data\": [\n";
    for (int f = 0; f < values.number_of_flows; f++) {
        fout << "        [";
        for (int t = 0; t < values.number_of_periods; t++) {
            double val = cplex.getValue(I[f][t]);
            fout << setprecision(0) << val;
            if (t + 1 < values.number_of_periods) fout << ", ";
        }
        fout << "]";
        if (f + 1 < values.number_of_flows) fout << ",";
        fout << "\n";
    }
    fout << "      ]\n";
    fout << "    },\n";

    // B[i][t] - Backorder
    fout << "    \"B\": {\n";
    fout << "      \"description\": \"Backorder quantity\",\n";
    fout << "      \"dimensions\": [" << values.number_of_items << ", " << values.number_of_periods << "],\n";
    fout << "      \"data\": [\n";
    for (int i = 0; i < values.number_of_items; i++) {
        fout << "        [";
        for (int t = 0; t < values.number_of_periods; t++) {
            double val = cplex.getValue(B[i][t]);
            fout << setprecision(0) << val;
            if (t + 1 < values.number_of_periods) fout << ", ";
        }
        fout << "]";
        if (i + 1 < values.number_of_items) fout << ",";
        fout << "\n";
    }
    fout << "      ]\n";
    fout << "    },\n";

    // U[i] - Unmet demand
    fout << "    \"U\": {\n";
    fout << "      \"description\": \"Unmet demand indicator\",\n";
    fout << "      \"dimensions\": [" << values.number_of_items << "],\n";
    fout << "      \"data\": [";
    for (int i = 0; i < values.number_of_items; i++) {
        int val = (int)cplex.getValue(U[i]);
        fout << val;
        if (i + 1 < values.number_of_items) fout << ", ";
    }
    fout << "]\n";
    fout << "    }\n";

    fout << "  }\n";
    fout << "}\n";

    fout.close();
    cout << "[Output] Solution exported successfully\n";
}

// Legacy CSV output - kept for backward compatibility
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
    // Redirect to JSON output
    string json_path = filename;
    size_t dot_pos = json_path.find_last_of('.');
    if (dot_pos != string::npos) {
        json_path = json_path.substr(0, dot_pos);
    }
    json_path += "_" + GetCurrentTimestamp() + ".json";

    OutputSolutionJSON(json_path, "CPLEX", filename, values, lists,
                       cplex, X, Y, L, I, B, U, nullptr);
}
