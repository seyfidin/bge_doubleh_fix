@echo off
setlocal EnableExtensions
title Build BGE Double H Fix - MinGW-w64

set "GPP="

rem 1) Try PATH first.
for %%G in (g++.exe) do (
    if not "%%~$PATH:G"=="" set "GPP=%%~$PATH:G"
)

rem 2) Common WinLibs / MSYS2 locations.
if not defined GPP if exist "C:\winlibs\mingw64\bin\g++.exe" set "GPP=C:\winlibs\mingw64\bin\g++.exe"
if not defined GPP if exist "C:\WinLibs\mingw64\bin\g++.exe" set "GPP=C:\WinLibs\mingw64\bin\g++.exe"
if not defined GPP if exist "C:\msys64\ucrt64\bin\g++.exe" set "GPP=C:\msys64\ucrt64\bin\g++.exe"
if not defined GPP if exist "C:\msys64\mingw64\bin\g++.exe" set "GPP=C:\msys64\mingw64\bin\g++.exe"

if not defined GPP (
    echo.
    echo MinGW-w64 g++.exe was not found.
    echo.
    echo Recommended: download a WinLibs x86_64 UCRT build and either:
    echo   - add mingw64\bin to PATH, or
    echo   - extract it to C:\winlibs
    echo.
    echo Then run this BAT again.
    echo.
    pause
    exit /b 1
)

echo Compiler:
echo   "%GPP%"
echo.

if not exist build mkdir build

echo Building native x64 Windows EXE...
"%GPP%" ^
    -std=c++17 ^
    -O2 ^
    -s ^
    -static ^
    -static-libgcc ^
    -static-libstdc++ ^
    -municode ^
    -mwindows ^
    -DUNICODE ^
    -D_UNICODE ^
    main.cpp ^
    -o build\BGE_DoubleH_Fix.exe ^
    -lcomdlg32 ^
    -lshell32

if errorlevel 1 (
    echo.
    echo Build failed.
    pause
    exit /b 1
)

echo.
echo Build successful:
echo   build\BGE_DoubleH_Fix.exe
echo.
for %%F in (build\BGE_DoubleH_Fix.exe) do echo Size: %%~zF bytes
echo.
echo The generated EXE does not require Python, .NET, or MinGW on the target PC.
echo.
pause
