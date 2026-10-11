@echo off
REM ===========================================================================
REM  build.bat - compile GRIMDEEP avec g++ (MSYS2 / MinGW-w64)
REM
REM  Prerequis (une fois, dans un terminal MSYS2 MINGW64) :
REM     pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-glfw mingw-w64-x86_64-glew mingw-w64-x86_64-glm
REM
REM  Resultat : build\grimdeep.exe   (lancer ensuite run.bat)
REM ===========================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"
set PATH=C:\msys64\mingw64\bin;%PATH%

if not exist build mkdir build

REM liste de tous les fichiers source du dossier src (sous-dossiers compris)
set SOURCES=
for /r src %%f in (*.cpp) do set SOURCES=!SOURCES! "%%f"

echo Compilation de GRIMDEEP...
g++ -O2 -std=c++17 -Wall -Wextra -Isrc !SOURCES! -o build\grimdeep.exe -lglew32 -lglfw3 -lopengl32 -lgdi32 -lwinmm
if %ERRORLEVEL% equ 0 (
    echo [OK] build\grimdeep.exe
) else (
    echo [ECHEC] voir les erreurs ci-dessus
    exit /b 1
)
