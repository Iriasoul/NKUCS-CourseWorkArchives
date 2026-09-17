#include<iostream>
#include <windows.h>
#include <stdlib.h>
#include<iomanip>
using namespace std;
unsigned long long N = 1024;
unsigned long long* a;
long long freq;
void init() {
    a = new unsigned long long[N];
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
    for (int i = 0; i < N; i++)
        a[i] = i + 1;
}
void recursion(unsigned long long n) {
    if (n == 1) {
        return;
    }
    else {
        for (int i = 0; i < n / 2; i++)
            a[i] += a[n - i - 1];
        n /= 2;
        recursion(n);
        return;
    }
}

float recursion() {
    long long start, end;
    QueryPerformanceCounter((LARGE_INTEGER*)&start);

    recursion(N);

    QueryPerformanceCounter((LARGE_INTEGER*)&end);

    init();
    return (end - start) * 1000.0 / freq;
}

int main() {
    int count = 65536;//2^25
    for (int n = 10; n <= 27; n++) {
        init();
        float average_time = 0.0;
        for (int i = 0; i < count; i++)
            average_time += recursion();
        cout << "n=" << n << setprecision(4) << endl
            << average_time / (float)count << "ms" << endl;

        if(count>=2)count /= 2;
        N *= 2;
    }
    return 0;
}