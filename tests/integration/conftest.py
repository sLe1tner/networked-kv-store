import pytest
import subprocess
import time
import os
import socket
import sys
import threading


def get_free_port():
    with socket.socket() as s:
        s.bind(('', 0))
        return s.getsockname()[1]


# Show server output while testing
# Use PYTEST_ADDOPTS to enable within ctest:
# PYTEST_ADDOPTS=--show-logs ctest -R Integration --output-on-failure -V
# or run with python from build dir:
# KV_SERVER_BIN=src/server/kv_server python3.10 -m pytest ../tests/integration
def pytest_addoption(parser):
    parser.addoption(
        "--show-logs",
        action="store_true",
        default=False,
        help="Display server stdout/stderr in the console"
    )


@pytest.fixture(scope="session")
def server_path():
    # Get path from environment variable set by CMake
    path = os.getenv("KV_SERVER_BIN")
    if not path or not os.path.exists(path):
        pytest.fail(f"Server binary not found at: {path}. "
                    "Make sure KV_SERVER_BIN is set correctly.")
    return path


@pytest.fixture(scope="session")
def kv_server(server_path, request):
    port = get_free_port()
    show_logs = request.config.getoption("--show-logs")
    print("\n\nDEBUG: show_logs is", show_logs)
    print("\n")

    if show_logs:
        stdout_pipe = subprocess.PIPE
        stderr_pipe = subprocess.PIPE
    else:
        stdout_pipe = subprocess.DEVNULL
        stderr_pipe = subprocess.DEVNULL

    # Start the server process
    proc = subprocess.Popen([server_path, str(port)],
                            stdout=stdout_pipe,
                            stderr=stderr_pipe,
                            text=True)

    if show_logs:
        # Start thread to read the output
        def log_output(pipe, prefix):
            for line in iter(pipe.readline, ''):
                sys.stdout.write(f"[{prefix}] {line}")
                sys.stdout.flush()
        threading.Thread(target=log_output, args=(proc.stdout, "SERV-OUT"), daemon=True).start()
        threading.Thread(target=log_output, args=(proc.stderr, "SERV-ERR"), daemon=True).start()

    # try to connect until it succeeds (max 2 sec)
    timeout = 2.0
    start_time = time.time()
    while True:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.1):
                break
        except (socket.timeout, ConnectionRefusedError):
            if time.time() - start_time > timeout:
                proc.terminate()
                pytest.fail("Server failed to start within 2 seconds")
            time.sleep(0.1)

    yield "127.0.0.1", port

    # Cleanup
    proc.terminate()
    proc.wait()
