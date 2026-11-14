/**
 * sum.cpp
 * 实验 2：N 个数求和
 *
 * 五种算法对比：
 *   1) naive     —— 单链式逐个累加，串行依赖链，无法利用超标量。
 *   2) 2way      —— 2 路独立累加器，IPC 接近翻倍。
 *   3) 4way      —— 4 路独立累加器，进一步利用流水线。
 *   4) recursive —— 递归两两配对相加，减少依赖链长度。
 *   5) iterative —— 二重循环两两相加，迭代版递归。
 *
 * 编译：g++ -O2 -std=c++17 sum.cpp -o sum.exe
 * 运行：.\sum.exe <n> [mode] [repeat]
 *        mode: naive | 2way | 4way | recursive | iterative | all (默认 all)
 *        repeat: 手动指定重复次数（可选，默认自动校准）
 *        n 须为 2 的幂
 */

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// ============================================================
// 辅助函数
// ============================================================

bool is_power_of_two(std::size_t n) {
    return n > 0 && (n & (n - 1)) == 0;
}

// ============================================================
// 数据生成
// ============================================================

/// data[i] = (i % 100) + 1.0
std::vector<double> generate_data(std::size_t n) {
    std::vector<double> data(n);
    for (std::size_t i = 0; i < n; ++i) {
        data[i] = static_cast<double>((i % 100) + 1);
    }
    return data;
}

// ============================================================
// 核心算法（全部保留不动）
// ============================================================

/**
 * 平凡算法：单链式逐个累加
 * 每次加法依赖上一次的 sum 值，形成严格的串行依赖链。
 */
double sum_naive_chain(const double* data, std::size_t n) {
    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        sum += data[i];
    }
    return sum;
}

/**
 * 多链路式累加（2 路特化）
 * sum1、sum2 两条独立累加链，无数据依赖，流水线可并发。
 */
double sum_2way(const double* data, std::size_t n) {
    double sum1 = 0.0, sum2 = 0.0;
    std::size_t i = 0;
    std::size_t main_end = n - (n % 2);
    for (; i < main_end; i += 2) {
        sum1 += data[i];
        sum2 += data[i + 1];
    }
    for (; i < n; ++i) {
        sum1 += data[i];
    }
    return sum1 + sum2;
}

/**
 * 多链路式累加（4 路特化）
 * 4 个独立累加器 sum1~sum4，充分利用 4 条流水线。
 */
double sum_4way(const double* data, std::size_t n) {
    double sum1 = 0.0, sum2 = 0.0, sum3 = 0.0, sum4 = 0.0;
    std::size_t i = 0;
    std::size_t main_end = n - (n % 4);
    for (; i < main_end; i += 4) {
        sum1 += data[i];
        sum2 += data[i + 1];
        sum3 += data[i + 2];
        sum4 += data[i + 3];
    }
    for (; i < n; ++i) {
        sum1 += data[i];
    }
    return sum1 + sum2 + sum3 + sum4;
}

/**
 * 通用多链路式累加（参数化 lanes 版本，仅用于扩展实验）
 */
double sum_multi_accumulator(const double* data, std::size_t n, std::size_t lanes) {
    constexpr std::size_t MAX_LANES = 16;
    if (lanes > MAX_LANES) lanes = MAX_LANES;
    if (lanes < 1) lanes = 1;

    double accum[MAX_LANES] = {};
    std::size_t i = 0;
    std::size_t main_end = n - (n % lanes);

    for (; i < main_end; i += lanes) {
        for (std::size_t k = 0; k < lanes; ++k) {
            accum[k] += data[i + k];
        }
    }
    for (; i < n; ++i) {
        accum[0] += data[i];
    }

    double sum = 0.0;
    for (std::size_t k = 0; k < lanes; ++k) {
        sum += accum[k];
    }
    return sum;
}

/**
 * 递归函数（内部实现）
 * 每轮前半部分与后半部分对称位置两两相加，规模减半后递归。
 * 要求 n 为 2 的幂。
 */
void sum_recursive_impl(double* work, std::size_t n) {
    if (n == 1) return;
    for (std::size_t i = 0; i < n / 2; ++i) {
        work[i] += work[n - i - 1];
    }
    sum_recursive_impl(work, n / 2);
}

/// 递归函数（带预分配工作数组）
double sum_recursive(const double* data, double* work, std::size_t n) {
    std::memcpy(work, data, n * sizeof(double));
    sum_recursive_impl(work, n);
    return work[0];
}

/**
 * 二重循环（迭代两两相加）
 * 外层循环有效长度逐轮减半，内层循环相邻元素两两相加写回前部。
 * 要求 n 为 2 的幂。
 */
double sum_iterative_pairwise(const double* data, double* work, std::size_t n) {
    std::memcpy(work, data, n * sizeof(double));
    for (std::size_t m = n; m > 1; m /= 2) {
        for (std::size_t i = 0; i < m / 2; ++i) {
            work[i] = work[i * 2] + work[i * 2 + 1];
        }
    }
    return work[0];
}

// ============================================================
// 正确性校验
// ============================================================

bool nearly_equal(double a, double b, double eps = 1e-6) {
    return std::fabs(a - b) <= eps * (1.0 + std::fabs(a) + std::fabs(b));
}

// ============================================================
// repeat 自动校准
// ============================================================

/**
 * choose_repeat_by_probe: 通过试跑估算单次执行时间，反推 repeat 次数。
 *
 * 每个 mode 单独调用一次，这样不会出现：
 * - 用 naive 校准出的 repeat 对 2way / 4way 太小
 * - 导致 profiling 总时长不足，uProf 没数据
 */
template <typename Func>
int choose_repeat_by_probe(Func&& func,
                           double target_ms = 3500.0,
                           int min_repeat = 100,
                           int probe_runs = 5,
                           int max_repeat = 200000000) {
    volatile double sink = 0.0;

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < probe_runs; ++i) {
        sink = func();
    }
    auto end = std::chrono::steady_clock::now();
    (void)sink;

    double probe_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double single_ms = probe_ms / probe_runs;

    if (single_ms <= 0.0) {
        return min_repeat;
    }

    long long repeat = static_cast<long long>(target_ms / single_ms);
    if (repeat < min_repeat) {
        repeat = min_repeat;
    }
    if (repeat > max_repeat) {
        repeat = max_repeat;
    }
    return static_cast<int>(repeat);
}

// ============================================================
// 统一计时框架
// ============================================================

template <typename Func>
std::pair<double, double> benchmark(Func&& func, int warmup, int repeat) {
    volatile double sink = 0.0;

    for (int w = 0; w < warmup; ++w) {
        sink = func();
    }

    auto start = std::chrono::steady_clock::now();
    for (int r = 0; r < repeat; ++r) {
        sink = func();
    }
    auto end = std::chrono::steady_clock::now();

    double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
    return {total_ms / repeat, static_cast<double>(sink)};
}

// ============================================================
// 主程序
// ============================================================

int main(int argc, char* argv[]) {
    // ---- 参数解析 ----
    std::size_t n = 1048576;
    std::string mode = "all";
    int user_repeat = -1;

    if (argc >= 2) {
        try {
            long long val = std::stoll(argv[1]);
            if (val <= 0) {
                std::cerr << "Error: n must be a positive integer.\n";
                std::cerr << "Usage: " << argv[0]
                          << " <n> [naive|2way|4way|recursive|iterative|all] [repeat]\n";
                return 1;
            }
            n = static_cast<std::size_t>(val);
        } catch (...) {
            std::cerr << "Error: invalid argument '" << argv[1] << "'.\n";
            std::cerr << "Usage: " << argv[0]
                      << " <n> [naive|2way|4way|recursive|iterative|all] [repeat]\n";
            return 1;
        }
    }

    if (argc >= 3) {
        mode = argv[2];
        if (mode != "naive" && mode != "2way" && mode != "4way" &&
            mode != "recursive" && mode != "iterative" && mode != "all") {
            std::cerr << "Error: mode must be naive|2way|4way|recursive|iterative|all.\n";
            return 1;
        }
    }

    if (argc >= 4) {
        try {
            user_repeat = std::stoi(argv[3]);
            if (user_repeat <= 0) {
                std::cerr << "Error: repeat must be a positive integer.\n";
                return 1;
            }
        } catch (...) {
            std::cerr << "Error: invalid repeat '" << argv[3] << "'.\n";
            return 1;
        }
    }

    // 递归和二重循环算法要求 n 为 2 的幂
    if (!is_power_of_two(n)) {
        std::cerr << "Warning: n = " << n << " is not a power of 2.\n";
        std::cerr << "Please provide n as a power of 2 (e.g., 1024, 4096, 1048576).\n";
        return 1;
    }

    // ---- 数据生成 ----
    auto data = generate_data(n);
    const double* ptr = data.data();
    std::vector<double> work(n);
    double* wptr = work.data();

    // ---- 运行模式 ----
    bool do_naive     = (mode == "naive"     || mode == "all");
    bool do_2way      = (mode == "2way"      || mode == "all");
    bool do_4way      = (mode == "4way"      || mode == "all");
    bool do_recursive = (mode == "recursive" || mode == "all");
    bool do_iterative = (mode == "iterative" || mode == "all");

    // ---- 每个 mode 单独确定 repeat ----
    int warmup = 5;

    int repeat_naive = 0;
    int repeat_2way = 0;
    int repeat_4way = 0;
    int repeat_recursive = 0;
    int repeat_iterative = 0;

    if (user_repeat > 0) {
        repeat_naive     = user_repeat;
        repeat_2way      = user_repeat;
        repeat_4way      = user_repeat;
        repeat_recursive = user_repeat;
        repeat_iterative = user_repeat;
    } else {
        if (do_naive) {
            repeat_naive = choose_repeat_by_probe(
                [&]() { return sum_naive_chain(ptr, n); },
                3500.0, 100, 5);
        }

        if (do_2way) {
            repeat_2way = choose_repeat_by_probe(
                [&]() { return sum_2way(ptr, n); },
                3500.0, 100, 5);
        }

        if (do_4way) {
            repeat_4way = choose_repeat_by_probe(
                [&]() { return sum_4way(ptr, n); },
                4000.0, 100, 5);
        }

        if (do_recursive) {
            repeat_recursive = choose_repeat_by_probe(
                [&]() { return sum_recursive(ptr, wptr, n); },
                3500.0, 100, 5);
        }

        if (do_iterative) {
            repeat_iterative = choose_repeat_by_probe(
                [&]() { return sum_iterative_pairwise(ptr, wptr, n); },
                3500.0, 100, 5);
        }
    }

    // ---- 输出实验参数 ----
    std::cout << "n = " << n << "\n";
    std::cout << "mode = " << mode << "\n";
    std::cout << "warmup = " << warmup << "\n";

    if (mode == "naive") {
        std::cout << "repeat = " << repeat_naive << "\n";
    } else if (mode == "2way") {
        std::cout << "repeat = " << repeat_2way << "\n";
    } else if (mode == "4way") {
        std::cout << "repeat = " << repeat_4way << "\n";
    } else if (mode == "recursive") {
        std::cout << "repeat = " << repeat_recursive << "\n";
    } else if (mode == "iterative") {
        std::cout << "repeat = " << repeat_iterative << "\n";
    } else {
        std::cout << "repeat_naive = " << repeat_naive << "\n";
        std::cout << "repeat_2way = " << repeat_2way << "\n";
        std::cout << "repeat_4way = " << repeat_4way << "\n";
        std::cout << "repeat_recursive = " << repeat_recursive << "\n";
        std::cout << "repeat_iterative = " << repeat_iterative << "\n";
    }

    // ---- 运行选定的算法 ----
    double naive_ms = 0.0, multi2_ms = 0.0, multi4_ms = 0.0, recursive_ms = 0.0, iterative_ms = 0.0;
    double naive_r = 0.0, multi2_r = 0.0, multi4_r = 0.0, recursive_r = 0.0, iterative_r = 0.0;

    if (do_naive) {
        auto [ms, r] = benchmark([&]() { return sum_naive_chain(ptr, n); }, warmup, repeat_naive);
        naive_ms = ms;
        naive_r = r;
    }

    if (do_2way) {
        auto [ms, r] = benchmark([&]() { return sum_2way(ptr, n); }, warmup, repeat_2way);
        multi2_ms = ms;
        multi2_r = r;
    }

    if (do_4way) {
        auto [ms, r] = benchmark([&]() { return sum_4way(ptr, n); }, warmup, repeat_4way);
        multi4_ms = ms;
        multi4_r = r;
    }

    if (do_recursive) {
        auto [ms, r] = benchmark([&]() { return sum_recursive(ptr, wptr, n); }, warmup, repeat_recursive);
        recursive_ms = ms;
        recursive_r = r;
    }

    if (do_iterative) {
        auto [ms, r] = benchmark([&]() { return sum_iterative_pairwise(ptr, wptr, n); }, warmup, repeat_iterative);
        iterative_ms = ms;
        iterative_r = r;
    }

    // ---- 输出结果 ----
    std::cout << std::fixed << std::setprecision(4);

    if (mode == "all") {
        bool ck2 = nearly_equal(naive_r, multi2_r);
        bool ck4 = nearly_equal(naive_r, multi4_r);
        bool ckR = nearly_equal(naive_r, recursive_r);
        bool ckI = nearly_equal(naive_r, iterative_r);

        double sp2 = (multi2_ms > 0.0)    ? naive_ms / multi2_ms    : 0.0;
        double sp4 = (multi4_ms > 0.0)    ? naive_ms / multi4_ms    : 0.0;
        double spR = (recursive_ms > 0.0) ? naive_ms / recursive_ms : 0.0;
        double spI = (iterative_ms > 0.0) ? naive_ms / iterative_ms : 0.0;

        std::cout << "naive_time_ms              = " << naive_ms << "\n";
        std::cout << "multi_2way_time_ms         = " << multi2_ms << "\n";
        std::cout << "multi_4way_time_ms         = " << multi4_ms << "\n";
        std::cout << "recursive_time_ms          = " << recursive_ms << "\n";
        std::cout << "iterative_pairwise_time_ms = " << iterative_ms << "\n";

        std::cout << std::setprecision(2);
        std::cout << "speedup_2way               = " << sp2 << "\n";
        std::cout << "speedup_4way               = " << sp4 << "\n";
        std::cout << "speedup_recursive          = " << spR << "\n";
        std::cout << "speedup_iterative          = " << spI << "\n";

        std::cout << "check_2way                 = " << (ck2 ? "PASS" : "FAIL") << "\n";
        std::cout << "check_4way                 = " << (ck4 ? "PASS" : "FAIL") << "\n";
        std::cout << "check_recursive            = " << (ckR ? "PASS" : "FAIL") << "\n";
        std::cout << "check_iterative            = " << (ckI ? "PASS" : "FAIL") << "\n";

        std::cout << std::fixed << std::setprecision(4);
        std::cout << "CSV:" << n << ","
                  << repeat_naive << "," << repeat_2way << "," << repeat_4way << ","
                  << repeat_recursive << "," << repeat_iterative << ","
                  << warmup << ","
                  << naive_ms << "," << multi2_ms << "," << multi4_ms << ","
                  << recursive_ms << "," << iterative_ms << ","
                  << std::setprecision(2)
                  << sp2 << "," << sp4 << "," << spR << "," << spI << ","
                  << (ck2 ? "PASS" : "FAIL") << ","
                  << (ck4 ? "PASS" : "FAIL") << ","
                  << (ckR ? "PASS" : "FAIL") << ","
                  << (ckI ? "PASS" : "FAIL") << "\n";

        if (!ck2 || !ck4 || !ckR || !ckI) {
            std::cerr << "\nWARNING: result mismatch detected!\n";
            std::cerr << std::fixed << std::setprecision(10);
            std::cerr << "  naive = " << naive_r << "\n";
            if (!ck2) std::cerr << "  2way  = " << multi2_r << "\n";
            if (!ck4) std::cerr << "  4way  = " << multi4_r << "\n";
            if (!ckR) std::cerr << "  recur = " << recursive_r << "\n";
            if (!ckI) std::cerr << "  iter  = " << iterative_r << "\n";
        }
    } else {
        double ms = 0.0;
        std::string label;

        if (mode == "naive") {
            ms = naive_ms;
            label = "naive_time_ms";
        } else if (mode == "2way") {
            ms = multi2_ms;
            label = "2way_time_ms";
        } else if (mode == "4way") {
            ms = multi4_ms;
            label = "4way_time_ms";
        } else if (mode == "recursive") {
            ms = recursive_ms;
            label = "recursive_time_ms";
        } else if (mode == "iterative") {
            ms = iterative_ms;
            label = "iterative_time_ms";
        }

        std::cout << label << " = " << ms << "\n";
    }

    return 0;
}
