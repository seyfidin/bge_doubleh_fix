@echo off
setlocal
title Build BGE Double H Fix (native C++)

where cl >nul 2>nul
if errorlevel 1 (
    echo.
    echo MSVC compiler "cl.exe" was not found.
    echo.
    echo Run this BAT from:
    echo   "x64 Native Tools Command Prompt for VS 2022"
    echo.
    echo Visual Studio Build Tools with "Desktop development with C++" is sufficient.
    echo.
    pause
    exit /b 1
)

if not exist build mkdir build

echo Building native x64 Windows EXE...
cl /nologo /std:c++17 /O2 /EHsc /W4 /MT /DUNICODE /D_UNICODE ^
   main.cpp ^
   /Fe:build\BGE_DoubleH_Fix.exe ^
   /link /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF Comdlg32.lib Shell32.lib

if errorlevel 1 (
    echo.
    echo Build failed.
    pause
    exit /b 1
)

echo.
echo Finished:
echo   build\BGE_DoubleH_Fix.exe
echo.
for %%F in (build\BGE_DoubleH_Fix.exe) do echo Size: %%~zF bytes
echo.
pause
