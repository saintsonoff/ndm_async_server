import pytest
import socket
import time
import threading
import subprocess
import os
import signal
import psutil
import statistics
from datetime import datetime
from typing import List, Tuple


TCP_PORT, UDP_PORT = int(os.getenv('TCP_PORT', '8080')), int(os.getenv('UDP_PORT', '8040'))


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
    
    def test_tcp_message_exceeding_buffer(self, server_process, tcp_client):
        client = tcp_client()
        message = b"Z" * 10000
        client.sendall(message)
        response = b""
        while len(response) < len(message):
            chunk = client.recv(8192)
            if not chunk:
                break
            response += chunk
        client.close()
        assert response == message
    
    def test_udp_message_at_buffer_limit(self, server_process, udp_client):
        client, port = udp_client()
        message = b"M" * 8191
        client.sendto(message, ("localhost", port))
        response, addr = client.recvfrom(8192)
        client.close()
        assert response == message
    
    def test_udp_message_exceeding_buffer(self, server_process, udp_client):
        client, port = udp_client()
        message = b"L" * 8200
        client.sendto(message, ("localhost", port))
        response, addr = client.recvfrom(8192)
        client.close()
        assert b"Error: Message too large" in response


class TestPerformance:
    
    def test_throughput_tcp(self, server_process, tcp_client):
        client = tcp_client()
        num_messages = 1000
        
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
        assert throughput > 100
    
    def test_concurrent_load(self, server_process, tcp_client):
        num_clients = 200
        messages_per_client = 20
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
            thread.join(timeout=60)
        end_time = time.time()
        
        assert len(results) == num_clients
        assert all(results)
        assert end_time - start_time < 30
    
    def test_latency_percentiles(self, server_process, tcp_client):
        num_requests = 1000
        latencies = []
        
        client = tcp_client()
        for i in range(num_requests):
            message = f"Latency test {i}".encode()
            
            start = time.perf_counter()
            client.sendall(message)
            response = client.recv(1024)
            end = time.perf_counter()
            
            assert response == message
            latencies.append((end - start) * 1000)
        
        client.close()
        
        latencies.sort()
        p50 = statistics.median(latencies)
        p95 = latencies[int(0.95 * len(latencies))]
        p99 = latencies[int(0.99 * len(latencies))]
        avg = statistics.mean(latencies)

        assert p50 < 10
        assert p95 < 50
        assert p99 < 100
    
    def test_cpu_memory_usage_under_load(self, server_process, tcp_client):
        server_pid = None
        for proc in psutil.process_iter(['pid', 'name', 'cmdline']):
            try:
                if 'async_server' in ' '.join(proc.info['cmdline'] or []):
                    server_pid = proc.info['pid']
                    break
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue
        
        if server_pid is None:
            pytest.skip("Server process not found")
        
        server_proc = psutil.Process(server_pid)
        
        cpu_samples = []
        memory_samples = []
        
        def monitor_resources():
            for _ in range(10):
                try:
                    cpu_samples.append(server_proc.cpu_percent(interval=0.1))
                    memory_samples.append(server_proc.memory_info().rss / 1024 / 1024)
                except psutil.NoSuchProcess:
                    break
                time.sleep(0.1)
        
        monitor_thread = threading.Thread(target=monitor_resources, daemon=True)
        monitor_thread.start()
        
        num_clients = 50
        results = []
        
        def load_task(client_id: int):
            try:
                client = tcp_client()
                for i in range(10):
                    message = f"Load{client_id}Msg{i}".encode()
                    client.sendall(message)
                    response = client.recv(1024)
                    if response != message:
                        results.append(False)
                        return
                client.close()
                results.append(True)
            except Exception:
                results.append(False)
        
        threads = []
        for i in range(num_clients):
            thread = threading.Thread(target=load_task, args=(i,))
            thread.start()
            threads.append(thread)
        
        for thread in threads:
            thread.join(timeout=30)
        
        monitor_thread.join(timeout=2)
        
        if cpu_samples and memory_samples:
            avg_cpu = statistics.mean(cpu_samples)
            max_cpu = max(cpu_samples)
            avg_memory = statistics.mean(memory_samples)
            max_memory = max(memory_samples)

            assert max_cpu < 90
            assert max_memory < 100
        
        assert all(results)


class TestPartialIO:
    
    def test_partial_send_recv(self, server_process, tcp_client):
        client = tcp_client()
        client.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        
        large_message = b"X" * 16384
        
        sent_total = 0
        while sent_total < len(large_message):
            try:
                chunk_size = min(1024, len(large_message) - sent_total)
                sent = client.send(large_message[sent_total:sent_total + chunk_size])
                if sent == 0:
                    break
                sent_total += sent
                time.sleep(0.001)
            except BlockingIOError:
                time.sleep(0.01)
                continue
        
        assert sent_total == len(large_message)
        
        received = b""
        while len(received) < len(large_message):
            try:
                chunk = client.recv(4096)
                if not chunk:
                    break
                received += chunk
            except BlockingIOError:
                time.sleep(0.01)
                continue
        
        client.close()
        assert received == large_message
    
    def test_non_blocking_behavior(self, server_process, tcp_client):
        client = tcp_client()
        client.setblocking(False)
        
        message = b"Non-blocking test"
        
        sent = False
        for _ in range(100):
            try:
                client.sendall(message)
                sent = True
                break
            except BlockingIOError:
                time.sleep(0.01)
        
        assert sent
        
        response = b""
        for _ in range(100):
            try:
                chunk = client.recv(1024)
                if chunk:
                    response += chunk
                    if len(response) >= len(message):
                        break
            except BlockingIOError:
                time.sleep(0.01)
        
        client.close()
        assert response == message
    
    def test_interrupted_operations(self, server_process, tcp_client):
        client = tcp_client()
        
        def signal_handler(signum, frame):
            pass
        
        old_handler = signal.signal(signal.SIGALRM, signal_handler)
        
        try:
            messages_sent = 0
            for i in range(50):
                message = f"Interrupted {i}".encode()
                
                signal.setitimer(signal.ITIMER_REAL, 0.001)
                
                try:
                    client.sendall(message)
                    response = client.recv(1024)
                    assert response == message
                    messages_sent += 1
                except InterruptedError:
                    continue
                finally:
                    signal.setitimer(signal.ITIMER_REAL, 0)
            
            client.close()
            assert messages_sent > 40
        
        finally:
            signal.signal(signal.SIGALRM, old_handler)
            signal.setitimer(signal.ITIMER_REAL, 0)
    
    def test_slow_consumer(self, server_process, tcp_client):
        client = tcp_client()
        
        num_messages = 20
        for i in range(num_messages):
            message = f"Slow consumer {i}".encode()
            client.sendall(message)
            
            time.sleep(0.05)
            
            response = client.recv(1024)
            assert response == message
        
        client.close()
    
    def test_fragmented_udp_behavior(self, server_process, udp_client):
        client, port = udp_client()
        
        sizes = [100, 500, 1000, 2000, 4000, 8000]
        
        for size in sizes:
            message = b"F" * size
            client.sendto(message, ("localhost", port))
            
            try:
                response, addr = client.recvfrom(8192)
                assert response == message
            except socket.timeout:
                pytest.fail(f"Timeout receiving {size} byte message")
        
        client.close()


class TestConnectionLimits:
    """Test connection limit (kMaxClients = 10000) - DOS protection"""
    
    def test_many_concurrent_connections(self, server_process, tcp_client):
        """Test server handles many concurrent connections (well below limit)"""
        connections = []
        max_test_connections = 100
        
        try:
            for i in range(max_test_connections):
                client = tcp_client()
                connections.append(client)
                
            assert len(connections) == max_test_connections
            
            # Test that all connections are functional
            test_message = b"Connection test"
            for i, conn in enumerate(connections):
                conn.sendall(test_message)
                response = conn.recv(1024)
                assert response == test_message, f"Connection {i} failed"
                
        finally:
            for conn in connections:
                try:
                    conn.close()
                except:
                    pass
    
    def test_connection_accepted_below_limit(self, server_process, tcp_client):
        """Verify connections work properly when below kMaxClients limit"""
        num_connections = 50
        connections = []
        
        try:
            for _ in range(num_connections):
                client = tcp_client()
                connections.append(client)
                
            for conn in connections:
                conn.sendall(b"test")
                response = conn.recv(1024)
                assert response == b"test"
                
        finally:
            for conn in connections:
                try:
                    conn.close()
                except:
                    pass


class TestThreadSafeTime:
    """Test thread-safe time retrieval using localtime_r"""
    
    def test_concurrent_time_requests(self, server_process, tcp_client):
        """Test multiple concurrent /time requests work correctly"""
        num_threads = 50
        results = []
        errors = []
        
        def time_request():
            try:
                client = tcp_client()
                client.sendall(b"/time")
                response = client.recv(1024).decode().strip()
                client.close()
                
                try:
                    parsed_time = datetime.strptime(response, "%Y-%m-%d %H:%M:%S")
                    results.append(parsed_time)
                except ValueError as e:
                    errors.append(f"Invalid format: {response}")
                    
            except Exception as e:
                errors.append(str(e))
        
        threads = []
        
        for _ in range(num_threads):
            thread = threading.Thread(target=time_request)
            thread.start()
            threads.append(thread)
        
        for thread in threads:
            thread.join(timeout=10)
        
        assert len(errors) == 0, f"Errors occurred: {errors[:5]}"
        assert len(results) == num_threads
        
        for t in results:
            assert t.year >= 2025
        
        times_sorted = sorted(results)
        time_diff = (times_sorted[-1] - times_sorted[0]).total_seconds()
        assert time_diff < 5, f"Time difference too large: {time_diff}s"
    
    def test_time_format_consistency(self, server_process, tcp_client):
        """Test time format remains consistent"""
        client = tcp_client()
        client.sendall(b"/time")
        response = client.recv(1024).decode().strip()
        client.close()
        
        parsed_time = datetime.strptime(response, "%Y-%m-%d %H:%M:%S")
        
        now = datetime.now()
        time_diff = abs((now - parsed_time).total_seconds())
        assert time_diff < 5, f"Time difference too large: {time_diff}s"


class TestEventfdIntegration:
    """Test eventfd (wakeup_fd) integration for fast shutdown"""
    
    def test_event_loop_still_responsive(self, server_process, tcp_client):
        """Verify eventfd doesn't break normal event loop operation"""
        num_iterations = 100
        
        for i in range(num_iterations):
            client = tcp_client()
            message = f"Eventfd test {i}".encode()
            client.sendall(message)
            response = client.recv(1024)
            client.close()
            
            assert response == message
    
    def test_rapid_connect_disconnect_with_eventfd(self, server_process, tcp_client):
        """Test rapid connections don't interfere with eventfd"""
        num_clients = 50
        results = []
        
        def client_task(client_id):
            try:
                client = tcp_client()
                for i in range(5):
                    message = f"Client{client_id}Msg{i}".encode()
                    client.sendall(message)
                    response = client.recv(1024)
                    if response != message:
                        results.append(False)
                        return
                client.close()
                results.append(True)
            except Exception:
                results.append(False)
        
        threads = []
        for i in range(num_clients):
            thread = threading.Thread(target=client_task, args=(i,))
            thread.start()
            threads.append(thread)
        
        for thread in threads:
            thread.join(timeout=30)
        
        assert len(results) == num_clients
        assert all(results), f"Failed: {results.count(False)}/{num_clients}"


class TestMemoryOrderingFix:
    """Test that memory ordering is consistent (fixed std::memory_order::acquire)"""
    
    def test_concurrent_stats_access(self, server_process, tcp_client):
        """Test concurrent stats access with correct memory ordering"""
        num_threads = 50
        results = []
        
        def stats_task(thread_id):
            try:
                client = tcp_client()
                client.sendall(b"/stats")
                response = client.recv(1024).decode().strip()
                client.close()
                
                assert "Total:" in response
                assert "Current:" in response
                
                results.append(True)
            except Exception:
                results.append(False)
        
        threads = []
        for i in range(num_threads):
            thread = threading.Thread(target=stats_task, args=(i,))
            thread.start()
            threads.append(thread)
        
        for thread in threads:
            thread.join(timeout=10)
        
        assert len(results) == num_threads
        assert all(results), "Some stats requests failed"
