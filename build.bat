@echo off
REM Compilation : g++ (MSYS2 mingw64) + GLFW + GLEW + GLM + WinMM (audio)
set PATH=C:\msys64\mingw64\bin;%PATH%
g++ -O2 -std=c++17 -Iaudio main.cpp -o cave.exe -lglew32 -lglfw3 -lopengl32 -lgdi32 -lwinmm
if %ERRORLEVEL% equ 0 (echo [OK] cave.exe) else (echo [ECHEC])
