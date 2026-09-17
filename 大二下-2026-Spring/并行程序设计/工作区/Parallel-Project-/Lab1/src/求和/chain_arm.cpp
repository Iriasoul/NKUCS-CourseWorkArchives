#include<iostream>
#include <sys/time.h>   
#include <unistd.h>
using namespace std;
unsigned long long N = 1024;
unsigned long long *a;
void init() {
    a=new unsigned long long[N];
    for (int i = 0; i < N; i++)
        a[i] = i + 1;
}

float chain() {
    unsigned long long sum = 0, sum1 = 0, sum2 = 0;
    struct timeval start,end;

    gettimeofday(&start,NULL);
    for (unsigned long long i = 0; i < N; i += 2) {
        sum1 += a[i];
        sum2 += a[i + 1];
    }
    sum = sum1 + sum2;
    gettimeofday(&end,NULL);

    float interval=((end.tv_sec - start.tv_sec)*1000.0+(end.tv_usec- start.tv_usec)/1000.0);
    return interval;
}

int main() {
    int count = 8192;//2^25
    for (int n =10 ; n <=27; n++) {
    init();
    float average_time = 0.0;
    for (int i = 0; i < count; i++)
        average_time += chain();
    cout << "n="<<n << fixed<<endl
        << average_time /(float)count << "ms" << endl;
    if(count>1)count/=2;
    N*=2;
    }
    return 0;

}