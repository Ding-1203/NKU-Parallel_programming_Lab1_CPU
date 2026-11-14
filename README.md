# Lab1 CPU 架构编程 — 体系结构相关性能测试

本项目为并行程序设计课程 Lab1 实验代码，包含两组 CPU 体系结构相关的性能对比实验及配套的自动化测试、性能剖析和可视化工具。

## 实验内容

### 实验 1：N×N 矩阵每列与给定向量的内积

对比两种访存模式对 cache 命中率的影响：

| 算法 | 访存模式 | 空间局部性 |
|------|----------|-----------|
| 平凡算法 (naive) | 逐列访问矩阵，步长为 n | 差，cache 频繁 miss |
| Cache 优化算法 (cache) | 逐行访问矩阵，步长为 1 | 好，与行主存储一致 |

### 实验 2：N 个数求和

对比串行依赖链与指令级并行对超标量流水线利用率的影响：

| 算法 | 原理 |
|------|------|
| 平凡算法 (naive) | 单链式逐个累加，严格串行依赖 |
| 2 路多链 (2way) | 2 个独立累加器，流水线并发 |
| 4 路多链 (4way) | 4 个独立累加器，进一步利用流水线 |
| 递归函数 (recursive) | 对称位置两两配对相加，规模递归减半 |
| 二重循环 (iterative) | 相邻元素两两相加的迭代版本 |

## 项目文件说明

### 核心源码

| 文件 | 说明 |
|------|------|
| `matrix_dot.cpp` | 矩阵-向量内积实验，支持 `naive/cache/all` 模式选择 |
| `sum.cpp` | N 个数求和实验，支持 `naive/2way/4way/recursive/iterative/all` 模式选择 |

### 自动化脚本

| 文件 | 说明 |
|------|------|
| `run_all.bat` | 一键编译 + 全规模批量测试 + 自动导出 CSV |
| `run_uprof_matrix.ps1` | 使用 AMDuProfPcm 对矩阵实验逐算法进行硬件事件 profiling |
| `run_uprof_sum.ps1` | 使用 AMDuProfPcm 对求和实验逐算法进行硬件事件 profiling |

### 可视化脚本

| 文件 | 说明 |
|------|------|
| `basic_plot.py` | 读取基础测试 CSV，绘制运行时间和加速比图表 |
| `advanced_plot.py` | 读取 profiler CSV，绘制 cache miss、IPC 等硬件指标对比图 |

### 测试结果目录

`test_results/` 下包含所有自动生成的数据和图表：

| 文件 | 说明 |
|------|------|
| `matrix_results.csv` | 矩阵实验基础测试结果 |
| `sum_results.csv` | 求和实验基础测试结果 |
| `matrix_summary.csv` | 矩阵实验 profiler 汇总数据 |
| `sum_summary.csv` | 求和实验 profiler 汇总数据 |
| `*.png` | 自动生成的图表 |

## 环境要求

- **操作系统**：Windows
- **编译器**：g++ (MinGW-w64)，支持 C++17
- **Python**：3.8+（用于画图，需 `pandas` 和 `matplotlib`）
- **Profiler**（可选）：AMD uProf（AMDuProfPcm）

## 编译与运行

### 编译

```bash
g++ -O2 -std=c++17 matrix_dot.cpp -o matrix_dot.exe
g++ -O2 -std=c++17 sum.cpp -o sum.exe
```

### 命令行格式

```bash
.\matrix_dot.exe <n> [naive|cache|all] [repeat]
.\sum.exe <n> [naive|2way|4way|recursive|iterative|all] [repeat]
```

- `n`：问题规模（求和实验要求 n 为 2 的幂）
- `mode`：算法选择，默认 `all`；指定单算法时只运行该算法，适合 profiling
- `repeat`：可选，手动指定重复测量次数；不传则自动校准（目标总时间约 1.5 秒）

### 运行示例

```bash
# 完整对比
.\matrix_dot.exe 1024 all
.\sum.exe 1048576 all

# 单算法模式（适合 profiler attach）
.\matrix_dot.exe 4096 naive
.\sum.exe 1048576 2way

# 手动指定 repeat
.\matrix_dot.exe 4096 cache 50
.\sum.exe 1048576 4way 500
```

### 一键批量测试

双击 `run_all.bat` 或在终端运行：

```bash
run_all.bat
```

自动完成编译、全规模测试、导出 CSV。

### 画图

```bash
pip install pandas matplotlib
python basic_plot.py
python advanced_plot.py
```

## 建议测试规模

| 实验 | 规模范围 | 说明 |
|------|----------|------|
| 矩阵 | 64, 128, 256, 512, 1024, 2048, 4096, 8192 | 覆盖 L1/L2/L3 cache 边界 |
| 求和 | 1024 ~ 1048576（2 的幂） | 观察超标量效果随规模变化趋势 |
