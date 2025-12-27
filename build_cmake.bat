@echo off
REM Build script sử dụng CMake với MinGW-w64
REM Usage:
REM   build_cmake.bat        - Build project (static library - default)
REM   build_cmake.bat dll    - Build as DLL (dynamic library)
REM   build_cmake.bat static - Build as static library
REM   build_cmake.bat clean  - Clean build directory

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

REM Xác định build type
set BUILD_TYPE=STATIC
if "%1"=="dll" (
    set BUILD_TYPE=DLL
    echo Building as DLL (Dynamic Link Library)...
) else if "%1"=="static" (
    set BUILD_TYPE=STATIC
    echo Building as Static Library...
) else (
    echo Building as Static Library (default)...
    echo Use "build_cmake.bat dll" to build as DLL
)
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
if "%BUILD_TYPE%"=="DLL" (
    cmake .. -G "MinGW Makefiles" -DBUILD_SHARED_LIBS=ON
) else (
    cmake .. -G "MinGW Makefiles" -DBUILD_SHARED_LIBS=OFF
)

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
if "%BUILD_TYPE%"=="DLL" (
    echo Build type: DLL (Dynamic Link Library)
    echo.
    echo IMPORTANT: When distributing your application:
    echo   - Copy both .exe and .dll files together
    echo   - DLL files are in: build\bin\
) else (
    echo Build type: Static Library
    echo Note: Executable is standalone (no DLL needed)
)
echo.
echo Executable locations:
echo   - build\bin\example_curl_dll_direct.exe (mini_curl DLL example)
echo   - build\bin\example_full_app.exe (full app example)
if "%BUILD_TYPE%"=="DLL" (
    echo   - build\bin\mini_curl.dll (mini_curl DLL)
    echo   - build\bin\mini_http_server.dll (mini_http_server DLL)
)
echo.
echo Run examples:
echo   build\bin\example_curl_dll_direct.exe
echo   build\bin\example_full_app.exe
echo.
echo To clean build: build_cmake.bat clean
echo To build DLL: build_cmake.bat dll
echo To build static: build_cmake.bat static
echo ========================================
echo.
pause

