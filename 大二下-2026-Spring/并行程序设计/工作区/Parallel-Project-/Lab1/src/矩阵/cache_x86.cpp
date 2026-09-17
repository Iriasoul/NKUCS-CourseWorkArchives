#include<iostream>
#include <windows.h>
#include <stdlib.h>
 using namespace std;
const int N = 10000;
unsigned long long a[N],b[N][N], ord_sum[N],cache_sum[N];

void init(int n)
{
	for (int i = 0; i < N; i++){
		a[i]=i;
		ord_sum[i] = 0;
		cache_sum[i]=0;
		for (int j = 0; j < N; j++)
		b[i][j] = i + j;
	}
}

 int main()
 {
	 long long head, tail,freq;
	 init(N);
	 QueryPerformanceFrequency((LARGE_INTEGER*)&freq);

		 //cache优化算法
		 QueryPerformanceCounter((LARGE_INTEGER*)&head);

		 for (int j = 0; j < N; j++)
			 for (int i = 0; i < N; i++)
				 cache_sum[i] += a[j] * b[j][i];

		 QueryPerformanceCounter((LARGE_INTEGER*)&tail);
		 cout << "cache算法:" << (tail - head) * 1000.0 / freq << "ms" << endl;
		 return 0;
}