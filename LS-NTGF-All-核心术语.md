# LS-NTGF-All 关键词与关键概念中英文对照

> 多产品批量计划优化求解器的术语、符号与概念索引

---

## 1. 问题领域术语

### 1.1 核心概念

| 中文 | 英文 | 缩写 | 定义 |
|:-----|:-----|:----:|:-----|
| 批量计划问题 | Lot Sizing Problem | LSP | 确定每个周期生产各产品的数量和时机 |
| 产品大类分组 | Product Family Grouping | - | 工艺相似的产品归为一组，共享启动状态 |
| 启动跨期 | Setup Carryover | - | 相邻周期间保持启动状态，无需重新启动 |
| 下游流向 | Downstream Flow | - | 产品完成后的下游加工路径 |
| 时间分解 | Time Decomposition | - | 按时间维度分解问题求解 |
| 阶段分解 | Stage Decomposition | - | 按决策阶段分解问题求解 |

### 1.2 应用场景

| 场景 | 英文 | 核心特征 |
|:-----|:-----|:---------|
| 钢铁连铸生产 | Steel Continuous Casting | 结晶器配置，钢种分组 |
| 化工批次生产 | Chemical Batch Production | 反应釜清洗，下游工序分流 |
| 电子元器件SMT | Electronics SMT Production | 吸嘴更换，测试工站分流 |

### 1.3 生产术语

| 中文 | 英文 | 定义 |
|:-----|:-----|:-----|
| 订单 | Order / Item | 需要生产的产品需求 |
| 周期 | Period | 计划时间轴的基本单位（班次、天、周） |
| 产品大类 | Product Family / Group | 工艺相似的产品集合，共享启动状态 |
| 流向 | Flow / Downstream | 产品完成后的下游加工路径 |
| 启动 | Setup | 为生产某产品大类进行的准备工作 |
| 跨期 | Carryover | 相邻周期间保持启动状态，无需重新启动 |
| 欠交 | Backorder | 订单未能在交期前完成，延迟交付 |
| 未满足 | Unmet | 订单完全无法在计划期内满足 |
| 产能 | Capacity | 周期内的可用工时或生产能力 |
| 在制品库存 | Work-in-Process Inventory | 已生产但未流向下游的产品库存 |

---

## 2. 数学模型术语

### 2.1 模型类型

| 中文 | 英文 | 缩写 | 定义 |
|:-----|:-----|:----:|:-----|
| 混合整数线性规划 | Mixed Integer Linear Programming | MILP | 包含整数变量和连续变量的线性规划 |
| 容量约束批量计划 | Capacitated Lot Sizing | CLS | 考虑产能限制的批量计划 |
| 启动跨期批量计划 | Lot Sizing with Setup Carryover | CLSP-SC | 包含启动跨期约束的批量计划 |
| 大M约束 | Big-M Constraint | - | 使用大常数M建立逻辑关系的约束 |

### 2.2 符号定义

#### 集合与索引

| 符号 | 英文 | 中文 | 定义 |
|:----:|:-----|:-----|:-----|
| $\mathcal{I}$ | Item Set | 订单集合 | $\{1, 2, \ldots, N\}$ |
| $\mathcal{T}$ | Period Set | 周期集合 | $\{1, 2, \ldots, T\}$ |
| $\mathcal{G}$ | Family Set | 产品大类集合 | $\{1, 2, \ldots, G\}$ |
| $\mathcal{F}$ | Flow Set | 流向集合 | $\{1, 2, \ldots, F\}$ |
| $i$ | Item Index | 订单索引 | $i \in \mathcal{I}$ |
| $t$ | Period Index | 周期索引 | $t \in \mathcal{T}$ |
| $g$ | Family Index | 产品大类索引 | $g \in \mathcal{G}$ |
| $f$ | Flow Index | 流向索引 | $f \in \mathcal{F}$ |

#### 需求参数

| 符号 | 英文 | 中文 | 定义 |
|:----:|:-----|:-----|:-----|
| $d_i$ | Demand | 需求量 | 订单i的总需求量 |
| $e_i$ | Earliest Period | 最早生产期 | 订单i可以开始生产的最早周期 |
| $l_i$ | Latest Period | 最晚交付期 | 订单i必须交付的最晚周期 |

#### 关联参数

| 符号 | 英文 | 中文 | 定义 |
|:----:|:-----|:-----|:-----|
| $h_{ig}$ | Item-Family Incidence | 订单-族归属 | 订单i属于族g则为1 |
| $k_{if}$ | Item-Flow Incidence | 订单-流向归属 | 订单i流向f则为1 |

#### 成本参数

| 符号 | 英文 | 中文 | 典型值 |
|:----:|:-----|:-----|:------:|
| $c_i^x$ | Production Cost | 单位生产成本 | - |
| $c_g^y$ | Setup Cost | 启动成本 | - |
| $c_f^I$ | Holding Cost | 单位库存成本 | - |
| $c^b$ | Backorder Penalty | 欠交惩罚 | 100 |
| $c^u$ | Unmet Penalty | 未满足惩罚 | 10000 |

#### 产能参数

| 符号 | 英文 | 中文 | 定义 |
|:----:|:-----|:-----|:-----|
| $s_i^x$ | Production Capacity Usage | 单位产能消耗 | 生产单位订单i消耗的产能 |
| $s_g^y$ | Setup Capacity Usage | 启动产能消耗 | 族g启动消耗的产能 |
| $C_t$ | Period Capacity | 周期产能 | 周期t的总可用产能 |
| $D_{ft}$ | Downstream Capacity | 下游处理能力 | 流向f在周期t的处理能力 |

#### 决策变量

| 符号 | 英文 | 中文 | 类型 |
|:----:|:-----|:-----|:----:|
| $x_{it}$ | Production Quantity | 生产量 | 连续，$\geq 0$ |
| $y_{gt}$ | Setup Binary | 启动二进制 | 二元，$\in \{0,1\}$ |
| $\lambda_{gt}$ | Carryover Binary | 跨期二进制 | 二元，$\in \{0,1\}$ |
| $I_{ft}$ | Inventory Level | 库存水平 | 连续，$\geq 0$ |
| $P_{ft}$ | Processing Quantity | 下游处理量 | 连续，$\geq 0$ |
| $b_{it}$ | Backorder Quantity | 欠交量 | 连续，$\geq 0$ |
| $u_i$ | Unmet Binary | 未满足二进制 | 二元，$\in \{0,1\}$ |

### 2.3 约束类型

| 中文 | 英文 | 编号 | 数学表达 |
|:-----|:-----|:----:|:---------|
| 需求满足约束 | Demand Constraint | (1) | $\sum_t x_{it} + u_i d_i \geq d_i$ |
| 流平衡约束 | Flow Balance | (2) | $\sum_i k_{if} x_{it} + I_{f,t-1} = P_{ft} + I_{ft}$ |
| 下游处理能力 | Downstream Capacity | (3) | $P_{ft} \leq D_{ft}$ |
| 未满足指示 | Unmet Indicator | (4) | $d_i u_i \geq b_{i,T}$ |
| 总产能约束 | Total Capacity | (5) | $\sum_i s_i^x x_{it} + \sum_g s_g^y y_{gt} \leq C_t$ |
| 启动约束 | Setup Constraint | (6) | $\sum_{i:h_{ig}=1} s_i^x x_{it} \leq C_t(y_{gt} + \lambda_{gt})$ |
| 每期最多一跨期 | At-Most-One Carryover | (7) | $\sum_g \lambda_{gt} \leq 1$ |
| 跨期可行性 | Carryover Feasibility | (8) | $y_{g,t-1} + \lambda_{g,t-1} - \lambda_{gt} \geq 0$ |
| 跨期排他性 | Carryover Exclusivity | (9) | 防止跨期与其他族启动冲突 |
| 初始状态 | Initial State | (10) | $y_{g,1} = \lambda_{g,1} = 0$ |
| 时间窗约束 | Time Window | (11) | $x_{it} = 0$ 当 $t < e_i$ 或 $t > l_i$ |
| 欠交定义 | Backorder Definition | (12) | $d_i - \sum_{\tau=1}^t x_{i\tau} = b_{it}$ |

---

## 3. 算法术语

### 3.1 算法框架

| 中文 | 英文 | 缩写 | 定义 |
|:-----|:-----|:----:|:-----|
| 松弛-固定法 | Relax-and-Fix | RF | 滚动时间窗口，逐步固定决策的启发式 |
| 固定-优化法 | Fix-and-Optimize | FO | 固定部分变量，优化剩余变量的局部搜索 |
| 松弛-恢复法 | Relax-and-Recover | RR | 三阶段分解：定启动→定跨期→定生产 |
| 时间分解 | Time Decomposition | - | 按时间轴分解为子问题 |
| 阶段分解 | Stage Decomposition | - | 按决策阶段分解为子问题 |

### 3.2 RF算法 (Relax-and-Fix)

#### 核心概念

| 中文 | 英文 | 符号 | 定义 |
|:-----|:-----|:----:|:-----|
| 已固定区 | Fixed Region | $T^{fix}$ | 决策已确定的周期集合 |
| 当前窗口 | Current Window | $T^{win}$ | 当前求解的周期集合，保持整数约束 |
| 放松区 | Relaxed Region | $T^{rel}$ | 变量松弛为连续的周期集合 |
| 窗口长度 | Window Size | $W$ | 当前求解窗口的周期数 |
| 固定步长 | Fix Step | $S$ | 每次迭代固定的周期数 |
| 子问题 | Subproblem | $SP(k, W)$ | 起始周期k，窗口大小W的子问题 |

#### RF参数

| 参数 | 符号 | 默认值 | 含义 |
|:-----|:----:|:------:|:-----|
| 窗口长度 | $W$ | 6 | 当前求解窗口的周期数 |
| 固定步长 | $S$ | 1 | 每次迭代固定的周期数 |
| 最大重试 | $R$ | 3 | 窗口扩展的最大重试次数 |
| 子问题时限 | - | 60秒 | 每个子问题的CPLEX时间限制 |

#### 变量类型

| 区域 | 二元变量 $y, \lambda$ | 连续变量 $x, I, P, b$ |
|:-----|:---------------------|:---------------------|
| $T^{fix}$ | 固定为已求解值 | 重新优化 |
| $T^{win}$ | 二元（整数约束） | 优化 |
| $T^{rel}$ | 连续松弛 $\in [0,1]$ | 优化 |

### 3.3 RFO算法 (RF + Fix-and-Optimize)

#### 核心概念

| 中文 | 英文 | 定义 |
|:-----|:-----|:-----|
| 构造阶段 | Construction Phase | RF构造初始可行解 |
| 改进阶段 | Improvement Phase | FO局部搜索优化 |
| 邻域子问题 | Neighborhood Subproblem | FO的局部优化子问题 |
| 优化窗口 | Optimization Window | FO优化的周期窗口 |
| 边界缓冲 | Border Buffer | 窗口边界的缓冲周期 |
| 多轮优化 | Multi-Pass Optimization | FO的迭代轮数 |

#### RFO参数

| 参数 | 符号 | 默认值 | 含义 |
|:-----|:----:|:------:|:-----|
| FO窗口长度 | $W_o$ | 8 | FO优化窗口大小 |
| FO步长 | $S_o$ | 3 | FO窗口滑动步长 |
| 最大轮数 | $H$ | 2 | FO优化的最大轮数 |
| 边界缓冲 | $\Delta$ | 1 | 窗口边界的缓冲周期数 |
| 子问题时限 | - | 30秒 | FO子问题时间限制 |

#### 邻域定义

| 符号 | 英文 | 定义 |
|:----:|:-----|:-----|
| $WND^+(a)$ | Extended Window | $[\max(1, a-\Delta), \min(T, a+W_o+\Delta))$ |
| $NSP(a)$ | Neighborhood Subproblem | 以周期a为起点的邻域子问题 |

### 3.4 RR算法 (Relax-and-Recover)

#### 三阶段

| 阶段 | 英文 | 目标 | 决策变量 |
|:-----|:-----|:-----|:---------|
| Stage 1 | Setup Structure | 确定启动结构 | 固定$\lambda=0$，求解$y^*$ |
| Stage 2 | Carryover Maximization | 最大化跨期 | 固定$y=y^*$，求解$\lambda^*$ |
| Stage 3 | Final Production Plan | 求解生产计划 | 固定$y^*, \lambda^*$，求解$x^*$ |

#### Stage 1 (求解启动结构)

| 中文 | 英文 | 定义 |
|:-----|:-----|:-----|
| 无跨期模型 | No-Carryover Model | 移除所有$\lambda$变量，简化为标准LSP |
| 产能放大 | Capacity Amplification | 可选放大产能系数，默认1.0 |
| 启动决策 | Setup Decision | 输出$y_{gt}^*$ |

#### Stage 2 (求解跨期变量)

| 中文 | 英文 | 数学表达 |
|:-----|:-----|:---------|
| 跨期最大化 | Carryover Maximization | $\max \sum_{g,t} \lambda_{gt}$ |
| 固定启动 | Fixed Setup | $y_{gt} = y_{gt}^*$ |
| 跨期连续性 | Carryover Continuity | $2\lambda_{gt} \leq y_{g,t-1}^* + y_{gt}^*$ |
| 跨期互斥 | Carryover Exclusivity | $\sum_g \lambda_{gt} \leq 1$ |

#### Stage 3 (最终求解)

| 中文 | 英文 | 定义 |
|:-----|:-----|:-----|
| 跨期替代 | Carryover Substitution | 若$\lambda_{gt}^*=1$，则$y_{gt}=0$ |
| 固定结构 | Fixed Structure | $y, \lambda$已固定，求解连续变量 |
| 完整生产计划 | Complete Production Plan | 输出$(x^*, I^*, P^*, b^*, u^*)$ |

---

## 4. 数值容差

| 符号 | 值 | 英文 | 中文 | 用途 |
|:----:|:--:|:-----|:-----|:-----|
| $\varepsilon$ | $10^{-6}$ | Epsilon | 浮点比较容差 | 判断浮点数相等 |
| $\varepsilon_{int}$ | $10^{-5}$ | Integer Tolerance | 整数容差 | 判断变量为整数 |
| $\varepsilon_{impr}$ | $10^{-3}$ | Improvement Tolerance | 改进容差 | FO判断是否改进 |

---

## 5. 问题特征术语

### 5.1 问题分类

| 中文 | 英文 | 说明 |
|:-----|:-----|:-----|
| 单层批量计划 | Single-Level Lot Sizing | 无物料清单，每个订单独立 |
| 多层批量计划 | Multi-Level Lot Sizing | 有物料清单，订单间有依赖 |
| 小批量 | Small Lot Size | 批量趋近需求，频繁启动 |
| 大批量 | Large Lot Size | 批量远超需求，集中生产 |
| 硬时间窗 | Hard Time Window | 必须在时间窗内生产 |
| 软时间窗 | Soft Time Window | 可违反时间窗，但有惩罚 |

### 5.2 复杂度特征

| 特征 | 英文 | 影响 |
|:-----|:-----|:-----|
| NP-Hard | NP-Hard | 无多项式时间精确算法 |
| 指数级状态空间 | Exponential State Space | 决策组合数随规模指数增长 |
| 弱LP松弛 | Weak LP Relaxation | Big-M约束导致LP松弛边界差 |
| 时间耦合 | Temporal Coupling | 跨期约束形成时间链式依赖 |
| 组合耦合 | Combinatorial Coupling | 产品大类分组形成组合依赖 |

### 5.3 求解难点

| 中文 | 英文 | 说明 |
|:-----|:-----|:-----|
| 变量规模 | Variable Scale | $O(NT + GT + FT)$ 个决策变量 |
| 约束复杂性 | Constraint Complexity | 跨期约束的逻辑关系复杂 |
| 对称性 | Symmetry | 相同族的订单可交换，增加搜索空间 |
| Big-M选择 | Big-M Selection | M过大导致松弛差，过小导致不可行 |
| 多目标权衡 | Multi-Objective Trade-off | 启动、库存、欠交成本的平衡 |

---

## 6. 算法比较与选择

### 6.1 算法特点对比

| 维度 | RF | RFO | RR |
|:-----|:--:|:---:|:--:|
| 求解速度 | 快 | 中 | 慢 |
| 解质量 | 中 | 好 | 优 |
| 鲁棒性 | 好 | 好 | 中 |
| 实现复杂度 | 简单 | 中等 | 复杂 |
| 适用规模 | 大 | 大 | 中 |

### 6.2 算法选择建议

| 场景 | 推荐算法 | 理由 |
|:-----|:---------|:-----|
| 大规模实例（T>50） | RF | 速度优先 |
| 中等规模（20<T<50） | RFO | 平衡速度和质量 |
| 小规模实例（T<20） | RR | 质量优先 |
| 时间限制紧 | RF | 快速得到可行解 |
| 质量要求高 | RFO/RR | 多轮优化 |
| 跨期机会多 | RFO | FO能充分利用跨期 |
| 启动成本高 | RR | Stage2最大化跨期 |

---

## 7. 启动跨期机制

### 7.1 核心概念

| 中文 | 英文 | 定义 |
|:-----|:-----|:-----|
| 启动状态 | Setup State | 产品大类的准备就绪状态 |
| 跨期状态 | Carryover State | 保持上期启动状态到当期 |
| 状态转换 | State Transition | $y_{gt}$和$\lambda_{gt}$的逻辑关系 |
| 跨期替代 | Carryover Substitution | 有跨期时无需启动 |
| 跨期收益 | Carryover Benefit | 节省启动成本 |

### 7.2 状态转换规则

| 前期状态 | 当期动作 | 结果 | 成本 |
|:---------|:---------|:-----|:-----|
| $y_{g,t-1}=0, \lambda_{g,t-1}=0$ | 要生产 | $y_{gt}=1$ | 支付启动成本 |
| $y_{g,t-1}=1$ | 要生产 | $\lambda_{gt}=1$ 或 $y_{gt}=1$ | 跨期免费或重新启动 |
| $\lambda_{g,t-1}=1$ | 要生产 | $\lambda_{gt}=1$ 或 $y_{gt}=1$ | 跨期免费或重新启动 |
| 任意 | 不生产 | $y_{gt}=0, \lambda_{gt}=0$ | 无成本 |

### 7.3 跨期约束

| 约束类型 | 数学表达 | 含义 |
|:---------|:---------|:-----|
| 连续性 | $y_{g,t-1} + \lambda_{g,t-1} - \lambda_{gt} \geq 0$ | 当期跨期需前期有状态 |
| 互斥性 | $\sum_g \lambda_{gt} \leq 1$ | 每期最多一个跨期 |
| 排他性 | $\lambda_{gt} + \lambda_{g,t-1} + y_{gt} - \sum_{g'\neq g} y_{g't} \leq 2$ | 跨期与其他族启动冲突 |

---

## 附录：缩写索引

| 缩写 | 英文全称 | 中文 |
|:----:|:---------|:-----|
| LSP | Lot Sizing Problem | 批量计划问题 |
| CLSP | Capacitated Lot Sizing Problem | 容量约束批量计划 |
| CLSP-SC | CLSP with Setup Carryover | 启动跨期批量计划 |
| MILP | Mixed Integer Linear Programming | 混合整数线性规划 |
| LP | Linear Programming | 线性规划 |
| MIP | Mixed Integer Programming | 混合整数规划 |
| RF | Relax-and-Fix | 松弛-固定法 |
| FO | Fix-and-Optimize | 固定-优化法 |
| RFO | Relax-and-Fix + Fix-and-Optimize | RF+FO算法 |
| RR | Relax-and-Recover | 松弛-恢复法 |
| WIP | Work-in-Process | 在制品 |

---

**文档版本**: 1.0
**生成日期**: 2026-01-14
**对应程序版本**: LS-NTGF-All (RF/RFO/RR三算法)
