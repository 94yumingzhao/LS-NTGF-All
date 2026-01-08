// input.cpp - 数据输入处理模块
// 从 CSV 文件读取生产计划优化问题的输入数据

#include "optimizer.h"
#include "common.h"

// 按分隔符分割字符串
void SplitString(const string& input, vector<string>& output, const string& delimiter) {
  output.clear();
  size_t start = 0;
  size_t end = input.find(delimiter);

  while (end != string::npos) {
    string token = input.substr(start, end - start);
    output.push_back(token);
    start = end + delimiter.length();
    end = input.find(delimiter, start);
  }

  if (start < input.length()) {
    output.push_back(input.substr(start));
  }
}

// 解析 CSV 行中的数值数据（跳过第一列标签）
void ParseCommaSeparatedValues(const string& line, vector<double>& values, int data_type) {
  vector<string> tokens;
  SplitString(line, tokens, ",");
  values.clear();

  for (size_t i = 1; i < tokens.size(); i++) {
    if (!tokens[i].empty()) {
      try {
        if (data_type == 0) {
          values.push_back(static_cast<double>(stoi(tokens[i])));
        } else {
          values.push_back(stod(tokens[i]));
        }
      } catch (const invalid_argument& e) {
        cout << "[警告] 无效数字: '" << tokens[i] << "'\n";
        values.push_back(0.0);
      } catch (const out_of_range& e) {
        cout << "[警告] 数字超范围: '" << tokens[i] << "'\n";
        values.push_back(0.0);
      }
    }
  }
}

// 从 CSV 文件读取生产计划数据
void ReadData(AllValues& values, AllLists& lists, const string& path) {
  ifstream inFile(path, ios::in);
  vector<string> data_in_line;
  string one_line;

  if (!inFile) {
    cout << "[错误] 无法打开文件: " << path << "\n";
    values = AllValues();
    lists = AllLists();
    return;
  }

  cout << "[读取] 文件: " << path << "\n";
  try {
    values = AllValues();
    lists = AllLists();

    // 读取基本参数
    getline(inFile, one_line);  // 案例编号（跳过）

    getline(inFile, one_line);
    SplitString(one_line, data_in_line, ",");
    values.number_of_periods = stoi(data_in_line[1]);

    getline(inFile, one_line);
    SplitString(one_line, data_in_line, ",");
    values.number_of_flows = stoi(data_in_line[1]);

    getline(inFile, one_line);
    SplitString(one_line, data_in_line, ",");
    values.number_of_groups = stoi(data_in_line[1]);

    // 读取成本参数
    getline(inFile, one_line);
    vector<double> cost_y_values;
    ParseCommaSeparatedValues(one_line, cost_y_values, 0);
    for (double cost : cost_y_values) {
      lists.cost_y.push_back(static_cast<int>(cost));
    }

    getline(inFile, one_line);
    vector<double> cost_i_values;
    ParseCommaSeparatedValues(one_line, cost_i_values, 1);
    for (double cost : cost_i_values) {
      lists.cost_i.push_back(cost);
    }

    getline(inFile, one_line);
    vector<double> usage_y_values;
    ParseCommaSeparatedValues(one_line, usage_y_values, 0);
    for (double usage : usage_y_values) {
      lists.usage_y.push_back(static_cast<int>(usage));
    }

    // 读取订单数
    getline(inFile, one_line);
    SplitString(one_line, data_in_line, ",");
    values.number_of_items = stoi(data_in_line[1]);
    values.original_number_of_items = values.number_of_items;
    values.machine_capacity = 1440;

    // 读取各流向的周期需求
    for (int f = 0; f < values.number_of_flows; f++) {
      getline(inFile, one_line);
      vector<double> demand_values;
      ParseCommaSeparatedValues(one_line, demand_values, 0);
      vector<int> temp_demand;
      for (double demand : demand_values) {
        temp_demand.push_back(static_cast<int>(demand));
      }
      lists.period_demand.push_back(temp_demand);
    }

    // 初始化订单标记矩阵
    lists.flow_flag.resize(values.number_of_items, vector<int>(values.number_of_flows, 0));
    lists.group_flag.resize(values.number_of_items, vector<int>(values.number_of_groups, 0));

    // 读取订单详细信息
    for (int i = 0; i < values.number_of_items; ) {
      if (!getline(inFile, one_line)) {
        cout << "[警告] 文件结束，已读取 " << i << "/" << values.number_of_items << " 订单\n";
        break;
      }

      if (one_line.empty() || one_line.find("order_") != 0) {
        continue;
      }

      SplitString(one_line, data_in_line, ",");

      if (data_in_line.size() < 9) {
        cout << "[错误] 订单行格式无效: " << one_line << "\n";
        continue;
      }

      try {
        int order_f = stoi(data_in_line[3]) - 1;
        int order_g = stoi(data_in_line[2]) - 1;
        int final_demand = static_cast<int>(stod(data_in_line[4]));
        int ew = stoi(data_in_line[5]);
        int lw = stoi(data_in_line[6]);
        int usage_x = stoi(data_in_line[7]);
        double cost_x = stod(data_in_line[8]);

        if (order_f < 0 || order_f >= values.number_of_flows) {
          cout << "[警告] 订单 " << i << " 流向无效: " << (order_f + 1) << "\n";
          continue;
        }
        if (order_g < 0 || order_g >= values.number_of_groups) {
          cout << "[警告] 订单 " << i << " 分组无效: " << (order_g + 1) << "\n";
          continue;
        }
        if (ew < 0 || lw >= values.number_of_periods || ew > lw) {
          cout << "[警告] 订单 " << i << " 时间窗无效: [" << ew << "," << lw << "]\n";
          continue;
        }

        lists.flow_flag[i][order_f] = 1;
        lists.group_flag[i][order_g] = 1;
        lists.final_demand.push_back(final_demand);
        lists.ew_x.push_back(ew);
        lists.lw_x.push_back(lw);
        lists.usage_x.push_back(usage_x);
        lists.cost_x.push_back(cost_x);
        i++;

      } catch (const exception& e) {
        cout << "[错误] 解析失败: " << e.what() << "\n";
        continue;
      }
    }

    // 初始化订单特定惩罚系数 (使用全局默认值)
    lists.cost_b.resize(values.number_of_items, values.b_penalty);
    lists.cost_u.resize(values.number_of_items, values.u_penalty);

    cout << "[读取] 完成，共 " << values.number_of_items << " 订单\n";

  } catch (const exception& e) {
    cout << "[错误] 读取失败: " << e.what() << "\n";
    values = AllValues();
    lists = AllLists();
    return;
  }
}
