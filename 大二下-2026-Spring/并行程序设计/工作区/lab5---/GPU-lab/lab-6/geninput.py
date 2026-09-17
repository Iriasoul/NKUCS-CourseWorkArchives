import os
import random

# create testcases directory
os.makedirs("testcases", exist_ok=True)

def generate_case(path,
                  sample_size,
                  k,
                  max_iterations,
                  seed):
    random.seed(seed)
    with open(path, "w") as f:
        # write basic parameters
        f.write(f"{sample_size} {k} {max_iterations}\n")

        # data_x
        for _ in range(sample_size):
            f.write(f"{random.uniform(-1000.0, 1000.0):.6f}\n")

        # data_y
        for _ in range(sample_size):
            f.write(f"{random.uniform(-1000.0, 1000.0):.6f}\n")

        # initial_centroid_x
        for _ in range(k):
            f.write(f"{random.uniform(-1000.0, 1000.0):.6f}\n")

        # initial_centroid_y
        for _ in range(k):
            f.write(f"{random.uniform(-1000.0, 1000.0):.6f}\n")


# ------------------------------------------------------
# Normal random testcases
# ------------------------------------------------------
for i, seed in enumerate([42, 123], start=1):

    # random valid sizes
    sample_size = random.randint(10, 200)
    k = random.randint(2, min(10, sample_size))
    max_iterations = random.randint(5, 50)

    generate_case(f"testcases/{i}.in",
                  sample_size,
                  k,
                  max_iterations,
                  seed)


# ------------------------------------------------------
# Edge Case 1: minimal valid dimensions
# ------------------------------------------------------
generate_case("testcases/3.in",
              sample_size=1,
              k=1,
              max_iterations=1,
              seed=9999)


# ------------------------------------------------------
# Edge Case 2: large stress dimensions
# (WARNING: This file may be large)
# ------------------------------------------------------
generate_case("testcases/4.in",
              sample_size=50000,    # large sample count
              k=50,                 # many clusters
              max_iterations=100,
              seed=2025)

print("✅ Generated 4 K-Means Clustering testcases in ./testcases/")
