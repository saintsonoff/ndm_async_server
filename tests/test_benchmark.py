import pytest
import socket
import time
import threading
import statistics
import os
from concurrent.futures import ThreadPoolExecutor, as_completed


TCP_PORT = int(os.getenv('TCP_PORT', '8080'))
UDP_PORT = int(os.getenv('UDP_PORT', '8040'))


@pytest.fixture(scope="module")
def server_ready():
    time.sleep(0.5)
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect(("localhost", TCP_PORT))
        sock.close()
    except:
        pytest.skip(f"Server is not running on localhost:{TCP_PORT}")
    yield


class TestHighLoadBenchmark:
    
    TARGET_RPS_10K = 10000
    BASELINE_P99_MS = 50.0
    SUCCESS_RATE_THRESHOLD = 0.95
    RPS_ACHIEVEMENT_THRESHOLD = 0.8
    MAX_P99_MS = 50.0
    
    SUSTAINED_DURATION_SEC = 60
    SUSTAINED_TARGET_RPS = 5000
    SUSTAINED_MAX_P99_MS = 100.0
    SUSTAINED_MIN_SUCCESS_RATE = 0.99
    
    BURST_SIZE = 1000
    BURST_COUNT = 5
    BURST_PAUSE_SEC = 2.0
    BURST_MAX_P99_MS = 20.0
    
    POOL_SIZE = 20
    POOL_REQUESTS_PER_CONN = 500
    POOL_MAX_P99_MS = 10.0
    POOL_MIN_SUCCESS_RATE = 0.99
    
    def test_10k_rps_target_tcp(self, server_ready):
        target_rps = self.TARGET_RPS_10K
        duration_seconds = 5
        target_requests = target_rps * duration_seconds
        
        num_workers = 100
        requests_per_worker = target_requests // num_workers
        
        latencies = []
        errors = []
        lock = threading.Lock()
        
        def worker(worker_id: int):
            local_latencies = []
            local_errors = 0
            
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(1.0)
                sock.connect(("localhost", TCP_PORT))
                
                for i in range(requests_per_worker):
                    message = f"W{worker_id}M{i}".encode()
                    
                    try:
                        start = time.perf_counter()
                        sock.sendall(message)
                        response = sock.recv(1024)
                        end = time.perf_counter()
                        
                        if response == message:
                            local_latencies.append((end - start) * 1000)
                        else:
                            local_errors += 1
                    except Exception:
                        local_errors += 1
                
                sock.close()
            except Exception:
                local_errors += requests_per_worker
            
            with lock:
                latencies.extend(local_latencies)
                errors.append(local_errors)
        
        start_time = time.time()
        
        with ThreadPoolExecutor(max_workers=num_workers) as executor:
            futures = [executor.submit(worker, i) for i in range(num_workers)]
            for future in as_completed(futures):
                future.result()
        
        end_time = time.time()
        actual_duration = end_time - start_time
        
        total_errors = sum(errors)
        successful_requests = len(latencies)
        actual_rps = successful_requests / actual_duration
        
        if latencies:
            latencies.sort()
            p50 = latencies[int(0.50 * len(latencies))]
            p95 = latencies[int(0.95 * len(latencies))]
            p99 = latencies[int(0.99 * len(latencies))]
            p999 = latencies[int(0.999 * len(latencies))] if len(latencies) > 1000 else p99
            avg = statistics.mean(latencies)
            min_lat = min(latencies)
            max_lat = max(latencies)
        else:
            p50 = p95 = p99 = p999 = avg = min_lat = max_lat = 0
        
        baseline_rps = self.TARGET_RPS_10K
        baseline_p99 = self.BASELINE_P99_MS
        
        rps_ratio = (actual_rps / baseline_rps) * 100
        p99_improvement = ((baseline_p99 - p99) / baseline_p99) * 100
        
        assert successful_requests > target_requests * self.SUCCESS_RATE_THRESHOLD
        assert actual_rps > target_rps * self.RPS_ACHIEVEMENT_THRESHOLD
        assert p99 < self.MAX_P99_MS
    
    def test_sustained_load_1_minute(self, server_ready):
        duration_seconds = self.SUSTAINED_DURATION_SEC
        target_rps = self.SUSTAINED_TARGET_RPS
        num_workers = 50
        
        latencies = []
        errors = 0
        lock = threading.Lock()
        start_time = time.time()
        stop_flag = threading.Event()
        
        def worker(worker_id: int):
            nonlocal errors
            local_latencies = []
            local_errors = 0
            request_count = 0
            
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(1.0)
                sock.connect(("localhost", TCP_PORT))
                
                while not stop_flag.is_set():
                    message = f"Sustained{worker_id}_{request_count}".encode()
                    
                    try:
                        start = time.perf_counter()
                        sock.sendall(message)
                        response = sock.recv(1024)
                        end = time.perf_counter()
                        
                        if response == message:
                            local_latencies.append((end - start) * 1000)
                        else:
                            local_errors += 1
                        
                        request_count += 1
                        
                        elapsed = time.time() - start_time
                        expected_requests = int((elapsed / duration_seconds) * (target_rps / num_workers))
                        if request_count > expected_requests:
                            time.sleep(0.001)
                    
                    except socket.timeout:
                        local_errors += 1
                    except Exception:
                        local_errors += 1
                        break
                
                sock.close()
            except Exception:
                pass
            
            with lock:
                latencies.extend(local_latencies)
                errors += local_errors
        
        threads = []
        for i in range(num_workers):
            t = threading.Thread(target=worker, args=(i,), daemon=True)
            t.start()
            threads.append(t)
        
        time.sleep(duration_seconds)
        stop_flag.set()
        
        for t in threads:
            t.join(timeout=2)
        
        actual_duration = time.time() - start_time
        successful_requests = len(latencies)
        actual_rps = successful_requests / actual_duration
        
        if latencies:
            latencies.sort()
            p50 = latencies[int(0.50 * len(latencies))]
            p95 = latencies[int(0.95 * len(latencies))]
            p99 = latencies[int(0.99 * len(latencies))]
            avg = statistics.mean(latencies)
        else:
            p50 = p95 = p99 = avg = 0
        
        assert actual_rps > target_rps * self.RPS_ACHIEVEMENT_THRESHOLD
        assert p99 < self.SUSTAINED_MAX_P99_MS
        assert (successful_requests / (successful_requests + errors)) > self.SUSTAINED_MIN_SUCCESS_RATE
    
    def test_burst_capacity(self, server_ready):
        burst_size = self.BURST_SIZE
        num_bursts = self.BURST_COUNT
        pause_between_bursts = self.BURST_PAUSE_SEC
        
        all_latencies = []

        for burst_num in range(num_bursts):
            latencies = []
            errors = 0
            lock = threading.Lock()
            
            def burst_worker():
                nonlocal errors
                try:
                    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    sock.settimeout(2.0)
                    sock.connect(("localhost", TCP_PORT))
                    
                    message = f"Burst{burst_num}".encode()
                    
                    start = time.perf_counter()
                    sock.sendall(message)
                    response = sock.recv(1024)
                    end = time.perf_counter()
                    
                    sock.close()
                    
                    if response == message:
                        with lock:
                            latencies.append((end - start) * 1000)
                    else:
                        with lock:
                            errors += 1
                except Exception:
                    with lock:
                        errors += 1
            
            start_time = time.time()
            
            threads = []
            for _ in range(burst_size):
                t = threading.Thread(target=burst_worker)
                t.start()
                threads.append(t)
            
            for t in threads:
                t.join(timeout=5)
            
            burst_duration = time.time() - start_time
            
            if latencies:
                latencies.sort()
                p99 = latencies[int(0.99 * len(latencies))]
                avg = statistics.mean(latencies)
            else:
                p99 = avg = 0
            
            all_latencies.extend(latencies)
            
            if burst_num < num_bursts - 1:
                time.sleep(pause_between_bursts)
        
        if all_latencies:
            all_latencies.sort()
            overall_p99 = all_latencies[int(0.99 * len(all_latencies))]

            assert overall_p99 < self.BURST_MAX_P99_MS
    
    def test_connection_pool_reuse(self, server_ready):
        pool_size = self.POOL_SIZE
        requests_per_connection = self.POOL_REQUESTS_PER_CONN
        total_requests = pool_size * requests_per_connection
        
        latencies = []
        errors = 0
        lock = threading.Lock()
        
        def pooled_worker(worker_id: int):
            nonlocal errors
            local_latencies = []
            local_errors = 0
            
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(1.0)
                sock.connect(("localhost", TCP_PORT))
                
                for i in range(requests_per_connection):
                    message = f"Pool{worker_id}_{i}".encode()
                    
                    try:
                        start = time.perf_counter()
                        sock.sendall(message)
                        response = sock.recv(1024)
                        end = time.perf_counter()
                        
                        if response == message:
                            local_latencies.append((end - start) * 1000)
                        else:
                            local_errors += 1
                    except Exception:
                        local_errors += 1
                
                sock.close()
            except Exception:
                local_errors += requests_per_connection
            
            with lock:
                latencies.extend(local_latencies)
                errors += local_errors

        start_time = time.time()
        
        threads = []
        for i in range(pool_size):
            t = threading.Thread(target=pooled_worker, args=(i,))
            t.start()
            threads.append(t)
        
        for t in threads:
            t.join()
        
        duration = time.time() - start_time
        actual_rps = len(latencies) / duration
        
        if latencies:
            latencies.sort()
            p50 = latencies[int(0.50 * len(latencies))]
            p95 = latencies[int(0.95 * len(latencies))]
            p99 = latencies[int(0.99 * len(latencies))]
            avg = statistics.mean(latencies)
        else:
            p50 = p95 = p99 = avg = 0

        assert len(latencies) > total_requests * self.POOL_MIN_SUCCESS_RATE
        assert p99 < self.POOL_MAX_P99_MS
