CXX      := g++
CXXFLAGS := -O2 -std=c++17 -fopenmp -Wall -Wextra

# 所有头文件
HEADERS := instance.h brute_force.h held_karp.h held_karp_pruned.h \
           held_karp_parallel.h held_karp_approx.h

.PHONY: all clean
all: gen bench

gen: gen.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) gen.cpp -o gen

bench: bench.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) bench.cpp -o bench

clean:
	rm -f gen bench
