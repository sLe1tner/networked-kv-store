import socket
import time


def send_cmd(addr, port, cmd):
    with socket.create_connection((addr, port)) as client:
        client.sendall(cmd.encode() + b'\n')
        return client.recv(1024).decode()


def test_set_get_del(kv_server):
    host, port = kv_server
    resp = send_cmd(host, port, "SET key1 hello")
    assert "OK" in resp
    resp = send_cmd(host, port, "GET key1")
    assert "hello" in resp
    resp = send_cmd(host, port, "del key1")
    assert "OK" in resp
    resp = send_cmd(host, port, "GET key1")
    assert "key not found" in resp


def test_client_disconnect_cleanup(kv_server):
    host, port = kv_server
    # Connect and immediately disconnect
    for _ in range(5):
        s = socket.create_connection((host, port))
        s.close()

    # After 5 connects/disconnects, the 6th should still work perfectly
    resp = send_cmd(host, port, "SET cleanup works\n")
    assert "OK" in resp


def test_fragmented_packet(kv_server):
    host, port = kv_server
    with socket.create_connection((host, port)) as s:
        s.sendall(b"SE")
        time.sleep(0.1)
        s.sendall(b"T key value\n")
        resp = s.recv(1024).decode()
        assert "OK" in resp


def test_large_value(kv_server):
    host, port = kv_server
    big_value = "A" * 6000  # more than the 4096 byte buffer
    cmd = f"SET big_key {big_value}\n".encode()

    with socket.create_connection((host, port)) as s:
        s.sendall(cmd)
        assert b"OK" in s.recv(1024)

        s.sendall(b"GET big_key\n")
        resp = b""
        # Receive until we get the full value back
        while len(resp) < 6000:
            resp += s.recv(4096)
        assert big_value.encode() in resp


def test_to_large_value(kv_server):
    host, port = kv_server
    payload = "A" * (1024 * 1024 * 5) # 5MB, max is currently set to 64KB
    resp = send_cmd(host, port, f"SET big_key {payload}\n")
    assert "value too large" in resp


def test_garbage_input(kv_server):
    host, port = kv_server
    with socket.create_connection((host, port)) as s:
        s.sendall(b"NOT_A_COMMAND key val\n")
        resp = s.recv(1024).decode()
        assert "unknown command" in resp

        # Connection still works after error
        s.sendall(b"SET recovery 1\n")
        assert b"OK" in s.recv(1024)
