#include<iostream>
#include <sys/time.h>  
#include <bits/stdc++.h>  
#include <unistd.h>
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
	init(N);
    struct timeval start,end;
		 //cache优化算法
		gettimeofday(&start,NULL);

		 for (int j = 0; j < N; j++)
			for (int i = 0; i < N; i++)
			cache_sum[i] += a[j] * b[j][i];
			
        gettimeofday(&end,NULL);
		 cout << "cache算法:" <<fixed<<setprecision(3)<<(end.tv_sec - start.tv_sec)*1000.0+(end.tv_usec- start.tv_usec)/1000.0<< "ms" << endl;
		 return 0;
}
