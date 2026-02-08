# Benchmark script for the Networked Key-Value Store server.
# It benchmarks the throughput and latency for a single client vs 10 concurrent clients.
# This script can be run like this: python3 benchmark.py
# The location it is ran from is only relevant if you wish to save current results
# or load previous results bench_results.json
# Saving and loading happens in the cwd.

import socket
import time
import statistics
import json
import os
from concurrent.futures import ThreadPoolExecutor



def save_baseline(data, filename="bench_results.json"):
    with open(filename, "w") as f:
        json.dump(data, f)


def load_baseline(filename="bench_results.json"):
    if os.path.exists(filename):
        with open(filename, "r") as f:
            return json.load(f)
    return None


def print_results(data, baseline=None):
    if not data: return

    # If baseline is provided, inject the "% Diff" into the data rows
    if baseline and len(baseline) == len(data):
        for i, row in enumerate(data):
            # Compare Throughput (higher is better)
            curr_tp = float(row["Throughput (req/s)"])
            base_tp = float(baseline[i]["Throughput (req/s)"])
            diff = ((curr_tp - base_tp) / base_tp) * 100
            row["% Diff"] = f"{diff:+.1f}%"

    headers = data[0].keys()
    widths = {k: max(len(k), max(len(str(row.get(k, ""))) for row in data)) for k in headers}

    # Header logic
    header_line = " | ".join(f"{k:<{widths[k]}}" for k in headers)
    sep_line = "-+-".join("-" * widths[k] for k in headers)

    print(f"\n{header_line}\n{sep_line}")
    for row in data:
        print(" | ".join(f"{str(row.get(k, '')):<{widths[k]}}" for k in headers))


def single_client_task(host, port, command, iterations):
    latencies = []
    try:
        with socket.create_connection((host, port), timeout=5) as s:
            for _ in range(iterations):
                start = time.perf_counter()
                s.sendall(command.encode())
                # Basic response handling
                data = s.recv(1024)
                if not data:
                    break
                latencies.append(time.perf_counter() - start)
    except Exception as e:
        print(f"Client error: {e}")
    return latencies


def run_concurrent_test(host, port, name, command, num_clients=10, req_per_client=1000):
    print(f"Running: {name} with {num_clients} concurrent clients...")

    start_time = time.perf_counter()

    with ThreadPoolExecutor(max_workers=num_clients) as executor:
        # Launch all clients simultaneously
        futures = [
            executor.submit(single_client_task, host, port, command, req_per_client)
            for _ in range(num_clients)
        ]

        # Collect all latency data
        all_latencies = []
        for f in futures:
            all_latencies.extend(f.result())

    total_duration = time.perf_counter() - start_time
    total_reqs = len(all_latencies)

    return {
        "Test": name,
        "Clients": num_clients,
        "Total Req": total_reqs,
        "Throughput (req/s)": f"{total_reqs / total_duration:.2f}",
        "Avg Latency (ms)": f"{statistics.mean(all_latencies)*1000:.3f}",
        "P99 Latency (ms)": f"{statistics.quantiles(all_latencies, n=100)[98]*1000:.3f}" if len(all_latencies) > 100 else "N/A"
    }


if __name__ == "__main__":
    HOST, PORT = "127.0.0.1", 12345

    previous_results = load_baseline()

    current_results = [
        # Baseline: 1 client
        run_concurrent_test(HOST, PORT, "Serial SET", f"SET key {'v' * 200 * 1024}\n", num_clients=1, req_per_client=5000),
        # Contention test: 10 clients
        run_concurrent_test(HOST, PORT, "Concurrent SET", f"SET key {'v' * 200 * 1024}\n", num_clients=10, req_per_client=1000),
        # Read-heavy test
        run_concurrent_test(HOST, PORT, "Concurrent GET", "GET key\n", num_clients=10, req_per_client=1000),
    ]

    print_results(current_results, baseline=previous_results)

    usr_answer = input("\nSave these results as the new baseline? (y/n): ")
    if usr_answer.lower() == 'y':
        save_baseline(current_results)
