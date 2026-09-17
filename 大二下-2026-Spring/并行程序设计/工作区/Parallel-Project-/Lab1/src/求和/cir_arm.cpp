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

float cir() {
    struct timeval start,end;

    gettimeofday(&start,NULL);

    for (int m = N; m > 1; m /= 2)
        for (int i = 0; i < m / 2; i++)
            a[i] = a[2 * i] + a[2 * i + 1];

    gettimeofday(&end,NULL);

    float interval=((end.tv_sec - start.tv_sec)*1000.0+(end.tv_usec- start.tv_usec)/1000.0);
    return interval;
}

int main() {
    int count = 8192;//2^25
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

