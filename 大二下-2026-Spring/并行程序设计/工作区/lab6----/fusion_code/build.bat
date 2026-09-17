@echo off
REM Final fusion build: single nvcc command compiles main.cu + all headers.
REM OpenMP via -Xcompiler /openmp (works under nvcc host), AVX2 via /arch:AVX2.
call "C:\Program Files\Microsoft Visual Studio\2022\Preview\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
nvcc -O2 -Xcompiler /openmp -Xcompiler /arch:AVX2 -Xcompiler /utf-8 -lcublas -o main.exe main.cu
if %errorlevel%==0 (echo [build ok] main.exe) else (echo [build FAILED])
