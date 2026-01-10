> 生成时间: 2026-01-07 23:08

# Numerical Experiments for Production Planning with Grouping, Carryover, and Backlogging (PP-GCB)

## 1. Background

### 1.1 Research Context

Galvanized steel sheets are coated with metallic zinc to prevent surface corrosion and prolong product service life. Due to their excellent mechanical properties and superior corrosion resistance, galvanized steel sheets are extensively used as essential materials in construction, automotive, and various manufacturing industries. To satisfy the rising market demand and maintain competitiveness, steel manufacturers continuously seek effective strategies to enhance production efficiency and reduce operational costs.

### 1.2 Production Process

Galvanized steel sheets are produced in galvanizing lines based on customer or downstream process orders. Each order specifies the required weight of finished products and comprises multiple coils. The production process involves:

1. **Order Reception**: Each order has a demand quantity, ready time (earliest start), and due date (latest completion)
2. **Material Preparation**: Cold-rolled coils from upstream lines are assigned to orders
3. **Galvanization**: Steel strips pass through a galvanizing pot containing molten galvanizing material
4. **Setup Operations**: Switching galvanizing materials requires draining, cleaning, and remelting - incurring substantial downtime

### 1.3 Key Concepts

| Concept | Description |
|---------|-------------|
| **Time Window** | The period between an order's ready time and due date within which production should ideally occur |
| **Order Family (Group)** | Orders requiring the same galvanization materials, can be processed consecutively without setup |
| **Setup Carryover** | Production setup from one period continuing into the next, reducing setup time and costs |
| **Backlogging** | Production beyond due dates is permitted but incurs penalties |

---

## 2. Problem Description

### 2.1 Problem Overview

The production planning problem aims to generate an optimal production schedule that efficiently meets customer orders and downstream process demands within a defined planning horizon. The PP-GCB (Production Planning with Grouping, Carryover, and Backlogging) model addresses practical complexities in production scheduling.

### 2.2 Sets and Indices

| Symbol | Description |
|--------|-------------|
| N | Set of all orders, indexed by i |
| T | Set of time periods, indexed by t |
| F | Set of downstream processes (flows), indexed by f |
| G | Set of order families (groups), indexed by g |

### 2.3 Parameters

| Symbol | Description |
|--------|-------------|
| d_i | Total demand quantity for order i |
| e_i | Earliest allowable production time (ready time) for order i |
| l_i | Due date (latest permissible completion time) for order i |
| d_ft | Demand from downstream process f in period t |
| C_t | Maximum production capacity available in period t |
| D_ft | Processing capacity of downstream process f in period t |
| c_i^X | Unit production cost of order i |
| c_g^Y | Setup cost for order family g |
| c_f^I | Unit inventory holding cost for downstream process f |
| c_i^U | Penalty cost if total demand for order i is unmet |
| c_i^B | Penalty cost per unit unmet demand per unit time after due date |
| s_i^X | Capacity usage per unit production of order i |
| s_g^Y | Capacity usage for conducting setup of order family g |
| h_ig | Binary: 1 if order i belongs to family g, 0 otherwise |
| k_if | Binary: 1 if order i can fulfill downstream process f, 0 otherwise |

### 2.4 Decision Variables

| Variable | Type | Description |
|----------|------|-------------|
| x_it | Continuous | Production quantity of order i in period t |
| y_gt | Binary | 1 if setup is conducted for order family g in period t |
| I_ft | Continuous | Inventory level for downstream process f at end of period t |
| lambda_gt | Binary | 1 if setup carryover for family g occurs from t-1 to t |
| u_i | Binary | 1 if demand for order i is unmet |
| b_it | Continuous | Unmet demand quantity for order i at end of period t (t >= l_i) |
| P_ft | Continuous | Processing quantity for downstream process f in period t |

### 2.5 Mathematical Formulation

**Objective Function:**

Minimize total costs including production cost, backorder penalties, setup cost, inventory cost, and unmet demand penalties:

```
min  SUM_{i in N, t in T} c_i^X * x_it
   + SUM_{i in N, t=l_i}^T c_i^B * b_it
   + SUM_{g in G, t in T} c_g^Y * y_gt
   + SUM_{f in F, t in T} c_f^I * I_ft
   + SUM_{i in N} c_i^U * u_i
```

**Constraints:**

(2) **Flow Balance Constraint:**
```
SUM_{i in N} k_if * x_it + I_{f,t-1} - P_ft - I_ft = 0,  for all f in F, t in T
```

(3) **Processing Capacity Constraint:**
```
P_ft <= D_ft,  for all f in F, t in T
```

(4) **Demand Satisfaction:**
```
SUM_{t in T} x_it + d_i * u_i >= d_i,  for all i in N
```

(4.1) **Backorder Definition:**
```
d_i * u_i >= b_iT,  for all i in N
```

(5) **Production Capacity Constraint:**
```
SUM_{i in N} s_i^X * x_it + SUM_{g in G} s_g^Y * y_gt <= C_t,  for all t in T
```

(6) **Setup Requirement (Individual):**
```
h_ig * s_i^X * x_it - C_t * y_gt - C_t * lambda_gt <= 0,  for all i in N, g in G, t in T
```

(6.1) **Setup Requirement (Aggregate):**
```
SUM_{i in N} h_ig * s_i^X * x_it - C_t * y_gt - C_t * lambda_gt <= 0,  for all g in G, t in T
```

(7) **Single Carryover Constraint:**
```
SUM_{g in G} lambda_gt <= 1,  for all t in T
```

(8) **Carryover Feasibility:**
```
y_{g,t-1} + lambda_{g,t-1} - lambda_gt >= 0,  for all g in G, t in T
```

(9) **Carryover Exclusivity:**
```
lambda_gt + lambda_{g,t-1} + y_gt - SUM_{g' in G, g' != g} y_{g',t} <= 2,  for all g in G, t in T
```

(10) **Initial Conditions (Setup):**
```
y_g0 = 0, lambda_g0 = 0,  for all g in G
```

(11) **Initial Conditions (Inventory):**
```
I_f0 = 0,  for all f in F
```

(12) **Non-negativity (Production):**
```
x_it >= 0,  for all i in N, t in T
```

(13) **Time Window Constraint:**
```
SUM_{t < e_i} x_it = 0,  for all i in N
```

(14) **Backorder Tracking:**
```
d_i - SUM_{t' <= t} x_{it'} = b_it,  for all i in N, t in T, t >= l_i
```

(15) **Backorder Non-negativity:**
```
b_it >= 0,  for all i in N, l_i <= t <= T
```

(16) **Inventory/Processing Non-negativity:**
```
I_ft, P_ft >= 0,  for all f in F, t in T
```

(17) **Binary Variables:**
```
y_gt, lambda_gt in {0, 1},  for all g in G, t in T
```

(18) **Unmet Demand Binary:**
```
u_i in {0, 1},  for all i in N
```

---

## 3. Solution Algorithm

### 3.1 Three-Stage Decomposition Algorithm (RR/PP-GCB)

The algorithm leverages a structured approach combining heuristic and exact methods. Orders sharing identical families and material flows are merged into larger consolidated orders to reduce problem dimensionality.

#### Stage 1: Setup Structure Determination

- Fix all carryover variables lambda_gt = 0
- Increase production capacity C_t to a sufficiently large value (e.g., 10x)
- Solve the simplified model to obtain initial setup decisions y_gt*

**Purpose:** Determine preliminary setup structure without capacity restrictions.

#### Stage 2: Carryover Optimization

Using the setup configuration y_gt* from Stage 1, solve the following optimization to maximize setup carryovers:

```
max  SUM_{g in G, t in T} lambda_gt

subject to:
    SUM_{g in G} lambda_gt <= 1,  for all t in T
    2 * lambda_gt <= y_{g,t-1} + y_gt,  for all g in G, t in T
    lambda_{g,t-1} + lambda_gt <= 2 - SUM_{g' in G, g' != g} y_{g',t-1} / |G|,  for all g in G, t in T
    lambda_gt in {0, 1}
```

**Purpose:** Maximize operational efficiency through setup continuity.

#### Stage 3: Final Production Planning

- For all lambda_gt = 1, set corresponding y_gt = 0
- Fix lambda_gt and modified y_gt
- Solve the complete model with original capacity constraints

**Purpose:** Calculate final production quantities x_it, inventory I_ft, and unmet demand indicators u_i.

### 3.2 Alternative Algorithms

| Algorithm | Description | Characteristics |
|-----------|-------------|-----------------|
| **RF** (Relax-and-Fix) | Rolling horizon heuristic | Fast, good initial solutions |
| **RFO** (RF + Fix-and-Optimize) | Two-stage approach | Better quality with local refinement |
| **CPLEX Direct** | Full MIP solver | Optimal for small instances |

---

## 4. Numerical Experiment Design

### 4.1 Test Instance Generation

Test instances are generated using the **LS-NTGF-Data-Cap** tool with the following base parameters:

| Parameter | Value | Description |
|-----------|-------|-------------|
| T | 30 | Number of planning periods |
| F | 5 | Number of downstream processes (flows) |
| G | 5 | Number of order families (groups) |
| zoom | 60 | Capacity scaling factor |

### 4.2 Instance Scale

| Scale | Orders (N) | Instances | Total |
|-------|------------|-----------|-------|
| Small | 100 | 10 | 10 |
| Medium | 200 | 10 | 10 |
| Large | 300 | 10 | 10 |

**Base Instances:** 30 instances (Medium difficulty)

### 4.3 Difficulty Levels

| Difficulty | capacity_utilization | demand_cv | peak_multiplier |
|------------|---------------------|-----------|-----------------|
| Easy | 0.55 | 0.15 | 1.5 |
| Medium | 0.70 | 0.25 | 2.0 |
| Hard | 0.85 | 0.35 | 2.5 |

**Extended Instances:** 3 scales x 3 difficulties x 10 instances = 90 instances

### 4.4 Experiment 1: Algorithm Effectiveness Validation

**Objective:** Validate that the PP-GCB three-stage algorithm effectively solves the problem.

**Method:**
1. Run RR algorithm on 30 base instances (Medium difficulty)
2. Record objective values, runtime, and optimality gap for each stage

**Metrics:**
- Solution success rate
- Stage-wise objective convergence
- Total runtime

### 4.5 Experiment 2: Algorithm Comparison

**Objective:** Compare performance of four solution methods.

| Method | Description | Time Limit |
|--------|-------------|------------|
| CPLEX | Direct MIP solver | 300s |
| RF | Relax-and-Fix | 60s/subproblem |
| RFO | RF + Fix-and-Optimize | 30s/subproblem |
| RR | PP-GCB three-stage | 60s/stage |

**Metrics:**
- Objective value (total cost)
- Gap to CPLEX optimal solution
- Computation time
- Scalability (performance vs. problem size)

### 4.6 Experiment 3: Parameter Sensitivity Analysis

#### 4.6.1 Penalty Coefficient Sensitivity

| u_penalty | b_penalty |
|-----------|-----------|
| 5,000 | 50 |
| 10,000 | 100 |
| 20,000 | 200 |

**Test Instances:** N=200, 10 instances, RR algorithm

#### 4.6.2 Problem Difficulty Sensitivity

Test RR algorithm across Easy/Medium/Hard difficulty levels.

**Test Instances:** N=200, 10 instances per difficulty

#### 4.6.3 Order Merging Threshold Sensitivity

| Threshold | Description |
|-----------|-------------|
| 500 | Aggressive merging |
| 1,000 | Default |
| 2,000 | Conservative merging |
| No merge | Disabled |

**Test Instances:** N=300, 10 instances, RR algorithm

---

## 5. Experimental Tools

### 5.1 Tool Overview

| Tool | Purpose | Location |
|------|---------|----------|
| **LS-NTGF-Data-Cap** | Test instance generation | D:/YM-Code/LS-NTGF-Data-Cap/ |
| **LS-NTGF-All** | Solver backend (RF/RFO/RR + CPLEX) | D:/YM-Code/LS-NTGF-All/ |
| **LS-NTGF-GUI** | Graphical user interface | D:/YM-Code/LS-NTGF-GUI/ |

### 5.2 Instance Generator (LS-NTGF-Data-Cap)

**Configuration Example** (`src/main.cpp`):

```cpp
vector<int> N_list = { 100, 200, 300 };
vector<int> T_list = { 30 };
vector<int> F_list = { 5 };
vector<int> G_list = { 5 };
int case_size = 10;
double capacity_utilization = 0.70;  // Medium difficulty
```

**Output Format:** CSV files in `data/` directory

**Naming Convention:** `{zoom}_N{N}_T{T}_F{F}_G{G}_{difficulty}_{timestamp}.csv`

### 5.3 Solver Backend (LS-NTGF-All)

**Command Line Usage:**

```bash
LS-NTGF-All --algo=RR -f <data.csv> -o <output_dir> -l <log_file> \
            -t <time_limit> --u-penalty <val> --b-penalty <val> \
            --threshold <threshold> --cplex-workdir <dir> \
            --cplex-workmem <mem> --cplex-threads <threads>
```

**Algorithm Options:**
- `--algo=RF`: Relax-and-Fix
- `--algo=RFO`: RF + Fix-and-Optimize
- `--algo=RR`: PP-GCB three-stage decomposition

### 5.4 GUI Interface (LS-NTGF-GUI)

**Workflow:**
1. Select test instance file (Browse)
2. Configure algorithm and parameters
3. Click "Run" to start optimization
4. Monitor real-time progress and logs
5. View results in table format
6. Export logs if needed

**Output Locations:**
- Results: `results/solution_<algo>_<timestamp>/`
- Logs: `logs/log_<algo>_<timestamp>.log`

---

## 6. Result Analysis Templates

### 6.1 Algorithm Performance Comparison

| N | Method | Avg. Objective | Avg. Gap (%) | Avg. Time (s) | Success Rate |
|---|--------|---------------|--------------|---------------|--------------|
| 100 | CPLEX | - | - | - | - |
| 100 | RF | - | - | - | - |
| 100 | RFO | - | - | - | - |
| 100 | RR | - | - | - | - |
| 200 | CPLEX | - | - | - | - |
| ... | ... | ... | ... | ... | ... |

### 6.2 RR Algorithm Stage Details

| Stage | Objective | Avg. Value | Avg. Time (s) |
|-------|-----------|------------|---------------|
| Stage 1 | Determine setup structure y* | - | - |
| Stage 2 | Optimize carryover lambda* | - | - |
| Stage 3 | Final production plan | - | - |

### 6.3 Parameter Sensitivity Results

| u_penalty | b_penalty | Avg. Objective | Unmet Demand Rate |
|-----------|-----------|---------------|-------------------|
| 5,000 | 50 | - | - |
| 10,000 | 100 | - | - |
| 20,000 | 200 | - | - |

---

## 7. Expected Outcomes

### 7.1 Algorithm Effectiveness
- RR algorithm achieves feasible solutions for all test instances
- Three-stage decomposition shows clear objective improvement across stages

### 7.2 Algorithm Comparison
- CPLEX provides optimal solutions for small instances (N<=100)
- RR outperforms RF/RFO in solution quality for larger instances
- RF provides fastest computation times
- RFO balances quality and speed

### 7.3 Scalability
- Computation time grows moderately with problem size for heuristics
- CPLEX becomes intractable for large instances (N>=300)

### 7.4 Parameter Sensitivity
- Higher penalties reduce unmet demand but increase total cost
- Order merging significantly reduces computation time with minimal quality loss

---

## 8. Estimated Experiment Duration

| Experiment | Instances | Methods | Estimated Time |
|------------|-----------|---------|----------------|
| Experiment 1 | 30 | 1 (RR) | ~1 hour |
| Experiment 2 | 30 | 4 | ~4 hours |
| Experiment 3 | 70 | 1 (RR) | ~2 hours |

**Total:** Approximately 7-8 hours (can be parallelized)

---

## References

1. Verdejo et al. (2009) - Sequencing problem on continuous galvanizing line with Tabu Search
2. Martinez-de-Pison et al. (2011) - Annealing cycle optimization using AI and genetic algorithms
3. Gao et al. (2018) - Hybrid MILP decomposition for parallel continuous galvanizing lines
4. Brahimi et al. (2006) - Time windows extension for production planning
5. Sox and Gao (1999), Haase (1996) - Setup carryover implications
6. Dillenberger (1994) - Branch-and-bound with setup carryovers
