import threading
import socket

def client_task(host, port, thread_id):
    for i in range(100):
        with socket.create_connection((host, port)) as s:
            key = f"thread_{thread_id}_key_{i}"

            s.sendall(f"SET {key} value_{i}\n".encode())
            s.recv(1024) # Wait for OK

            s.sendall(f"GET {key}\n".encode())
            resp = s.recv(1024).decode()
            assert f"value_{i}" in resp


def test_concurrent_clients(kv_server):
    host, port = kv_server
    threads = []

    # Spawn 10 clients
    for i in range(10):
        t = threading.Thread(target=client_task, args=(host, port, i))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()
