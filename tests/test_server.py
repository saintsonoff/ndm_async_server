import pytest
import socket
import time
import threading
import subprocess
import os
import signal
from datetime import datetime
from typing import List, Tuple


def load_env():
    env_vars = {}
    env_path = "/workspaces/ndm_async_server/.env"
    
    if os.path.exists(env_path):
        with open(env_path, 'r') as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith('#') and '=' in line:
                    key, value = line.split('=', 1)
                    env_vars[key.strip()] = value.strip()
    
    tcp_port = int(env_vars.get('TCP_PORT', os.getenv('TCP_PORT', '8080')))
    udp_port = int(env_vars.get('UDP_PORT', os.getenv('UDP_PORT', '8040')))
    
    return tcp_port, udp_port


TCP_PORT, UDP_PORT = load_env()


@pytest.fixture(scope="session")
def server_process():
    time.sleep(0.5)
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect(("localhost", TCP_PORT))
        sock.close()
    except:
        pytest.skip(f"Server is not running on given address localhost:{TCP_PORT}.")
    
    yield None


@pytest.fixture
def tcp_client():
    def _create_client():
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        sock.connect(("localhost", TCP_PORT))
        return sock
    return _create_client


@pytest.fixture
def udp_client():
    def _create_client():
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(5.0)
        return sock, UDP_PORT
    return _create_client


class TestTCPBasicFunctionality:
    
    def test_tcp_echo_message(self, server_process, tcp_client):
        client = tcp_client()
        message = b"Hello Server"
        client.sendall(message)
        response = client.recv(1024)
        client.close()
        assert response == message
    
    def test_tcp_time_command(self, server_process, tcp_client):
        client = tcp_client()
        client.sendall(b"/time")
        response = client.recv(1024).decode().strip()
        client.close()
        
        try:
            datetime.strptime(response, "%Y-%m-%d %H:%M:%S")
            assert True
        except ValueError:
            pytest.fail(f"Invalid time format: {response}")
    
    def test_tcp_stats_command(self, server_process, tcp_client):
        client = tcp_client()
        client.sendall(b"/stats")
        response = client.recv(1024).decode().strip()
        client.close()
        
        assert "Total:" in response
        assert "Current:" in response
    
    def test_tcp_multiple_messages_same_connection(self, server_process, tcp_client):
        client = tcp_client()
        
        messages = [b"Message 1", b"Message 2", b"Message 3"]
        for msg in messages:
            client.sendall(msg)
            response = client.recv(1024)
            assert response == msg
        
        client.close()
    
    def test_tcp_large_message(self, server_process, tcp_client):
        client = tcp_client()
        message = b"X" * 4000
        client.sendall(message)
        response = b""
        while len(response) < len(message):
            chunk = client.recv(4096)
            if not chunk:
                break
            response += chunk
        client.close()
        assert response == message


class TestUDPBasicFunctionality:
    
    def test_udp_echo_message(self, server_process, udp_client):
        client, port = udp_client()
        message = b"Hello UDP"
        client.sendto(message, ("localhost", port))
        response, addr = client.recvfrom(1024)
        client.close()
        assert response == message
    
    def test_udp_time_command(self, server_process, udp_client):
        client, port = udp_client()
        client.sendto(b"/time", ("localhost", port))
        response, addr = client.recvfrom(1024)
        client.close()
        
        try:
            datetime.strptime(response.decode().strip(), "%Y-%m-%d %H:%M:%S")
            assert True
        except ValueError:
            pytest.fail(f"Invalid time format: {response}")
    
    def test_udp_stats_command(self, server_process, udp_client):
        client, port = udp_client()
        client.sendto(b"/stats", ("localhost", port))
        response, addr = client.recvfrom(1024)
        client.close()
        
        response_str = response.decode().strip()
        assert "Total:" in response_str
        assert "Current:" in response_str
    
    def test_udp_multiple_messages(self, server_process, udp_client):
        client, port = udp_client()
        
        messages = [b"UDP Message 1", b"UDP Message 2", b"UDP Message 3"]
        for msg in messages:
            client.sendto(msg, ("localhost", port))
            response, addr = client.recvfrom(1024)
            assert response == msg
        
        client.close()


class TestMultiplexing:
    
    def test_concurrent_tcp_connections(self, server_process, tcp_client):
        num_clients = 10
        results = []
        
        def client_task(client_id: int):
            try:
                client = tcp_client()
                message = f"Client {client_id}".encode()
                client.sendall(message)
                response = client.recv(1024)
                client.close()
                results.append(response == message)
            except Exception as e:
                results.append(False)
        
        threads = []
        for i in range(num_clients):
            thread = threading.Thread(target=client_task, args=(i,))
            thread.start()
            threads.append(thread)
        
        for thread in threads:
            thread.join(timeout=10)
        
        assert len(results) == num_clients
        assert all(results)
    
    def test_concurrent_udp_messages(self, server_process, udp_client):
        num_clients = 10
        results = []
        
        def udp_task(client_id: int):
            try:
                client, port = udp_client()
                message = f"UDP Client {client_id}".encode()
                client.sendto(message, ("localhost", port))
                response, addr = client.recvfrom(1024)
                client.close()
                results.append(response == message)
            except Exception as e:
                results.append(False)
        
        threads = []
        for i in range(num_clients):
            thread = threading.Thread(target=udp_task, args=(i,))
            thread.start()
            threads.append(thread)
        
        for thread in threads:
            thread.join(timeout=10)
        
        assert len(results) == num_clients
        assert all(results)
    
    def test_mixed_tcp_udp_concurrent(self, server_process, tcp_client, udp_client):
        num_operations = 20
        results = []
        
        def tcp_task(task_id: int):
            try:
                client = tcp_client()
                message = f"TCP {task_id}".encode()
                client.sendall(message)
                response = client.recv(1024)
                client.close()
                results.append(("tcp", response == message))
            except Exception as e:
                results.append(("tcp", False))
        
        def udp_task(task_id: int):
            try:
                client, port = udp_client()
                message = f"UDP {task_id}".encode()
                client.sendto(message, ("localhost", port))
                response, addr = client.recvfrom(1024)
                client.close()
                results.append(("udp", response == message))
            except Exception as e:
                results.append(("udp", False))
        
        threads = []
        for i in range(num_operations):
            if i % 2 == 0:
                thread = threading.Thread(target=tcp_task, args=(i,))
            else:
                thread = threading.Thread(target=udp_task, args=(i,))
            thread.start()
            threads.append(thread)
        
        for thread in threads:
            thread.join(timeout=10)
        
        assert len(results) == num_operations
        assert all(success for _, success in results)
    
    def test_rapid_connections_and_disconnections(self, server_process, tcp_client):
        num_iterations = 50
        
        for i in range(num_iterations):
            client = tcp_client()
            message = f"Rapid {i}".encode()
            client.sendall(message)
            response = client.recv(1024)
            client.close()
            assert response == message
    
    def test_long_lived_and_short_lived_connections(self, server_process, tcp_client):
        long_lived = tcp_client()
        
        for i in range(10):
            short_lived = tcp_client()
            message = f"Short {i}".encode()
            short_lived.sendall(message)
            response = short_lived.recv(1024)
            short_lived.close()
            assert response == message
        
        long_message = b"Long lived connection"
        long_lived.sendall(long_message)
        response = long_lived.recv(1024)
        long_lived.close()
        assert response == long_message


class TestStatistics:
    
    def test_statistics_tracking(self, server_process, tcp_client):
        initial_client = tcp_client()
        initial_client.sendall(b"/stats")
        initial_stats = initial_client.recv(1024).decode().strip()
        initial_client.close()
        
        new_connections = []
        for i in range(5):
            client = tcp_client()
            new_connections.append(client)
        
        time.sleep(0.5)
        
        stats_client = tcp_client()
        stats_client.sendall(b"/stats")
        new_stats = stats_client.recv(1024).decode().strip()
        stats_client.close()
        
        for client in new_connections:
            client.close()
        
        assert "Total:" in new_stats
        assert "Current:" in new_stats


class TestEdgeCases:
    
    def test_empty_message_tcp(self, server_process, tcp_client):
        client = tcp_client()
        client.sendall(b"")
        time.sleep(0.1)
        client.close()
    
    def test_special_characters(self, server_process, tcp_client):
        client = tcp_client()
        message = b"!@#$%^&*()_+-=[]{}|;:,.<>?"
        client.sendall(message)
        response = client.recv(1024)
        client.close()
        assert response == message
    
    def test_newline_in_message(self, server_process, tcp_client):
        client = tcp_client()
        message = b"Line1\nLine2\nLine3"
        client.sendall(message)
        response = client.recv(1024)
        client.close()
        assert response == message
    
    def test_command_with_trailing_data(self, server_process, tcp_client):
        client = tcp_client()
        client.sendall(b"/time extra data")
        response = client.recv(1024).decode()
        client.close()
        assert len(response) > 0
    
    def test_invalid_command(self, server_process, tcp_client):
        client = tcp_client()
        client.sendall(b"/invalid_command")
        response = client.recv(1024)
        client.close()
        assert response == b"/invalid_command"


class TestPerformance:
    
    def test_throughput_tcp(self, server_process, tcp_client):
        client = tcp_client()
        num_messages = 100
        
        start_time = time.time()
        for i in range(num_messages):
            message = f"Message {i}".encode()
            client.sendall(message)
            response = client.recv(1024)
            assert response == message
        end_time = time.time()
        
        client.close()
        
        duration = end_time - start_time
        throughput = num_messages / duration
        assert throughput > 10
    
    def test_concurrent_load(self, server_process, tcp_client):
        num_clients = 50
        messages_per_client = 5
        results = []
        
        def load_task(client_id: int):
            try:
                client = tcp_client()
                for i in range(messages_per_client):
                    message = f"Client{client_id}Msg{i}".encode()
                    client.sendall(message)
                    response = client.recv(1024)
                    if response != message:
                        results.append(False)
                        return
                client.close()
                results.append(True)
            except Exception as e:
                results.append(False)
        
        threads = []
        start_time = time.time()
        for i in range(num_clients):
            thread = threading.Thread(target=load_task, args=(i,))
            thread.start()
            threads.append(thread)
        
        for thread in threads:
            thread.join(timeout=30)
        end_time = time.time()
        
        assert len(results) == num_clients
        assert all(results)
        assert end_time - start_time < 10
