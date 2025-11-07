@echo off
REM SPINDEX One-Click Launcher for Windows
REM Double-click this file to start SPINDEX

title SPINDEX Launcher

echo.
echo ╔══════════════════════════════════════════════════════════════╗
echo ║                                                              ║
echo ║                 🚀 SPINDEX LAUNCHER 🚀                       ║
echo ║                                                              ║
echo ║         One-Click Startup for Full-Stack System             ║
echo ║                                                              ║
echo ╚══════════════════════════════════════════════════════════════╝
echo.

REM Check if Python is installed
python --version >nul 2>&1
if errorlevel 1 (
    echo ❌ Python is not installed or not in PATH
    echo.
    echo Please install Python 3.8 or higher from:
    echo https://www.python.org/downloads/
    echo.
    echo Make sure to check "Add Python to PATH" during installation
    pause
    exit /b 1
)

echo ✅ Python found
echo.

REM Check if Node.js is installed
node --version >nul 2>&1
if errorlevel 1 (
    echo ❌ Node.js is not installed or not in PATH
    echo.
    echo Please install Node.js 16 or higher from:
    echo https://nodejs.org/
    pause
    exit /b 1
)

echo ✅ Node.js found
echo.

REM Run the Python launcher
echo Starting SPINDEX...
echo.
python start_spindex.py

if errorlevel 1 (
    echo.
    echo ❌ SPINDEX failed to start
    pause
    exit /b 1
)

pause
