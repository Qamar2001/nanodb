@echo off
echo ============================================
echo   NanoDB - Compiling...
echo ============================================

g++ -std=c++17 -Wall -Wextra -O2 -g -o nanodb ^
    logger.cpp data_types.cpp schema.cpp hash_map.cpp ^
    lru_cache.cpp pager.cpp avl_tree.cpp graph.cpp ^
    parser.cpp executor.cpp stack.cpp queue.cpp tpch_loader.cpp main.cpp

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Compilation failed. See errors above.
    pause
    exit /b 1
)

echo.
echo ============================================
echo   Compilation successful! Running NanoDB...
echo ============================================
echo.

nanodb.exe

echo.
echo ============================================
echo   NanoDB finished.
echo ============================================
pause
