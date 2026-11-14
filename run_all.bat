@echo off
chcp 65001 >nul 2>&1
setlocal EnableDelayedExpansion

echo ============================================================
echo   CPU Architecture Lab - One-Click Benchmark + CSV Export
echo ============================================================
echo.

:: ---- [1/4] Compile ----
echo [1/4] Compiling...

g++ -O2 -std=c++17 matrix_dot.cpp -o matrix_dot.exe 2>&1
if errorlevel 1 (
    echo FAILED: matrix_dot.cpp compilation error!
    pause
    exit /b 1
)
echo   matrix_dot.cpp  OK

g++ -O2 -std=c++17 sum.cpp -o sum.exe 2>&1
if errorlevel 1 (
    echo FAILED: sum.cpp compilation error!
    pause
    exit /b 1
)
echo   sum.cpp         OK
echo.

:: ---- [2/4] Matrix Dot Product ----
echo ============================================================
echo   Experiment 1: Matrix-Vector Dot Product (cache)
echo ============================================================
echo.

:: 写 CSV 表头
echo n,repeat,warmup,naive_time_ms,cache_time_ms,speedup,check> matrix_results.csv

for %%N in (64 128 256 512 1024 2048 4096 8192) do (
    echo ------ n = %%N ------
    for /f "tokens=*" %%L in ('matrix_dot.exe %%N all') do (
        set "LINE=%%L"
        :: 如果是 CSV: 开头的行，截取内容写入 csv
        if "!LINE:~0,4!"=="CSV:" (
            echo !LINE:~4!>> matrix_results.csv
        ) else (
            echo %%L
        )
    )
    echo.
)

echo   [Exported] matrix_results.csv
echo.

:: ---- [3/4] N-Element Summation ----
echo ============================================================
echo   Experiment 2: N-Element Summation (superscalar)
echo ============================================================
echo.

:: 写 CSV 表头
echo n,repeat_naive,repeat_2way,repeat_4way,repeat_recursive,repeat_iterative,warmup,naive_ms,2way_ms,4way_ms,recursive_ms,iterative_ms,speedup_2way,speedup_4way,speedup_recursive,speedup_iterative,check_2way,check_4way,check_recursive,check_iterative> sum_results.csv

for %%N in (1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576) do (
    echo ------ n = %%N ------
    for /f "tokens=*" %%L in ('sum.exe %%N all') do (
        set "LINE=%%L"
        if "!LINE:~0,4!"=="CSV:" (
            echo !LINE:~4!>> sum_results.csv
        ) else (
            echo %%L
        )
    )
    echo.
)

echo   [Exported] sum_results.csv
echo.

:: ---- [4/4] Done ----
echo ============================================================
echo   All tests completed.
echo   Output files:
echo     matrix_results.csv
echo     sum_results.csv
echo   Run 'python plot_results.py' to generate plots.
echo ============================================================
pause
