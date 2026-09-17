#include<iostream>
#include <sys/time.h>   
#include <unistd.h>
#include <bits/stdc++.h>  
using namespace std;
unsigned long long N = 1024;
unsigned long long *a;
void init() {
    a=new unsigned long long[N];
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

double recursion() {
    struct timeval start,end;

    gettimeofday(&start,NULL);
    recursion(N);
    gettimeofday(&end,NULL);
    init();
    float interval=((end.tv_sec - start.tv_sec)*1000.0+(end.tv_usec- start.tv_usec)/1000.0);
    return interval;
}

int main() {
    int count = 256;
    for (int n = 10; n <= 27; n++) {
        init();
        double average_time = 0.0;
        for (int i = 0; i < count; i++)
            average_time += recursion();
        cout << "n=" << n  << endl
            << average_time / count << "ms" << endl;

        if(count>=2)count /= 2;
        N *= 2;
        delete a;
    }
    return 0;
}