#include<iostream>
#include <windows.h>
#include <stdlib.h>
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

float ord() {
    unsigned long long sum = 0;
    long long start, end;

    QueryPerformanceCounter((LARGE_INTEGER*)&start);

    for (int i = 0; i < N; i++)
        sum += a[i];

    QueryPerformanceCounter((LARGE_INTEGER*)&end);
    return (end - start) * 1000.0 / freq;
}

int main() {
    int count = 262144;//2^25
    for (int n = 10; n <= 27; n++) {
        init();
        float average_time = 0.0;
        for (int i = 0; i < count; i++)
            average_time += ord();
        cout << "n=" << n << fixed << endl
            << average_time / (float)count << "ms" << endl;
       if(count>=1024) count /= 2;
        N *= 2;
    }
    return 0;
}