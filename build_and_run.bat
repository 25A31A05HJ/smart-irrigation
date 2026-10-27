@echo off
echo Building...
g++ -std=c++17 -O2 -static server.cpp -o irrigation_server.exe -lws2_32
if errorlevel 1 (
  echo BUILD FAILED
  pause
  exit /b 1
)
echo Build OK. Starting server...
irrigation_server.exe
pause
