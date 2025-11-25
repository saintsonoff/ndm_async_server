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
    BURST_MAX_P99_MS = 200.0
    
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
        
        print(f"\n{'='*70}")
        print(f"High Load Benchmark: {target_rps:,} RPS target")
        print(f"{'='*70}")
        print(f"Configuration:")
        print(f"  Workers: {num_workers}")
        print(f"  Requests per worker: {requests_per_worker}")
        print(f"  Total requests: {target_requests:,}")
        print(f"  Duration: {duration_seconds}s")
        
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
        
        print(f"\n{'='*70}")
        print(f"Results:")
        print(f"{'='*70}")
        print(f"Throughput:")
        print(f"  Target RPS:      {target_rps:>10,}")
        print(f"  Actual RPS:      {actual_rps:>10,.0f}")
        print(f"  Achievement:     {(actual_rps/target_rps*100):>10.1f}%")
        print(f"\nRequests:")
        print(f"  Total:           {target_requests:>10,}")
        print(f"  Successful:      {successful_requests:>10,}")
        print(f"  Failed:          {total_errors:>10,}")
        print(f"  Success rate:    {(successful_requests/target_requests*100):>10.2f}%")
        print(f"\nLatency (ms):")
        print(f"  Min:             {min_lat:>10.3f}")
        print(f"  Average:         {avg:>10.3f}")
        print(f"  p50 (median):    {p50:>10.3f}")
        print(f"  p95:             {p95:>10.3f}")
        print(f"  p99:             {p99:>10.3f}")
        print(f"  p99.9:           {p999:>10.3f}")
        print(f"  Max:             {max_lat:>10.3f}")
        print(f"\nTiming:")
        print(f"  Duration:        {actual_duration:>10.2f}s")
        print(f"{'='*70}")
        
        baseline_rps = self.TARGET_RPS_10K
        baseline_p99 = self.BASELINE_P99_MS
        
        rps_ratio = (actual_rps / baseline_rps) * 100
        p99_improvement = ((baseline_p99 - p99) / baseline_p99) * 100
        
        print(f"\nvs {baseline_rps:,} RPS baseline (p99={baseline_p99}ms):")
        print(f"  RPS:  {rps_ratio:.0f}% of target ({actual_rps:,.0f} vs {baseline_rps:,})")
        print(f"  p99:  {p99_improvement:+.1f}% {'better' if p99_improvement > 0 else 'worse'} ({p99:.3f}ms vs {baseline_p99}ms)")
        
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
        
        print(f"\n{'='*70}")
        print(f"Sustained Load Test: {duration_seconds}s @ {target_rps:,} RPS")
        print(f"{'='*70}")
        
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
        
        print(f"\nSustained Load Results:")
        print(f"{'='*70}")
        print(f"  Duration:        {actual_duration:.1f}s")
        print(f"  Total requests:  {successful_requests:,}")
        print(f"  Actual RPS:      {actual_rps:,.0f}")
        print(f"  Failed:          {errors:,}")
        print(f"  Success rate:    {(successful_requests/(successful_requests+errors)*100):.2f}%")
        print(f"\nLatency (ms):")
        print(f"  Average:         {avg:.3f}")
        print(f"  p50:             {p50:.3f}")
        print(f"  p95:             {p95:.3f}")
        print(f"  p99:             {p99:.3f}")
        print(f"{'='*70}")
        
        assert actual_rps > target_rps * self.RPS_ACHIEVEMENT_THRESHOLD
        assert p99 < self.SUSTAINED_MAX_P99_MS
        assert (successful_requests / (successful_requests + errors)) > self.SUSTAINED_MIN_SUCCESS_RATE
    
    def test_burst_capacity(self, server_ready):
        burst_size = self.BURST_SIZE
        num_bursts = self.BURST_COUNT
        pause_between_bursts = self.BURST_PAUSE_SEC
        
        all_latencies = []
        
        print(f"\n{'='*70}")
        print(f"Burst Capacity Test")
        print(f"{'='*70}")
        print(f"  Burst size: {burst_size} concurrent requests")
        print(f"  Number of bursts: {num_bursts}")
        print(f"  Pause between: {pause_between_bursts}s")
        
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
            
            print(f"\nBurst #{burst_num + 1}:")
            print(f"  Duration:    {burst_duration:.2f}s")
            print(f"  Successful:  {len(latencies)}/{burst_size}")
            print(f"  Failed:      {errors}")
            print(f"  Avg latency: {avg:.2f}ms")
            print(f"  p99 latency: {p99:.2f}ms")
            
            if burst_num < num_bursts - 1:
                time.sleep(pause_between_bursts)
        
        if all_latencies:
            all_latencies.sort()
            overall_p99 = all_latencies[int(0.99 * len(all_latencies))]
            overall_avg = statistics.mean(all_latencies)
            
            print(f"\n{'='*70}")
            print(f"Overall burst statistics:")
            print(f"  Total requests:  {len(all_latencies):,}")
            print(f"  Average latency: {overall_avg:.2f}ms")
            print(f"  p99 latency:     {overall_p99:.2f}ms")
            print(f"{'='*70}")
            
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
        
        print(f"\n{'='*70}")
        print(f"Connection Pool Test")
        print(f"{'='*70}")
        print(f"  Pool size: {pool_size} connections")
        print(f"  Requests per connection: {requests_per_connection}")
        print(f"  Total requests: {total_requests:,}")
        
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
        
        print(f"\nResults:")
        print(f"{'='*70}")
        print(f"  Duration:        {duration:.2f}s")
        print(f"  Actual RPS:      {actual_rps:,.0f}")
        print(f"  Successful:      {len(latencies):,}")
        print(f"  Failed:          {errors}")
        print(f"  Success rate:    {(len(latencies)/total_requests*100):.2f}%")
        print(f"\nLatency (ms):")
        print(f"  Average:         {avg:.3f}")
        print(f"  p50:             {p50:.3f}")
        print(f"  p95:             {p95:.3f}")
        print(f"  p99:             {p99:.3f}")
        print(f"{'='*70}")
        
        assert len(latencies) > total_requests * self.POOL_MIN_SUCCESS_RATE
        assert p99 < self.POOL_MAX_P99_MS
