@echo off
REM Build script sử dụng CMake với MinGW-w64
REM Usage:
REM   build_cmake.bat        - Build project
REM   build_cmake.bat clean   - Clean build directory

REM Kiểm tra nếu có argument "clean"
if "%1"=="clean" (
    echo Cleaning build directory...
    if exist build (
        rmdir /S /Q build
        echo Build directory cleaned.
    ) else (
        echo Build directory does not exist.
    )
    pause
    exit /b 0
)

echo Building với CMake và MinGW-w64...
echo.

REM Kiểm tra CMake
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Error: CMake not found!
    echo Please install CMake: https://cmake.org/download/
    pause
    exit /b 1
)

REM Kiểm tra MinGW-w64
where g++ >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Error: MinGW-w64 g++ not found!
    echo Please install MinGW-w64 and add it to PATH
    pause
    exit /b 1
)

echo CMake version:
cmake --version
echo.

echo MinGW-w64 g++ version:
g++ --version
echo.

REM Tạo thư mục build nếu chưa có
if not exist build mkdir build
cd build

REM Xóa cache cũ nếu có để đảm bảo dùng đúng generator
if exist CMakeCache.txt (
    echo Xoa cache CMake cu...
    del /Q CMakeCache.txt 2>nul
)

REM Configure với MinGW Makefiles
echo Configuring CMake với MinGW Makefiles...
cmake .. -G "MinGW Makefiles"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo CMake configuration failed!
    cd ..
    pause
    exit /b 1
)

echo.
echo Building project...
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Build failed!
    cd ..
    pause
    exit /b 1
)

cd ..
echo.
echo ========================================
echo Build successful!
echo.
echo Executable locations:
echo   - build\bin\example_api_simple_c.exe (mini_curl example)
echo   - build\bin\example_simple.exe (mini_http_server simple example)
echo   - build\bin\example_oidc.exe (mini_http_server OIDC example)
echo.
echo Run examples:
echo   build\bin\example_api_simple_c.exe
echo   build\bin\example_simple.exe
echo   build\bin\example_oidc.exe
echo.
echo To clean build: build_cmake.bat clean
echo ========================================
echo.
pause

