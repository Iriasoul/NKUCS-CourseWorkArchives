# lab4 MPI 编译
# 用法:
#   make            编译出 ./main
#   make clean      清理
# 切换策略: 改 main.cc 顶部的 `strategy` 常量后重新 make
# 改进程数: 改 qsub_mpi.sh 的 -np 与 #PBS -l nodes=..:ppn=..

CXX      = mpicxx
CXXFLAGS = -O3 -std=c++17 -fopenmp -march=armv8-a -I.
LDFLAGS  = -fopenmp -pthread

main: main.cc
	$(CXX) $(CXXFLAGS) -o main main.cc $(LDFLAGS)

clean:
	rm -f main

.PHONY: clean
