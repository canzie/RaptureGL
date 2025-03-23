@echo off
echo Building LiDAR Game in Debug mode...

:: Check if Tracy repo exists
if not exist "Engine\vendor\tracy" (
    echo Tracy repository not found, cloning it...
    git clone https://github.com/wolfpld/tracy.git Engine\vendor\tracy
)

:: Create build directory if it doesn't exist
if not exist "build\debug" mkdir "build\debug"

:: Navigate to build directory
cd "build\debug"

:: Configure CMake for Debug build with explicit flags
echo Configuring CMake for Debug build...
cmake -DCMAKE_BUILD_TYPE=Debug -DRAPTURE_DEBUG=ON ../..

:: Build the project explicitly in Debug config
echo Building project in Debug configuration...
cmake --build . --config Debug

:: Return to original directory
cd ..\..

echo.
echo Build completed in Debug mode. Press any key to exit.
pause > nul 