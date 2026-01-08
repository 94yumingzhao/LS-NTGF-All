# LS-NTGF-All 数学模型与算法分析

**文档版本**: 1.0
**创建日期**: 2026-01-08
**项目名称**: LS-NTGF-All (Lot Sizing with Non-Triangular Group Families)

---

## 目录

1. [问题描述](#1-问题描述)
2. [符号定义](#2-符号定义)
3. [完整数学模型](#3-完整数学模型)
4. [算法一: RF (Relax-and-Fix)](#4-算法一-rf-relax-and-fix)
5. [算法二: RFO (RF + Fix-and-Optimize)](#5-算法二-rfo-rf--fix-and-optimize)
6. [算法三: RR (Relax-and-Recover)](#6-算法三-rr-relax-and-recover)
7. [约束结构分析](#7-约束结构分析)
8. [算法比较与选择建议](#8-算法比较与选择建议)

---

## 1. 问题描述

### 1.1 业务背景

本项目求解的是一个**多产品批量计划问题 (Multi-Product Lot Sizing Problem)**，具有以下特征:

- **产品族分组 (Product Family Grouping)**: 产品按工艺相似性分为多个族(Group)，同一族的产品共享启动状态
- **启动跨期 (Setup Carryover)**: 相邻周期间可保持启动状态，避免重复启动成本
- **下游工序流向 (Downstream Flow)**: 产品有不同的下游加工流向，受下游产能约束
- **时间窗约束 (Time Window Constraint)**: 每个订单有最早生产期和最晚交付期
- **欠交与未满足惩罚 (Backorder & Unmet Penalty)**: 允许延迟交付(欠交)和完全不满足(未满足)，但需支付惩罚成本

### 1.2 问题分类

该问题属于:
- **混合整数线性规划 (MILP)**: 包含二元决策变量(启动、跨期)和连续变量(生产量、库存)
- **NP-Hard问题**: 批量计划问题的一般形式已被证明是NP-Hard的
- **大规模优化**: 实际规模可达数百订单、数十周期，直接求解困难

---

## 2. 符号定义

### 2.1 集合与索引

| 符号 | 含义 |
|------|------|
| $I = \{1, 2, \ldots, N\}$ | 订单(产品)集合 |
| $T = \{1, 2, \ldots, T\}$ | 计划周期集合 |
| $G = \{1, 2, \ldots, G\}$ | 产品族(分组)集合 |
| $F = \{1, 2, \ldots, F\}$ | 下游流向集合 |
| $i \in I$ | 订单索引 |
| $t \in T$ | 周期索引 |
| $g \in G$ | 产品族索引 |
| $f \in F$ | 流向索引 |

### 2.2 参数

| 符号 | 含义 |
|------|------|
| $d_i$ | 订单 $i$ 的需求量 |
| $e_i$ | 订单 $i$ 的最早生产期 (Earliest Window) |
| $l_i$ | 订单 $i$ 的最晚交付期 (Latest Window) |
| $c^x_i$ | 订单 $i$ 的单位生产成本 |
| $c^y_g$ | 产品族 $g$ 的启动成本 |
| $c^I_f$ | 流向 $f$ 的单位库存持有成本 |
| $c^b$ | 单位欠交惩罚 (默认 100) |
| $c^u$ | 未满足惩罚 (默认 10000) |
| $s^x_i$ | 订单 $i$ 的单位产能消耗 |
| $s^y_g$ | 产品族 $g$ 的启动产能消耗 |
| $C_t$ | 周期 $t$ 的总产能 (默认 1440) |
| $D_{ft}$ | 流向 $f$ 在周期 $t$ 的下游处理能力 |
| $h_{ig}$ | 订单-族归属矩阵: 订单 $i$ 属于族 $g$ 则为 1，否则为 0 |
| $k_{if}$ | 订单-流向归属矩阵: 订单 $i$ 流向 $f$ 则为 1，否则为 0 |

### 2.3 决策变量

| 符号 | 类型 | 含义 |
|------|------|------|
| $x_{it}$ | 连续 $\geq 0$ | 订单 $i$ 在周期 $t$ 的生产量 |
| $y_{gt}$ | 二元 $\{0,1\}$ | 产品族 $g$ 在周期 $t$ 是否启动 |
| $\lambda_{gt}$ | 二元 $\{0,1\}$ | 产品族 $g$ 在周期 $t$ 是否有启动跨期 |
| $I_{ft}$ | 连续 $\geq 0$ | 流向 $f$ 在周期 $t$ 末的在制品库存 |
| $P_{ft}$ | 连续 $\geq 0$ | 流向 $f$ 在周期 $t$ 的下游处理量 |
| $b_{it}$ | 连续 $\geq 0$ | 订单 $i$ 在周期 $t$ 的欠交量 |
| $u_i$ | 二元 $\{0,1\}$ | 订单 $i$ 是否完全未满足 |

---

## 3. 完整数学模型

### 3.1 目标函数

$$
\min Z = \underbrace{\sum_{i \in I} \sum_{t \in T} c^x_i \cdot x_{it}}_{\text{生产成本}} + \underbrace{\sum_{g \in G} \sum_{t \in T} c^y_g \cdot y_{gt}}_{\text{启动成本}} + \underbrace{\sum_{f \in F} \sum_{t \in T} c^I_f \cdot I_{ft}}_{\text{库存成本}} + \underbrace{\sum_{i \in I} \sum_{t \geq l_i} c^b \cdot b_{it}}_{\text{欠交惩罚}} + \underbrace{\sum_{i \in I} c^u \cdot u_i}_{\text{未满足惩罚}}
$$

### 3.2 约束条件

#### 约束(1): 需求满足约束

$$
\sum_{t \in T} x_{it} + u_i \cdot d_i \geq d_i, \quad \forall i \in I
$$

> 每个订单要么被完全生产，要么标记为未满足

#### 约束(2): 下游工序流平衡

$$
\sum_{i \in I} k_{if} \cdot x_{it} + I_{f,t-1} - P_{ft} - I_{ft} = 0, \quad \forall f \in F, \forall t \in T
$$

> 流向 $f$ 的当期产出 + 期初库存 = 下游处理量 + 期末库存

#### 约束(3): 下游处理能力

$$
P_{ft} \leq D_{ft}, \quad \forall f \in F, \forall t \in T
$$

#### 约束(4): 终期未满足指示

$$
d_i \cdot u_i \geq b_{i,T}, \quad \forall i \in I
$$

> 若终期仍有欠交，则标记为未满足

#### 约束(5): 总产能约束

$$
\sum_{i \in I} s^x_i \cdot x_{it} + \sum_{g \in G} s^y_g \cdot y_{gt} \leq C_t, \quad \forall t \in T
$$

#### 约束(6): 产品族启动约束 (Big-M)

$$
\sum_{i: h_{ig}=1} s^x_i \cdot x_{it} \leq C_t \cdot (y_{gt} + \lambda_{gt}), \quad \forall g \in G, \forall t \in T
$$

> 族 $g$ 在周期 $t$ 要生产，必须有启动或跨期

#### 约束(7): 每期最多一个跨期

$$
\sum_{g \in G} \lambda_{gt} \leq 1, \quad \forall t \in T
$$

#### 约束(8): 跨期可行性

$$
y_{g,t-1} + \lambda_{g,t-1} - \lambda_{gt} \geq 0, \quad \forall g \in G, \forall t \geq 2
$$

> 周期 $t$ 要有跨期，周期 $t-1$ 必须有启动或跨期

#### 约束(9): 跨期排他性

$$
\lambda_{gt} + \lambda_{g,t-1} + y_{gt} - \sum_{g' \neq g} y_{g't} \leq 2, \quad \forall g \in G, \forall t \geq 2
$$

> 防止跨期与其他族的启动冲突

#### 约束(10): 初始状态

$$
y_{g,1} = 0, \quad \lambda_{g,1} = 0, \quad \forall g \in G
$$

> 第一周期没有上期遗留的启动状态

#### 约束(11): 时间窗约束

$$
x_{it} = 0, \quad \forall i \in I, \forall t < e_i \text{ 或 } t > l_i
$$

#### 约束(12): 欠交定义

$$
d_i - \sum_{\tau=1}^{t} x_{i\tau} = b_{it}, \quad \forall i \in I, \forall t \geq l_i
$$

### 3.3 变量域

$$
\begin{aligned}
x_{it} &\geq 0, &\forall i, t \\
y_{gt} &\in \{0, 1\}, &\forall g, t \\
\lambda_{gt} &\in \{0, 1\}, &\forall g, t \\
I_{ft} &\geq 0, &\forall f, t \\
P_{ft} &\geq 0, &\forall f, t \\
b_{it} &\geq 0, &\forall i, t \\
u_i &\in \{0, 1\}, &\forall i
\end{aligned}
$$

---

## 4. 算法一: RF (Relax-and-Fix)

### 4.1 算法思想

**Relax-and-Fix (RF)** 是一种基于时间分解的启发式算法，核心思想是:

1. 将时间轴划分为三个动态区域: **已固定区 $T^{\text{fix}}$**、**当前窗口 $T^{\text{win}}$**、**放松区 $T^{\text{rel}}$**
2. 在当前窗口内保持整数约束，放松区内放松为连续变量
3. 求解子问题后，固定窗口内的解，滑动窗口向前推进
4. 迭代直到所有周期都被固定

### 4.2 算法参数

| 参数 | 符号 | 默认值 | 含义 |
|------|------|--------|------|
| 窗口长度 | $W$ | 6 | 当前求解窗口的周期数 |
| 固定步长 | $S$ | 1 | 每次迭代固定的周期数 |
| 最大重试 | $R$ | 3 | 窗口扩展的最大重试次数 |
| 子问题时限 | - | 60秒 | 每个子问题的CPLEX时间限制 |

### 4.3 子问题 $\text{SP}(k, W)$ 定义

给定起始周期 $k$ 和窗口大小 $W$，定义时间区间:

$$
\begin{aligned}
T^{\text{fix}} &= \{1, 2, \ldots, k-1\} &\text{(已固定)} \\
T^{\text{win}} &= \{k, k+1, \ldots, k+W-1\} &\text{(当前窗口)} \\
T^{\text{rel}} &= \{k+W, k+W+1, \ldots, T\} &\text{(放松区)}
\end{aligned}
$$

子问题中变量类型:
- $t \in T^{\text{fix}}$: $y_{gt}, \lambda_{gt}$ 固定为 $\bar{y}_{gt}, \bar{\lambda}_{gt}$
- $t \in T^{\text{win}}$: $y_{gt}, \lambda_{gt} \in \{0,1\}$ (二元)
- $t \in T^{\text{rel}}$: $y_{gt}, \lambda_{gt} \in [0,1]$ (连续松弛)
- $u_i$: 在迭代过程中放松为连续，最终求解时恢复二元

### 4.4 算法流程

```
输入: 问题数据, 参数 W, S, R
输出: 可行解 (y*, lambda*, x*, ...)

1. 初始化:
   k = 1, W_current = W
   y_bar[g][t] = 0, lambda_bar[g][t] = 0 for all g, t

2. 主循环 (while k <= T):
   2.1 求解子问题 SP(k, W_current)

   2.2 if 求解成功:
       固定周期 [k, k+S) 的 y, lambda 值
       k = k + S
       W_current = W  // 重置窗口大小
   else:
       for retry = 1 to R:
           W_current = W_current + 1
           求解 SP(k, W_current)
           if 成功: break

       if 仍失败:
           回滚最近一次固定
           W_current = W + 2

3. 最终求解:
   固定所有 y, lambda
   恢复 u 为二元变量
   求解完整模型得到 x*, I*, b*, u*

4. 返回解
```

### 4.5 算法特点

**优点**:
- 实现简单，易于理解
- 每个子问题规模小，求解快
- 能处理大规模实例

**缺点**:
- 解质量依赖于固定顺序
- 早期决策可能导致后期不可行
- 没有后续优化机制

---

## 5. 算法二: RFO (RF + Fix-and-Optimize)

### 5.1 算法思想

**RFO** 是 RF 的增强版本，在 RF 构造初始解后，使用 **Fix-and-Optimize (FO)** 进行局部搜索优化:

1. **阶段1 (RF)**: 使用 RF 算法构造初始可行解
2. **阶段2 (FO)**: 滑动窗口局部优化，改进解质量
3. **阶段3 (Final)**: 固定所有二元变量，求解最终生产计划

### 5.2 FO 阶段参数

| 参数 | 符号 | 默认值 | 含义 |
|------|------|--------|------|
| FO窗口长度 | $W_o$ | 8 | FO优化窗口大小 |
| FO步长 | $S_o$ | 3 | FO窗口滑动步长 |
| 最大轮数 | $H$ | 2 | FO优化的最大轮数 |
| 边界缓冲 | $\Delta$ | 1 | 窗口边界的缓冲周期 |
| 子问题时限 | - | 30秒 | FO子问题时间限制 |

### 5.3 FO 邻域子问题 $\text{NSP}(a)$

给定窗口起点 $a$，定义扩展窗口:

$$
\text{WND}^+(a) = [\max(1, a - \Delta), \min(T, a + W_o + \Delta))
$$

邻域子问题中:
- $t \in \text{WND}^+(a)$: $y_{gt}, \lambda_{gt} \in \{0,1\}$ (二元，可优化)
- $t \notin \text{WND}^+(a)$: $y_{gt}, \lambda_{gt}$ 固定为当前最优值

### 5.4 算法流程

```
输入: 问题数据, RF参数, FO参数
输出: 优化后的解

===== 阶段1: RF =====
1. 执行 RF 算法得到初始解
   y_current, lambda_current, objective_current

===== 阶段2: FO =====
2. for h = 1 to H:
   improved = false

   for a = 0, S_o, 2*S_o, ... < T:
       2.1 求解 NSP(a)
       2.2 if 新目标 < objective_current - epsilon:
           更新 y_current, lambda_current
           objective_current = 新目标
           improved = true

   if not improved:
       break  // 无改进则提前终止

===== 阶段3: Final =====
3. 固定所有 y_current, lambda_current
4. 求解完整模型得到 x*, I*, b*, u*
5. 返回解
```

### 5.5 算法特点

**优点**:
- 在 RF 基础上显著改进解质量
- FO 阶段能逃离局部最优
- 多轮优化确保充分改进

**缺点**:
- 计算时间比 RF 长
- 改进幅度随轮次递减
- 窗口大小需要调优

---

## 6. 算法三: RR (Relax-and-Recover)

### 6.1 算法思想

**Relax-and-Recover (RR)** 是一种三阶段分解算法:

1. **Stage 1**: 固定 $\lambda = 0$，求解启动结构 $y^*$
2. **Stage 2**: 固定 $y^*$，最大化跨期变量 $\lambda^*$
3. **Stage 3**: 固定 $y^*$ 和 $\lambda^*$，求解最终生产计划

核心思想是将复杂的联合优化问题分解为三个较易求解的子问题。

### 6.2 Stage 1: 求解启动结构

**目标**: 在无跨期约束下，确定最优的启动模式 $y^*$

**模型特点**:
- 移除所有 $\lambda$ 变量 (等价于 $\lambda_{gt} = 0, \forall g, t$)
- 产能可选择放大 (默认系数 1.0，不放大)
- Big-M 约束简化为: $\sum_{i: h_{ig}=1} s^x_i \cdot x_{it} \leq C_t \cdot y_{gt}$

**输出**: $y^*_{gt}, \forall g, t$ (启动决策)

### 6.3 Stage 2: 求解跨期变量

**目标**: 在给定 $y^*$ 下，最大化跨期次数

$$
\max \sum_{g \in G} \sum_{t \in T} \lambda_{gt}
$$

**约束**:

**(a) 固定启动**:
$$
y_{gt} = y^*_{gt}, \quad \forall g, t
$$

**(b) 初始状态**:
$$
\lambda_{g,1} = 0, \quad \forall g
$$

**(c) 每期最多一个跨期**:
$$
\sum_{g \in G} \lambda_{gt} \leq 1, \quad \forall t
$$

**(d) 跨期连续性**:
$$
2\lambda_{gt} \leq y^*_{g,t-1} + y^*_{gt}, \quad \forall g, \forall t \geq 2
$$

> 只有当 $y^*_{g,t-1} = 1$ 且 $y^*_{gt} = 1$ 时，$\lambda_{gt}$ 才能为 1

**(e) 跨期排他性**:
$$
\lambda_{g,t-1} + \lambda_{gt} \leq 2 - \frac{1}{|G|} \sum_{g' \neq g} y^*_{g',t-1}, \quad \forall g, \forall t \geq 3
$$

**输出**: $\lambda^*_{gt}, \forall g, t$ (跨期决策)

### 6.4 Stage 3: 最终求解

**目标**: 固定二元变量，求解连续变量的最优配置

**固定规则**:
- 若 $\lambda^*_{gt} = 1$，则 $y_{gt} = 0$ (跨期替代启动)
- 否则 $y_{gt} = y^*_{gt}$

**求解**: 完整的 MILP 模型，但 $(y, \lambda)$ 已固定，问题退化为 LP (若 $u$ 也固定) 或小规模 MIP

**输出**: 完整的生产计划 $(x^*, I^*, P^*, b^*, u^*)$

### 6.5 算法流程

```
输入: 问题数据, CPLEX参数
输出: 最终解

===== Stage 1 =====
1. 构建无跨期模型 (lambda = 0)
2. 求解得到 y*
3. 记录 Step1 目标值和统计

===== Stage 2 =====
4. 构建跨期优化模型
5. 固定 y = y*
6. 最大化 sum(lambda)
7. 求解得到 lambda*

===== Stage 3 =====
8. 构建完整模型
9. 固定 y (考虑跨期替代) 和 lambda = lambda*
10. 求解最终生产计划
11. 返回解和指标
```

### 6.6 算法特点

**优点**:
- 分解清晰，每阶段目标明确
- Stage 2 是小规模纯整数规划，求解快
- 能找到较多的跨期机会，节省启动成本

**缺点**:
- Stage 1 的决策对后续影响大
- 无法修正 Stage 1 中的次优决策
- 最终目标值可能劣于直接求解

---

## 7. 约束结构分析

### 7.1 约束分类

| 约束 | 涉及变量 | 类型 | 紧致性 |
|------|----------|------|--------|
| 需求满足 | $x, u$ | 线性 | 通常紧 |
| 流平衡 | $x, I, P$ | 线性等式 | 始终紧 |
| 下游能力 | $P$ | 线性 | 部分紧 |
| 总产能 | $x, y$ | 线性 | 关键紧 |
| 家族Big-M | $x, y, \lambda$ | 大M | 决定性 |
| 跨期约束 | $y, \lambda$ | 线性 | 结构性 |
| 时间窗 | $x$ | 边界 | 强制 |
| 欠交定义 | $x, b$ | 线性等式 | 定义性 |

### 7.2 Big-M 约束的影响

Big-M 约束 $\sum_i s^x_i x_{it} \leq C_t (y_{gt} + \lambda_{gt})$ 是模型的核心:

- **当 $y_{gt} + \lambda_{gt} = 0$**: 该族不能生产任何产品
- **当 $y_{gt} + \lambda_{gt} = 1$**: 产能上限为 $C_t$，与总产能约束一致
- **M值选择**: 使用 $C_t$ 作为 M 值，已是最紧的合理选择

### 7.3 跨期约束的耦合性

跨期约束 (7)-(9) 形成时间上的耦合:

```
t=1      t=2      t=3      t=4      ...
y[g,1]   y[g,2]   y[g,3]   y[g,4]
  |        |        |        |
  +---->lambda[g,2] |        |
           |        |        |
           +---->lambda[g,3] |
                    |        |
                    +---->lambda[g,4]
```

这种链式结构使得:
- 早期周期的启动决策影响后续所有周期的跨期可能性
- 跨期一旦中断，需要重新启动才能继续

---

## 8. 算法比较与选择建议

### 8.1 性能对比

| 指标 | RF | RFO | RR |
|------|-----|-----|-----|
| **求解速度** | 快 | 中 | 快 |
| **解质量** | 中 | 高 | 中-高 |
| **稳定性** | 高 | 高 | 中 |
| **可扩展性** | 好 | 好 | 好 |
| **参数敏感** | 中 | 高 | 低 |

### 8.2 选择建议

**选择 RF 当**:
- 需要快速获得可行解
- 问题规模大，计算资源有限
- 解质量要求不高

**选择 RFO 当**:
- 追求最优解质量
- 有足够的计算时间
- RF 解质量不满意

**选择 RR 当**:
- 跨期节省是重要目标
- 问题有明显的阶段性结构
- 需要理解启动-跨期关系

### 8.3 典型结果范围

基于测试经验，对于典型规模 (N=100, T=30, G=5, F=5):

| 算法 | 目标值范围 | 求解时间 |
|------|-----------|---------|
| RF | 580K-620K | 5-15秒 |
| RFO | 570K-600K | 15-60秒 |
| RR | 575K-610K | 10-30秒 |

---

## 附录: 超参数配置参考

### RF 参数

```cpp
constexpr int kRFWindowSize = 6;       // W: 窗口长度
constexpr int kRFFixStep = 1;          // S: 固定步长
constexpr int kRFMaxRetries = 3;       // R: 最大扩展重试次数
constexpr double kRFSubproblemTimeLimit = 60.0;  // 子问题时间限制
```

### FO 参数 (RFO)

```cpp
constexpr int kFOWindowSize = 8;       // W_o: FO窗口长度
constexpr int kFOStep = 3;             // S_o: FO步长
constexpr int kFOMaxRounds = 2;        // H: 最大优化轮数
constexpr int kFOBoundaryBuffer = 1;   // Delta: 边界缓冲
constexpr double kFOSubproblemTimeLimit = 30.0;  // FO子问题时间限制
```

### CPLEX 参数

```cpp
constexpr double kDefaultCplexTimeLimit = 30.0;  // 总时间限制
// cplex_threads = 0 (自动)
// cplex_workmem = 4096 MB
// MIP Strategy File = 3 (节点文件存储)
```

---

*文档结束*
