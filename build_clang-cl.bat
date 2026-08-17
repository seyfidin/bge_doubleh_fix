@echo off
setlocal
title Build BGE Double H Fix with clang-cl

where clang-cl >nul 2>nul
if errorlevel 1 (
    echo clang-cl.exe was not found.
    pause
    exit /b 1
)

if not exist build mkdir build

clang-cl /nologo /std:c++17 /O2 /EHsc /MT /DUNICODE /D_UNICODE ^
    main.cpp ^
    /Fe:build\BGE_DoubleH_Fix.exe ^
    /link /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF Comdlg32.lib Shell32.lib

if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)

echo Finished: build\BGE_DoubleH_Fix.exe
pause
