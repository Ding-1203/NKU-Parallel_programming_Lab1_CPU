/**
 * matrix_dot.cpp
 * 实验 1：N×N 矩阵每一列与给定向量的内积
 *
 * 两种算法对比：
 *   1) 平凡算法 (naive) —— 逐列访问矩阵（外层枚举列 i，内层枚举行 j）
 *      由于 C/C++ 二维数组按行主序存储，逐列访问的步长为 n，
 *      相邻两次内存访问跨越一整行，空间局部性差，cache 命中率低。
 *
 *   2) cache 优化算法 (cache) —— 逐行访问矩阵（外层枚举行 j，内层枚举列 i）
 *      访问步长为 1，与行主存储顺序一致，空间局部性好，
 *      cache line 被充分利用，性能显著优于平凡算法。
 *
 * 编译：g++ -O2 -std=c++17 matrix_dot.cpp -o matrix_dot.exe
 * 运行：.\matrix_dot.exe <n> [mode] [repeat]
 *        mode: naive | cache | all (默认 all)
 *        repeat: 手动指定重复次数（可选，默认自动校准）
 */

#include <cmath>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// ============================================================
// 数据生成
// ============================================================

/// 生成长度为 n 的确定性向量，vec[i] = i % 100
std::vector<double> generate_vector(std::size_t n) {
    std::vector<double> vec(n);
    for (std::size_t i = 0; i < n; ++i) {
        vec[i] = static_cast<double>(i % 100);
    }
    return vec;
}

/// 生成 n×n 行主序矩阵（一维数组），mat[row * n + col] = (row + col) % 100
std::vector<double> generate_matrix(std::size_t n) {
    std::vector<double> mat(n * n);
    for (std::size_t row = 0; row < n; ++row) {
        for (std::size_t col = 0; col < n; ++col) {
            mat[row * n + col] = static_cast<double>((row + col) % 100);
        }
    }
    return mat;
}

// ============================================================
// 核心算法
// ============================================================

/**
 * 平凡算法：逐列访问矩阵
 *
 * 外层循环枚举列 i，内层循环枚举行 j。
 * 每步外层循环完整计算出 result[i] = sum_j mat[j][i] * vec[j]。
 *
 * 访存分析：mat[j * n + i] 中 j 变化时地址增量为 n（跨一整行），
 * 导致连续访问在内存中不相邻，cache 命中率低。
 */
void dot_naive_col_major(const std::vector<double>& mat,
                         const std::vector<double>& vec,
                         std::vector<double>& result,
                         std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        result[i] = 0.0;
        for (std::size_t j = 0; j < n; ++j) {
            result[i] += mat[j * n + i] * vec[j];
        }
    }
}

/**
 * cache 优化算法：逐行访问矩阵
 *
 * 先将 result 清零，然后外层循环枚举行 j，内层循环枚举列 i。
 * 一步外层循环不会计算出任何一个完整的内积，
 * 而是向每个 result[i] 累加一次乘法结果 mat[j][i] * vec[j]。
 *
 * 访存分析：mat[j * n + i] 中 i 变化时地址增量为 1，
 * 与行主存储顺序一致，连续访问在内存中相邻，
 * 一条 cache line 上的数据会被连续使用，cache 命中率高。
 */
void dot_cache_row_major(const std::vector<double>& mat,
                         const std::vector<double>& vec,
                         std::vector<double>& result,
                         std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        result[i] = 0.0;
    }
    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t i = 0; i < n; ++i) {
            result[i] += mat[j * n + i] * vec[j];
        }
    }
}

// ============================================================
// 正确性校验
// ============================================================

bool nearly_equal(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) <= eps * (1.0 + std::fabs(a) + std::fabs(b));
}

bool check_vectors_equal(const std::vector<double>& a,
                         const std::vector<double>& b,
                         double eps = 1e-9) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (!nearly_equal(a[i], b[i], eps)) return false;
    }
    return true;
}

// ============================================================
// repeat 自动校准
// ============================================================

/**
 * choose_repeat_by_probe: 通过试跑估算单次执行时间，反推 repeat 次数。
 *
 * 思路：先试跑 probe_runs 次，测量总时间，估算单次执行时间。
 * 然后根据目标总测量时间 target_ms 反推 repeat = target_ms / single_ms。
 * repeat 不低于 min_repeat。
 *
 * @param func        待测函数
 * @param target_ms   目标总测量时间（毫秒），推荐 1000~2000
 * @param min_repeat  repeat 下限
 * @param probe_runs  试跑次数
 * @return            推荐的 repeat 次数
 */
template <typename Func>
int choose_repeat_by_probe(Func&& func, double target_ms = 3000.0,
                           int min_repeat = 3, int probe_runs = 3) {
    // 试跑
    volatile double sink = 0.0;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < probe_runs; ++i) {
        func();
    }
    auto end = std::chrono::steady_clock::now();
    // 防止死代码消除
    (void)sink;

    double probe_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double single_ms = probe_ms / probe_runs;

    if (single_ms <= 0.0) {
        return min_repeat; // 时间太短无法估算，用下限
    }

    int repeat = static_cast<int>(target_ms / single_ms);
    if (repeat < min_repeat) repeat = min_repeat;

    return repeat;
}

// ============================================================
// 统一计时框架
// ============================================================

/**
 * benchmark: 预热 + 重复测量，返回平均时间（毫秒）。
 */
double benchmark(const std::function<void()>& func,
                 const std::vector<double>& result,
                 std::size_t n,
                 int warmup,
                 int repeat) {
    for (int w = 0; w < warmup; ++w) {
        func();
    }

    auto start = std::chrono::steady_clock::now();
    for (int r = 0; r < repeat; ++r) {
        func();
    }
    auto end = std::chrono::steady_clock::now();

    volatile double sink = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        sink += result[i];
    }
    (void)sink;

    double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
    return total_ms / repeat;
}

// ============================================================
// 主程序
// ============================================================

int main(int argc, char* argv[]) {
    // ---- 参数解析 ----
    std::size_t n = 1024;
    std::string mode = "all";
    int user_repeat = -1; // -1 表示自动

    if (argc >= 2) {
        try {
            long long val = std::stoll(argv[1]);
            if (val <= 0) {
                std::cerr << "Error: n must be a positive integer.\n";
                std::cerr << "Usage: " << argv[0] << " <n> [naive|cache|all] [repeat]\n";
                return 1;
            }
            n = static_cast<std::size_t>(val);
        } catch (...) {
            std::cerr << "Error: invalid argument '" << argv[1] << "'.\n";
            std::cerr << "Usage: " << argv[0] << " <n> [naive|cache|all] [repeat]\n";
            return 1;
        }
    }
    if (argc >= 3) {
        mode = argv[2];
        if (mode != "naive" && mode != "cache" && mode != "all") {
            std::cerr << "Error: mode must be 'naive', 'cache', or 'all'.\n";
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

    // ---- 数据生成 ----
    auto vec = generate_vector(n);
    auto mat = generate_matrix(n);

    int warmup = 3;
    bool run_naive = (mode == "naive" || mode == "all");
    bool run_cache = (mode == "cache" || mode == "all");

    // ---- 确定 repeat (可能根据最慢算法校准) ----
    // 对矩阵实验，naive 是最慢的算法，用它来校准
    std::vector<double> result_buf(n);
    int repeat;
    if (user_repeat > 0) {
        repeat = user_repeat;
    } else if (run_naive) {
        // 用 naive 算法校准（它最慢）
        repeat = choose_repeat_by_probe(
            [&]() { dot_naive_col_major(mat, vec, result_buf, n); },
            1500.0, /*min_repeat=*/2, /*probe_runs=*/2);
    } else {
        // 只跑 cache 算法
        repeat = choose_repeat_by_probe(
            [&]() { dot_cache_row_major(mat, vec, result_buf, n); },
            1500.0, /*min_repeat=*/2, /*probe_runs=*/3);
    }

    // ---- 输出实验参数 ----
    std::cout << "n = " << n << "\n";
    std::cout << "mode = " << mode << "\n";
    std::cout << "warmup = " << warmup << "\n";
    std::cout << "repeat = " << repeat << "\n";

    // ---- 运行算法 ----
    double naive_ms = 0.0, cache_ms = 0.0;
    std::vector<double> result_naive(n), result_cache(n);

    if (run_naive) {
        naive_ms = benchmark(
            [&]() { dot_naive_col_major(mat, vec, result_naive, n); },
            result_naive, n, warmup, repeat);
    }
    if (run_cache) {
        cache_ms = benchmark(
            [&]() { dot_cache_row_major(mat, vec, result_cache, n); },
            result_cache, n, warmup, repeat);
    }

    // ---- 输出结果 ----
    std::cout << std::fixed << std::setprecision(4);

    if (mode == "all") {
        bool pass = check_vectors_equal(result_naive, result_cache);
        double speedup = (cache_ms > 0) ? naive_ms / cache_ms : 0.0;

        std::cout << "naive_time_ms       = " << naive_ms << "\n";
        std::cout << "cache_opt_time_ms   = " << cache_ms << "\n";
        std::cout << "speedup             = " << speedup << "\n";
        std::cout << "check               = " << (pass ? "PASS" : "FAIL") << "\n";

        // CSV 行：供批处理脚本解析（CSV: 前缀标识）
        std::cout << "CSV:" << n << "," << repeat << "," << warmup << ","
                  << naive_ms << "," << cache_ms << "," << speedup << ","
                  << (pass ? "PASS" : "FAIL") << "\n";

        if (!pass) {
            std::cerr << "WARNING: results mismatch!\n";
            int count = 0;
            for (std::size_t i = 0; i < n && count < 5; ++i) {
                if (!nearly_equal(result_naive[i], result_cache[i])) {
                    std::cerr << "  [" << i << "]: naive=" << result_naive[i]
                              << "  cache=" << result_cache[i] << "\n";
                    ++count;
                }
            }
        }
    } else if (mode == "naive") {
        std::cout << "naive_time_ms       = " << naive_ms << "\n";
    } else {
        std::cout << "cache_opt_time_ms   = " << cache_ms << "\n";
    }

    return 0;
}
