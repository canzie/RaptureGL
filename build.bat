@echo off
echo LiDAR Game Build System

echo Choose build configuration:
echo 1. Debug
echo 2. Release
echo.
choice /c 12 /n /m "Enter your choice (1-2): "

if errorlevel 2 goto RELEASE
if errorlevel 1 goto DEBUG

:DEBUG
echo.
echo Building in Debug mode...
call build_debug.bat
goto END

:RELEASE
echo.
echo Building in Release mode with optimizations...
call build_release.bat
goto END

:END
exit /b 0 