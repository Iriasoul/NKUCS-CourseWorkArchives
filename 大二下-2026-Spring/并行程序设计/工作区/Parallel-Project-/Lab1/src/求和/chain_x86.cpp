#include<iostream>
#include <windows.h>
#include <stdlib.h>
using namespace std;
unsigned long long N = 1024;
unsigned long long *a;
long long freq;
void init() {
    a=new unsigned long long[N];
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
    for (int i = 0; i < N; i++)
        a[i] = i + 1;
}

float chain() {
    unsigned long long sum = 0, sum1 = 0, sum2 = 0;
    long long start, end;
    QueryPerformanceCounter((LARGE_INTEGER*)&start);

    for (unsigned long long i = 0; i < N; i += 2) {
        sum1 += a[i];
        sum2 += a[i + 1];
    }
    sum = sum1 + sum2;
    QueryPerformanceCounter((LARGE_INTEGER*)&end);
    float interval=(end - start) * 1000.0 / freq;
    return interval;
}

int main() {
    int count = 8388608;//2^25
    for (int n =10 ; n <=27; n++) {
    init();
    float average_time = 0.0;
    for (int i = 0; i < count; i++)
        average_time += chain();
    cout << "n="<<n << fixed<<endl
        << average_time /(float)count << "ms" << endl;
    count/=2;
    N*=2;
    }
    return 0;
}