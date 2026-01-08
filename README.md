# LS-NTGF-All

**批量计划优化求解器 - 支持产品族分组与启动跨期**

多产品批量计划问题(Lot Sizing)的统一求解系统，支持启动跨期、产品族分组和下游流向约束。

---

## 项目概述

LS-NTGF-All 是一个统一的批量计划求解器，实现了三种分解算法：

| 算法 | 全称 | 特点 | 适用场景 |
|------|------|------|----------|
| **RF** | Relax-and-Fix | 时间窗口滚动固定 | 快速获取可行解 |
| **RFO** | RF + Fix-and-Optimize | RF + 局部搜索优化 | 追求最优解质量 |
| **RR** | Relax-and-Recover | 三阶段分解 | 最大化跨期节省 |

## 问题特征

- **产品族分组**: 产品按工艺相似性分组，同族产品共享启动状态
- **启动跨期**: 相邻周期可保持启动状态，避免重复启动成本
- **下游流向约束**: 产品有不同的下游加工流向，受下游产能限制
- **时间窗约束**: 每个订单有最早生产期和最晚交付期
- **欠交与未满足惩罚**: 允许延迟交付和完全不满足，但需支付惩罚成本

## 项目结构

```
LS-NTGF-All/
├── src/
│   ├── main.cpp              # 主程序入口，命令行解析
│   ├── optimizer.h           # 核心数据结构和函数声明
│   ├── common.h              # 工具函数和类型定义
│   ├── input.cpp             # CSV数据文件读取
│   ├── output.cpp            # JSON/CSV结果输出
│   ├── cplex_lot_sizing.cpp  # CPLEX完整模型直接求解
│   ├── big_order.cpp         # 订单合并(流向-分组策略)
│   ├── case_analysis.cpp     # 批量算例分析工具
│   ├── logger.h/cpp          # 日志系统(双向输出流)
│   ├── tee_stream.h          # CPLEX日志双向输出流
│   └── solvers/
│       ├── rf_solver.cpp     # RF算法实现
│       ├── rfo_solver.cpp    # RFO算法实现
│       └── rr_solver.cpp     # RR算法实现
├── docs/
│   └── LS-NTGF-All_数学模型与算法分析_*.md  # 数学模型文档
├── CMakeLists.txt            # CMake构建配置
├── CMakePresets.json         # VS2022构建预设
└── README.md                 # 本文件
```

## 构建环境

### 依赖项

- **IBM CPLEX Optimization Studio** (测试版本 22.1.1)
  - 默认路径: `D:/CPLEX` (可在CMake中配置)
- **CMake** >= 3.15
- **MSVC** (Visual Studio 2022)

### 构建步骤

```powershell
# 使用VS2022预设配置
cmake --preset vs2022-release

# 构建
cmake --build --preset vs2022-release

# 可执行文件位置
# build/vs2022/bin/Release/LS-NTGF-All.exe
```

## 使用方法

### 命令行参数

```
LS-NTGF-All.exe [选项] [数据文件]

算法选择:
  --algo=RF           Relax-and-Fix (默认)
  --algo=RFO          RF + Fix-and-Optimize
  --algo=RR           Relax-and-Recover 三阶段分解

选项:
  -f, --file <路径>       输入数据文件
  -o, --output <目录>     输出目录 (默认: ./results)
  -l, --log <文件>        日志文件路径 (默认: ./logs/solve.log)
  -t, --time <秒>         CPLEX时间限制 (默认: 30)
  --u-penalty <整数>      未满足惩罚 (默认: 10000)
  --b-penalty <整数>      欠交惩罚 (默认: 100)
  --threshold <小数>      大订单阈值 (默认: 1000)
  --no-merge              禁用订单合并
  --cplex-workdir <路径>  CPLEX工作目录 (默认: D:\CPLEX_Temp)
  --cplex-workmem <MB>    CPLEX内存限制 (默认: 4096)
  --cplex-threads <数量>  CPLEX线程数, 0=自动 (默认: 0)
  -h, --help              显示帮助信息
```

### 使用示例

```powershell
# 使用RF算法求解
LS-NTGF-All.exe --algo=RF data.csv

# 使用RFO算法，时间限制60秒
LS-NTGF-All.exe --algo=RFO -t 60 data.csv

# 使用RR算法，指定输出目录
LS-NTGF-All.exe --algo=RR --output=./out data.csv
```

## 输入数据格式

CSV文件结构：

```csv
case_id,<算例名称>
T,<周期数>
F,<流向数>
G,<分组数>
cost_y,<启动成本_g1>,<启动成本_g2>,...
cost_i,<库存成本_f1>,<库存成本_f2>,...
usage_y,<启动产能_g1>,<启动产能_g2>,...
N,<订单数>
demand_f1,<需求_f1_t1>,<需求_f1_t2>,...
demand_f2,<需求_f2_t1>,<需求_f2_t2>,...
...
order_1,<编号>,<分组>,<流向>,<需求量>,<最早期>,<最晚期>,<产能消耗>,<生产成本>
order_2,...
...
```

## 输出格式

JSON结果文件包含：

```json
{
  "summary": {
    "algorithm": "RF|RFO|RR",
    "input_file": "...",
    "objective": 579709.00,
    "total_time": 12.345,
    "solve_time": 10.234,
    "gap": 0.001234
  },
  "problem": {
    "N": 100, "T": 30, "F": 5, "G": 5, "capacity": 1440
  },
  "metrics": {
    "cost": { "production": ..., "setup": ..., "inventory": ..., "backorder": ..., "unmet": ... },
    "setup_carryover": { "total_setups": ..., "total_carryovers": ..., "saved_setup_cost": ... },
    "demand": { "total_demand": ..., "unmet_count": ..., "unmet_rate": ..., "on_time_rate": ... },
    "capacity": { "avg_utilization": ..., "max_utilization": ..., "by_period": [...] }
  },
  "variables": {
    "Y": { "dimensions": [G, T], "data": [[...], ...] },
    "L": { "dimensions": [G, T], "data": [[...], ...] },
    "X": { "dimensions": [N, T], "data": [[...], ...] },
    "I": { "dimensions": [F, T], "data": [[...], ...] },
    "B": { "dimensions": [N, T], "data": [[...], ...] },
    "U": { "dimensions": [N], "data": [...] }
  }
}
```

## 算法详解

### RF (Relax-and-Fix)

基于时间窗口的分解算法：
1. 将时间轴划分为：已固定区、当前窗口、放松区
2. 当前窗口内保持整数约束，放松区松弛为连续变量
3. 求解子问题后固定窗口解，窗口向前滑动
4. 重复直到所有周期都被固定

**参数**: 窗口大小=6, 固定步长=1, 最大重试=3

### RFO (RF + Fix-and-Optimize)

两阶段方法：
1. **阶段1 (RF)**: 构造初始可行解
2. **阶段2 (FO)**: 滑动窗口局部搜索优化解质量

**FO参数**: 窗口大小=8, 步长=3, 最大轮数=2

### RR (Relax-and-Recover)

三阶段分解：
1. **阶段1**: 固定 $\lambda=0$，求解启动结构 $y^*$
2. **阶段2**: 固定 $y^*$，最大化跨期变量 $\lambda^*$
3. **阶段3**: 固定 $y^*, \lambda^*$，求解最终生产计划

## 数学模型

详细数学模型请参见 `docs/LS-NTGF-All_数学模型与算法分析_*.md`

### 决策变量

| 变量 | 域 | 含义 |
|------|-----|------|
| $x_{it}$ | $\geq 0$ | 订单 $i$ 在周期 $t$ 的生产量 |
| $y_{gt}$ | $\{0,1\}$ | 产品族 $g$ 在周期 $t$ 是否启动 |
| $\lambda_{gt}$ | $\{0,1\}$ | 产品族 $g$ 在周期 $t$ 是否有跨期 |
| $I_{ft}$ | $\geq 0$ | 流向 $f$ 在周期 $t$ 末的在制品库存 |
| $b_{it}$ | $\geq 0$ | 订单 $i$ 在周期 $t$ 的欠交量 |
| $u_i$ | $\{0,1\}$ | 订单 $i$ 是否完全未满足 |

### 目标函数

最小化：生产成本 + 启动成本 + 库存成本 + 欠交惩罚 + 未满足惩罚

$$\min \sum_i \sum_t c^x_i x_{it} + \sum_g \sum_t c^y_g y_{gt} + \sum_f \sum_t c^I_f I_{ft} + \sum_i \sum_{t \geq l_i} c^b b_{it} + \sum_i c^u u_i$$

## 性能基准

典型规模 (N=100, T=30, G=5, F=5) 测试结果：

| 算法 | 目标值范围 | 求解时间 |
|------|-----------|---------|
| RF | 580K - 620K | 5 - 15 秒 |
| RFO | 570K - 600K | 15 - 60 秒 |
| RR | 575K - 610K | 10 - 30 秒 |

## GUI集成

本求解器设计用于与 **LS-NTGF-GUI** 配合使用，GUI提供：
- 可视化问题实例生成
- 算法选择和参数调优
- 实时求解进度监控
- 结果可视化（热力图、图表、变量浏览器）

供GUI解析的状态码：
- `[LOAD:OK:N:T:F:G]` - 数据加载成功
- `[MERGE:合并前:合并后]` - 订单合并完成
- `[STAGE:n:START]` / `[STAGE:n:DONE:目标值:时间:间隙]` - 阶段进度
- `[DONE:SUCCESS]` - 求解完成

## 相关文档

- [数学模型与算法分析](docs/LS-NTGF-All_数学模型与算法分析_20260108.md) - 完整的数学公式和算法描述

## 许可证

私有项目，使用前请联系作者。
