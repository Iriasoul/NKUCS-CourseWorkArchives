#include<iostream>
#include <sys/time.h>   
#include <unistd.h>
using namespace std;
unsigned long long N = 1024;
unsigned long long* a;

void init() {
    a = new unsigned long long[N];
    struct timeval start,end;
    for (int i = 0; i < N; i++)
        a[i] = i + 1;
}

float ord() {
    unsigned long long sum = 0;
    struct timeval start,end;

    gettimeofday(&start,NULL);

    for (int i = 0; i < N; i++)
        sum += a[i];

    gettimeofday(&end,NULL);

    float interval=((end.tv_sec - start.tv_sec)*1000.0+(end.tv_usec- start.tv_usec)/1000.0);
    return interval;
}

int main() {
    int count = 8192;//2^25
    for (int n = 10; n <= 27; n++) {
        init();
        double average_time = 0.0;
        for (int i = 0; i < count; i++)
            average_time += ord();
        cout << "n=" << n << fixed << endl
            << average_time /count << "ms" << endl;
       if(count>1) count /= 2;
        N *= 2;
    }
    return 0;
}