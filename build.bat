@echo off
chcp 65001 >nul 2>&amp;1
echo === MIDI Mapper Build Script ===

:: Try g++ in PATH first
where g++ >nul 2>&amp;1
if %errorlevel%==0 (
    echo Found g++ in PATH
    g++ -O2 -std=c++17 -o midi_mapper.exe midi_mapper.cpp -lwinmm -static
    if %errorlevel%==0 (
        echo Build successful: midi_mapper.exe
    ) else (
        echo Build failed!
    )
    goto :end
)

:: Try MSYS2 MinGW
set MSYS2_GCC=C:\msys64\ucrt64\bin\g++.exe
if exist "%MSYS2_GCC%" (
    echo Found MSYS2 MinGW g++
    "%MSYS2_GCC%" -O2 -std=c++17 -o midi_mapper.exe midi_mapper.cpp -lwinmm -static
    if %errorlevel%==0 (
        echo Build successful: midi_mapper.exe
    ) else (
        echo Build failed!
    )
    goto :end
)

:: Try mingw32 path
set MSYS2_GCC2=C:\msys64\mingw64\bin\g++.exe
if exist "%MSYS2_GCC2%" (
    echo Found MSYS2 mingw64 g++
    "%MSYS2_GCC2%" -O2 -std=c++17 -o midi_mapper.exe midi_mapper.cpp -lwinmm -static
    if %errorlevel%==0 (
        echo Build successful: midi_mapper.exe
    ) else (
        echo Build failed!
    )
    goto :end
)

echo ERROR: No C++ compiler found!
echo Please install MinGW via MSYS2:
echo   1. Run: C:\msys64\ucrt64.exe
echo   2. Execute: pacman -S mingw-w64-ucrt-x86_64-gcc
echo   3. Re-run this script
:end
pause
