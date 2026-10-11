@echo off
REM ===========================================================================
REM  run.bat - lance GRIMDEEP depuis la racine du projet (qui contient assets\)
REM  Les options sont transmises au jeu, ex. :  run.bat --windowed --seed 42
REM ===========================================================================
cd /d "%~dp0"
set PATH=C:\msys64\mingw64\bin;%PATH%
if not exist build\grimdeep.exe (
    echo build\grimdeep.exe introuvable : lancez d'abord build.bat
    exit /b 1
)
build\grimdeep.exe %*
