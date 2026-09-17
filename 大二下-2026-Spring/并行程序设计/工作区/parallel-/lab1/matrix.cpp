// #include <iostream>
// #include <chrono>

// using namespace std;

// int main()
// {
//     const int n = 10000;

//     // 初始化矩阵和向量
//     int **matrix = new int *[n];
//     for (int i = 0; i < n; ++i)
//     {
//         matrix[i] = new int[n];
//     }
//     int *vec = new int[n];
//     int *result = new int[n];

//     for (int i = 0; i < n; ++i)
//     {
//         vec[i] = i; // 向量元素为 i
//         result[i] = 0;
//         for (int j = 0; j < n; ++j)
//         {
//             matrix[i][j] = i + j; // 矩阵元素为 i+j
//         }
//     }

//     auto start = chrono::high_resolution_clock::now();

//     // 平凡算法：逐列访问（外层循环为列，内层循环为行）
//     // for (int col = 0; col < n; col++)
//     // {
//     //     result[col] = 0;
//     //     for (int row = 0; row < n; row++)
//     //     {
//     //         result[col] += matrix[row][col] * vec[row];
//     //     }
//     // }

//     // Cache 优化算法，改为逐行访问矩阵元素：一步外层循环计算不出任何一个内积，只是向每个内积累加一个乘法结果
//     for (int col = 0; col < n; ++col)
//     {
//         result[col] = 0;
//     }

//     for (int row = 0; row < n; ++row)
//     {
//         for (int col = 0; col < n; ++col)
//         {
//             result[col] += matrix[row][col] * vec[row];
//         }
//     }

//     auto end = chrono::high_resolution_clock::now();
//     auto duration = chrono::duration_cast<chrono::duration<double>>(end - start);

//     cout << "Time: " << duration.count() << " seconds" << endl;

//     for (int i = 0; i < n; ++i)
//     {
//         delete[] matrix[i];
//     }
//     delete[] matrix;
//     delete[] vec;
//     delete[] result;

//     return 0;
// }

#include <iostream>
#include <chrono>

using namespace std;

int main()
{
    const int n = 10000;
    const int num = 10; // 运行次数

    int **matrix = new int *[n];
    for (int i = 0; i < n; ++i)
    {
        matrix[i] = new int[n];
    }
    int *vec = new int[n];
    int *result = new int[n];

    for (int i = 0; i < n; ++i)
    {
        vec[i] = i; // 向量元素为 i
        for (int j = 0; j < n; ++j)
        {
            matrix[i][j] = i + j; // 矩阵元素为 i+j
        }
    }

    double totalDuration = 0.0;

    // 多次运行计算并记录总时间
    for (int run = 0; run < num; ++run)
    {
        // 每次运行前重置 result 数组为 0
        for (int col = 0; col < n; ++col)
        {
            result[col] = 0;
        }

        auto start = chrono::high_resolution_clock::now();

        // 平凡算法：逐列访问（外层循环为列，内层循环为行）
        for (int col = 0; col < n; col++)
        {
            result[col] = 0;
            for (int row = 0; row < n; row++)
            {
                result[col] += matrix[row][col] * vec[row];
            }
        }

        // Cache 优化算法，改为逐行访问矩阵元素：一步外层循环计算不出任何一个内积，只是向每个内积累加一个乘法结果
        // for (int col = 0; col < n; ++col)
        // {
        //     result[col] = 0;
        // }

        // for (int row = 0; row < n; ++row)
        // {
        //     for (int col = 0; col < n; ++col)
        //     {
        //         result[col] += matrix[row][col] * vec[row];
        //     }
        // }

        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::duration<double>>(end - start);
        totalDuration += duration.count();
    }

    double averageDuration = totalDuration / num;

    cout << averageDuration << " s" << endl;

    for (int i = 0; i < n; ++i)
    {
        delete[] matrix[i];
    }
    delete[] matrix;
    delete[] vec;
    delete[] result;

    return 0;
}

