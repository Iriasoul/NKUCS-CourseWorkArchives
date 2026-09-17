#include <iostream>
#include <chrono>
#include <cstdlib>
#include <cstring>

using namespace std;

// 平凡算法：逐个累加
int A(const int *a, int n)
{
    int sum = 0;
    for (int i = 0; i < n; i++)
    {
        sum += a[i];
    }
    return sum;
}

// 优化算法1：两路链式累加
int B(const int *a, int n)
{
    int sum1 = 0, sum2 = 0;
    for (int i = 0; i < n; i += 2)
    {
        sum1 += a[i];
        sum2 += a[i + 1];
    }
    return sum1 + sum2;
}

// 优化算法2：非递归的两两相加
void C(int a[], int n)
{
    for (int m = n; m > 1; m /= 2)
    {
        for (int i = 0; i < m / 2; i++)
        {
            a[i] = a[2 * i] + a[2 * i + 1];
        }
    }
}

int main()
{
    const int n = 1 << 30;
    int *a = new int[n];
    // 初始化数组全为1
    for (int i = 0; i < n; i++)
    {
        a[i] = 1;
    }

    auto start = chrono::high_resolution_clock::now();
    int sum1 = A(a, n);
    auto end = chrono::high_resolution_clock::now();
    cout << "A: " << sum1 << " Time: " << chrono::duration_cast<chrono::microseconds>(end - start).count() << " us" << endl;

    start = chrono::high_resolution_clock::now();
    int sum2 = B(a, n);
    end = chrono::high_resolution_clock::now();
    cout << "B: " << sum2 << " Time: " << chrono::duration_cast<chrono::microseconds>(end - start).count() << " us" << endl;

    int *a_copy = new int[n];
    memcpy(a_copy, a, n * sizeof(int));
    start = chrono::high_resolution_clock::now();
    C(a_copy, n);
    int sum3 = a_copy[0];
    end = chrono::high_resolution_clock::now();
    cout << "C: " << sum3 << " Time: " << chrono::duration_cast<chrono::microseconds>(end - start).count() << " us" << endl;

    delete[] a;
    delete[] a_copy;
    return 0;
}