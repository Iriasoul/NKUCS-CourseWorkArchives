#include<iostream>
#include <windows.h>
#include <stdlib.h>
using namespace std;
unsigned long long N = 1024;
byte* a;
long long freq;
void init() {
    a = new byte[N];
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
    for (int i = 0; i < N; i++)
        a[i] = i + 1;
}

float cir() {
    long long start, end;

    QueryPerformanceCounter((LARGE_INTEGER*)&start);

    for (int m = N; m > 1; m /= 2)
        for (int i = 0; i < m / 2; i++)
            a[i] = a[2 * i] + a[2 * i + 1];

    QueryPerformanceCounter((LARGE_INTEGER*)&end);

    init();
    return (end - start) * 1000.0 / freq;
}

int main() {
    int count = 1024;//2^25
    for (int n = 10; n <= 27; n++) {
        init();
        float average_time = 0.0;
        for (int i = 0; i < count; i++)
            average_time += cir();
        cout << "n=" << n << fixed << endl
            << average_time / (float)count << "ms" << endl;
        N *= 2;
        delete a;
    }
    return 0;
}