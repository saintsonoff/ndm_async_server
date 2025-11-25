import pytest
from hypothesis import given, strategies as st, settings, HealthCheck
import socket
import time
import threading
import os
from typing import List


TCP_PORT, UDP_PORT = int(os.getenv('TCP_PORT', '8080')), int(os.getenv('UDP_PORT', '8040'))


@pytest.fixture(scope="module")
def server_ready():
    time.sleep(1)
    return True


@pytest.fixture
def tcp_socket(server_ready):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5.0)
    try:
        sock.connect(("localhost", TCP_PORT))
        yield sock
    finally:
        sock.close()


@pytest.fixture
def udp_socket(server_ready):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(5.0)
    yield sock, UDP_PORT
    sock.close()


class TestPropertyBasedTCP:
    
    @given(message=st.text(min_size=1, max_size=1000).map(lambda x: x.encode('utf-8')))
    @settings(max_examples=50, suppress_health_check=[HealthCheck.function_scoped_fixture])
    def test_tcp_echo_property(self, tcp_socket, message):
        if message.startswith(b'/'):
            pytest.skip("Command messages tested separately")
        
        tcp_socket.sendall(message)
        response = b""
        while len(response) < len(message):
            chunk = tcp_socket.recv(4096)
            if not chunk:
                break
            response += chunk
        
        assert response == message
    
    @given(message=st.text(
        alphabet=st.characters(blacklist_categories=('Cs', 'Cc'), blacklist_characters='/'),
        min_size=1,
        max_size=500
    ))
    @settings(max_examples=30, suppress_health_check=[HealthCheck.function_scoped_fixture])
    def test_tcp_non_command_messages(self, message):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        try:
            sock.connect(("localhost", TCP_PORT))
            encoded = message.encode('utf-8', errors='ignore')
            if len(encoded) == 0 or encoded.startswith(b'/'):
                return
            
            sock.sendall(encoded)
            response = b""
            while len(response) < len(encoded):
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response += chunk
            
            assert response == encoded
        finally:
            sock.close()
    
    @given(size=st.integers(min_value=1, max_value=4000))
    @settings(max_examples=20, suppress_health_check=[HealthCheck.function_scoped_fixture])
    def test_tcp_various_message_sizes(self, size):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        try:
            sock.connect(("localhost", TCP_PORT))
            message = b'A' * size
            sock.sendall(message)
            response = b""
            while len(response) < len(message):
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response += chunk
            
            assert len(response) == size
            assert response == message
        finally:
            sock.close()


class TestPropertyBasedUDP:
    
    @given(message=st.text(min_size=1, max_size=1000).map(lambda x: x.encode('utf-8')))
    @settings(max_examples=50, suppress_health_check=[HealthCheck.function_scoped_fixture])
    def test_udp_echo_property(self, udp_socket, message):
        if message.startswith(b'/'):
            pytest.skip("Command messages tested separately")
        
        sock, port = udp_socket
        sock.sendto(message, ("localhost", port))
        response, addr = sock.recvfrom(4096)
        
        assert response == message
    
    @given(message=st.text(
        alphabet=st.characters(blacklist_categories=('Cs', 'Cc'), blacklist_characters='/'),
        min_size=1,
        max_size=500
    ))
    @settings(max_examples=30, suppress_health_check=[HealthCheck.function_scoped_fixture])
    def test_udp_non_command_messages(self, message):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(5.0)
        try:
            encoded = message.encode('utf-8', errors='ignore')
            if len(encoded) == 0 or encoded.startswith(b'/'):
                return
            
            sock.sendto(encoded, ("localhost", UDP_PORT))
            response, addr = sock.recvfrom(4096)
            
            assert response == encoded
        finally:
            sock.close()


class TestPropertyBasedCommands:
    
    @given(prefix=st.text(min_size=0, max_size=50))
    @settings(max_examples=20, suppress_health_check=[HealthCheck.function_scoped_fixture])
    def test_command_recognition(self, prefix):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        try:
            sock.connect(("localhost", TCP_PORT))
            
            message = (prefix + "data").encode('utf-8', errors='ignore')
            if len(message) == 0:
                return
            
            sock.sendall(message)
            response = sock.recv(4096)
            
            assert len(response) > 0
        finally:
            sock.close()
    
    @given(command=st.sampled_from([b"/time", b"/stats"]))
    @settings(max_examples=10, suppress_health_check=[HealthCheck.function_scoped_fixture])
    def test_valid_commands_tcp(self, command):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        try:
            sock.connect(("localhost", TCP_PORT))
            sock.sendall(command)
            response = sock.recv(4096)
            
            assert len(response) > 0
            if command == b"/time":
                assert len(response.decode().strip()) == 19
            elif command == b"/stats":
                assert b"Total:" in response
        finally:
            sock.close()
    
    @given(command=st.sampled_from([b"/time", b"/stats"]))
    @settings(max_examples=10, suppress_health_check=[HealthCheck.function_scoped_fixture])
    def test_valid_commands_udp(self, command):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(5.0)
        try:
            sock.sendto(command, ("localhost", UDP_PORT))
            response, addr = sock.recvfrom(4096)
            
            assert len(response) > 0
            if command == b"/time":
                assert len(response.decode().strip()) == 19
            elif command == b"/stats":
                assert b"Total:" in response
        finally:
            sock.close()


class TestPropertyBasedConcurrency:
    
    @given(
        num_clients=st.integers(min_value=2, max_value=20),
        message_size=st.integers(min_value=10, max_value=200)
    )
    @settings(max_examples=10, deadline=None)
    def test_concurrent_clients_property(self, num_clients, message_size):
        results = []
        
        def client_task(client_id: int):
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(5.0)
                sock.connect(("localhost", TCP_PORT))
                
                message = (f"C{client_id}:" + "X" * message_size).encode()
                sock.sendall(message)
                response = b""
                while len(response) < len(message):
                    chunk = sock.recv(4096)
                    if not chunk:
                        break
                    response += chunk
                
                sock.close()
                results.append(response == message)
            except Exception as e:
                results.append(False)
        
        threads = []
        for i in range(num_clients):
            thread = threading.Thread(target=client_task, args=(i,))
            thread.start()
            threads.append(thread)
        
        for thread in threads:
            thread.join(timeout=15)
        
        assert len(results) == num_clients
        assert all(results), f"Failed clients: {results.count(False)}/{num_clients}"
    
    @given(
        num_operations=st.integers(min_value=5, max_value=30)
    )
    @settings(max_examples=10, deadline=None)
    def test_mixed_protocol_property(self, num_operations):
        results = []
        
        def tcp_task(task_id: int):
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(5.0)
                sock.connect(("localhost", TCP_PORT))
                message = f"TCP{task_id}".encode()
                sock.sendall(message)
                response = sock.recv(4096)
                sock.close()
                results.append(response == message)
            except Exception:
                results.append(False)
        
        def udp_task(task_id: int):
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                sock.settimeout(5.0)
                message = f"UDP{task_id}".encode()
                sock.sendto(message, ("localhost", UDP_PORT))
                response, _ = sock.recvfrom(4096)
                sock.close()
                results.append(response == message)
            except Exception:
                results.append(False)
        
        threads = []
        for i in range(num_operations):
            if i % 2 == 0:
                thread = threading.Thread(target=tcp_task, args=(i,))
            else:
                thread = threading.Thread(target=udp_task, args=(i,))
            thread.start()
            threads.append(thread)
        
        for thread in threads:
            thread.join(timeout=15)
        
        assert len(results) == num_operations
        success_rate = sum(results) / len(results)
        assert success_rate >= 0.95, f"Success rate: {success_rate:.2%}"


class TestPropertyBasedStress:
    
    @given(
        iterations=st.integers(min_value=10, max_value=50)
    )
    @settings(max_examples=5, deadline=None)
    def test_rapid_connect_disconnect_property(self, iterations):
        failures = 0
        
        for i in range(iterations):
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(2.0)
                sock.connect(("localhost", TCP_PORT))
                message = f"R{i}".encode()
                sock.sendall(message)
                response = sock.recv(1024)
                sock.close()
                
                if response != message:
                    failures += 1
            except Exception:
                failures += 1
        
        failure_rate = failures / iterations
        assert failure_rate < 0.05, f"Failure rate: {failure_rate:.2%}"
    
    @given(
        num_messages=st.integers(min_value=5, max_value=20)
    )
    @settings(max_examples=10, deadline=None, suppress_health_check=[HealthCheck.function_scoped_fixture])
    def test_sequential_messages_property(self, tcp_socket, num_messages):
        for i in range(num_messages):
            message = f"Seq{i}".encode()
            tcp_socket.sendall(message)
            response = tcp_socket.recv(1024)
            assert response == message
