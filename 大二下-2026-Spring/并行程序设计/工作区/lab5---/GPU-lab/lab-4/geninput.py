import argparse
import os
import random

def create_testcases_dir():
    if not os.path.exists("testcases"):
        os.makedirs("testcases")

def _stream_random_array_to_file(filename, n, low, high, chunk_size=1_000_000):
    # Write numbers in chunks to keep memory usage bounded on small servers.
    with open(filename, "w") as f:
        f.write(f"{n}\n")
        remaining = n
        first = True
        while remaining > 0:
            current = min(chunk_size, remaining)
            chunk = [str(random.randint(low, high)) for _ in range(current)]
            if first:
                f.write(" ".join(chunk))
                first = False
            else:
                f.write(" " + " ".join(chunk))
            remaining -= current
        f.write("\n")

def generate_testcase_1():
    N = 10
    _stream_random_array_to_file("testcases/1.in", N, -10, 10, chunk_size=10)

def generate_testcase_2():
    N = 1000000
    _stream_random_array_to_file("testcases/2.in", N, -100, -50)

def generate_testcase_3():
    N = 100000000
    _stream_random_array_to_file("testcases/3.in", N, -1000, -500)

def generate_testcase_4():
    N = 1000000000
    _stream_random_array_to_file("testcases/4.in", N, -10000, 10000)


def parse_args():
    parser = argparse.ArgumentParser(description="Generate reduction lab testcases")
    parser.add_argument(
        "--quick",
        action="store_true",
        help="Generate only small/medium tests (1.in, 2.in)",
    )
    parser.add_argument(
        "--include-test4",
        action="store_true",
        help="Also generate 4.in (1e9 values, very large and slow)",
    )
    parser.add_argument(
        "--chunk-size",
        type=int,
        default=1_000_000,
        help="Numbers written per chunk to control memory usage",
    )
    return parser.parse_args()

def main():
    args = parse_args()

    if args.chunk_size <= 0:
        raise ValueError("--chunk-size must be a positive integer")

    random.seed(77777)

    create_testcases_dir()

    # Rebind stream helper with configured chunk size for all generators.
    global _stream_random_array_to_file
    original_stream = _stream_random_array_to_file

    def _stream_with_config(filename, n, low, high, chunk_size=args.chunk_size):
        return original_stream(filename, n, low, high, chunk_size=chunk_size)

    _stream_random_array_to_file = _stream_with_config

    generate_testcase_1()
    generate_testcase_2()

    if not args.quick:
        generate_testcase_3()

    if args.include_test4:
        generate_testcase_4()

    for i in range(1, 5):
        filename = f"testcases/{i}.in"
        if os.path.exists(filename):
            file_size = os.path.getsize(filename)
            print(f"  {filename}: {file_size:,} bytes")

if __name__ == "__main__":
    main()