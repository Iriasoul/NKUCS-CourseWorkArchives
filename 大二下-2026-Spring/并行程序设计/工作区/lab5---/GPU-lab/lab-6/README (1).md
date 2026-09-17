# K-Means Clustering

## Description

Implement a program that performs **k-means clustering** for 2D points.
Given arrays of x and y coordinates for data points, initial centroids, and other parameters, iteratively:

1. Assign each point to its **nearest centroid** (Euclidean distance).
2. Recompute each centroid as the **mean** of all points assigned to it.

After `max_iterations` iterations (or earlier convergence, if you implement it), the **final centroids** and **labels** should be stored in the output arrays.

Let:

* ( (x_i, y_i) ) be the i-th data point.
* ( (\mu_{j,x}, \mu_{j,y}) ) be the j-th centroid.
* ( S_j ) be the set of points assigned to centroid j.

Assignment step:

$$
\text{label}*i
= \arg\min*{0 \le j < k} \bigl( (x_i - \mu_{j,x})^2 + (y_i - \mu_{j,y})^2 \bigr)
$$

Update step:

$$
\mu_{j,x} = \frac{1}{|S_j|} \sum_{i \in S_j} x_i,
\quad
\mu_{j,y} = \frac{1}{|S_j|} \sum_{i \in S_j} y_i
$$

Inputs:

* **data_x, data_y**: Coordinates of the 2D data points, stored as 1D FP32 arrays.
* **initial_centroid_x, initial_centroid_y**: Initial centroid positions, stored as 1D FP32 arrays.
* **sample_size**: Number of data points.
* **k**: Number of clusters.
* **max_iterations**: Maximum number of k-means iterations.

Outputs:

* **centroid_x, centroid_y**: Final centroid positions after iteration.
* **labels**: Cluster index (0 to k-1) assigned to each point.

All arrays are flattened 1D arrays.

* All values are FP32 except `labels`, which is int32

---

## Input Description

You will be given the parameters and arrays in the following order.

Input format:

```bash
sample_size k max_iterations
data_x[0] data_x[1] ... data_x[sample_size-1]
data_y[0] data_y[1] ... data_y[sample_size-1]
initial_centroid_x[0] ... initial_centroid_x[k-1]
initial_centroid_y[0] ... initial_centroid_y[k-1]
```

Constraints:

* 1 ≤ sample_size ≤ 1000000
* 1 ≤ k ≤ 1000
* All arrays are float32 except `labels`, which is int32

---

## Output Description

Output the **final centroids** and **labels**.

Output format:

```bash
centroid_x[0] ... centroid_x[k-1]
centroid_y[0] ... centroid_y[k-1]
labels[0] ... labels[sample_size-1]
```

`labels[i]` is the cluster index (0-based) assigned to point i.

---

## Example

### Input

```
4 2 10
1.0 2.0 8.0 9.0
1.0 2.0 8.0 9.0
1.0 8.0
1.0 8.0
```

### Output

```
1.5 8.5
1.5 8.5
0 0 1 1
```

---

## How to Run

### 1. Build the Program

```bash
cd hard/kmeans
make
```

Or build from the top-level hard directory:

```bash
cd hard
make kmeans
```

To compile for a specific GPU architecture:

```bash
make GPU_ARCH=gfx90a    # AMD MI210
make GPU_ARCH=gfx908    # AMD MI100
make GPU_ARCH=gfx1100   # AMD Radeon W7900
```

This builds the `main` executable from the combined `main.hip` source.

**Optional: Build legacy versions**

```bash
make all_versions   # Builds main, exe_main, and exe_fs_main
```

---

### 2. Generate Test Cases (Optional)

```bash
python3 geninput.py
```

---

### 3. Run the Program

The combined `main` executable supports both stdin and file input.

**Option A: Interactive input**

```bash
./main
```

**Option B: File input**

```bash
./main testcases/1.in
```

**Option C: Pipe input**

```bash
echo "4 2 10
1.0 2.0 8.0 9.0
1.0 2.0 8.0 9.0
1.0 8.0
1.0 8.0" | ./main
```

Or redirect from file:

```bash
./main < testcases/1.in
```

---

### 4. Clean Up

```bash
make clean
```

---
