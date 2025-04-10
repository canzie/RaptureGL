@echo off
echo Building LiDAR Game in Release mode with optimizations...

:: Check if Tracy repo exists (Tracy can still be included in Release builds if needed)
if not exist "Engine\vendor\tracy" (
    echo Tracy repository not found, cloning it...
    git clone https://github.com/wolfpld/tracy.git Engine\vendor\tracy
)

:: Create build directory if it doesn't exist
if not exist "build\release" mkdir "build\release"

:: Navigate to build directory
cd "build\release"

:: Configure CMake for Release build with optimizations
echo Configuring CMake for Release build...
cmake -DCMAKE_BUILD_TYPE=Release -DRAPTURE_DEBUG=OFF ../..

:: Build the project explicitly in Release config
echo Building project in Release configuration...
cmake --build . --config Release

:: Return to original directory
cd ..\..

echo.
echo Build completed in Release mode. Press any key to exit.
pause > nul 