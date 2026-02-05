import time
import socket


def test_throughput(kv_server):
    host, port = kv_server
    iterations = 5000

    start_time = time.perf_counter()
    with socket.create_connection((host, port)) as s:
        for i in range(iterations):
            s.sendall(f"SET key_{i} value_{i}\n".encode())
            s.recv(1024)
    duration = time.perf_counter() - start_time

    # Server must handle at least 1000 req/sec
    req_per_sec = iterations / duration
    assert req_per_sec > 1000, f"Performance regression detected: {req_per_sec} req/sec"



def test_connection_saturation_resilience(kv_server):
    host, port = kv_server
    max_connections = 100
    connections = []
    try:
        # Open 100 connections
        for i in range(max_connections):
            s = socket.create_connection((host, port))
            # Send a partial command to keep the server's buffer active
            s.sendall(f"SET key_{i} ".encode())
            connections.append(s)

        # Verify the server is still responsive on a new connection
        # -> poll loop isn't blocked by the 100 existing ones
        with socket.create_connection((host, port)) as supervisor:
            supervisor.sendall(b"PING\n")
            assert b"Pong" in supervisor.recv(1024)

        # Complete the commands on the 100 connections
        for i, s in enumerate(connections):
            s.sendall(f"val_{i}\n".encode())
            assert b"OK" in s.recv(1024)

    finally:
        for s in connections:
            s.close()



def test_slow_loris_resilience(kv_server):
    host, port = kv_server
    slow_cmd = b"SET slow_key some_value\n"

    with socket.create_connection((host, port)) as slow_client:
        # Send byte by byte
        for char in slow_cmd[:-1]: # everything but the newline
            slow_client.sendall(bytes([char]))
            time.sleep(0.01)

        # While the slow client is hanging, a fast client should be unaffected
        with socket.create_connection((host, port)) as fast_client:
            fast_client.sendall(b"PING\n")
            assert b"Pong" in fast_client.recv(1024)

        # Finish the slow command
        slow_client.sendall(b"\n")
        assert b"OK" in slow_client.recv(1024)



def test_connection_churn(kv_server):
    host, port = kv_server
    # Open and close 500 connections in sequence
    for i in range(500):
        with socket.create_connection((host, port)) as s:
            s.sendall(b"PING\n")
            assert b"Pong" in s.recv(1024)
    # If the server survived, it's not leaking file descriptors per connection
