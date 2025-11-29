# NDM Async Server

Асинхронный TCP/UDP сервер на C++23 с epoll-based event loop и компонентной архитектурой.

## Возможности

- Асинхронная обработка TCP и UDP подключений через epoll
- Компонентно-ориентированная архитектура (похожа на userver)
- Поддержка команд: `/time`, `/stats`, `/shutdown`
- Echo-сервер для обычных сообщений
- Graceful shutdown через SIGINT/SIGTERM с уведомлением клиентов
- **Быстрый shutdown**: eventfd для мгновенного пробуждения event loop

## Требования

- g++ 13+ с поддержкой C++23
- GNU Make
- Python 3.9+ (для тестов)
- systemd (опционально, для production deployment)

## Сборка

```bash
cd src

# Release сборка (по умолчанию)
make

# Debug сборка с sanitizers
make BUILD_TYPE=debug

# Release с debug info
make BUILD_TYPE=relwithdebinfo

# Очистка
make clean
```

## Запуск

```bash
# Дефолтные порты: TCP 8080, UDP 8040
./src/build/bin/async_server

# Кастомные порты
TCP_PORT=9000 UDP_PORT=9001 ./src/build/bin/async_server
```

## Тесты

```bash
cd tests

python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt

# Запуск всех тестов
source venv/bin/activate
pkill -9 async_server 2>/dev/null
TCP_PORT=8080 UDP_PORT=8040 ../src/build/bin/async_server &
sleep 1
pytest test_server.py -v

# Все тесты кроме benchmark
pytest test_server.py -v -k "not TestBenchmark"

# Конкретный тест
pytest test_server.py::TestTCPBasicFunctionality::test_tcp_echo_message -v

# Property-based тесты
pytest test_property_based.py -v

# Benchmark тесты
pytest test_benchmark.py -v
```

## Команды

- `/time` — текущая дата и время (формат: `YYYY-MM-DD HH:MM:SS`)
- `/stats` — статистика подключений (`Total: X, Current: Y`)
- `/shutdown` — завершение работы сервера
- Любое другое сообщение — echo обратно клиенту

## Архитектура

```
ComponentManager
├── EventLoop (epoll + eventfd для wakeup)
├── TcpListenerComponent (accept + recv/send, max 10k connections)
└── UdpListenerComponent (recvfrom/sendto)
```

Каждый компонент имеет lifecycle: `Start()` → `Run()` → `Stop()` и health checking.

## Структура проекта

```
src/
├── lib/
│   ├── component/          # Базовый интерфейс компонентов
│   ├── event_loop/         # epoll wrapper
│   ├── tcp_listener/       # TCP компонент
│   ├── udp_listener/       # UDP компонент
│   ├── component_manager/  # Оркестратор компонентов
│   ├── command/            # Обработчик команд
│   ├── socket/             # Socket wrapper
│   └── config/             # Конфигурация
├── bin/
│   └── main.cpp           # Entry point
└── Makefile
tests/
├── test_server.py         # Функциональные тесты
├── test_benchmark.py      # Нагрузочные тесты
└── test_property_based.py # Property-based тесты
```

## Примеры использования

### TCP

```bash
echo "Hello" | nc localhost 8080
# Hello

echo "/time" | nc localhost 8080
# 2025-11-29 15:30:45

echo "/stats" | nc localhost 8080
# Total: 5, Current: 1
```

### UDP

```bash
echo "Hello" | nc -u localhost 8040
# Hello

echo "/time" | nc -u localhost 8040
# 2025-11-29 15:30:45
```

## Build Types

- `release` (default): `-O3 -march=native -flto -Werror`
- `debug`: `-O0 -g3 -fsanitize=address,undefined`
- `relwithdebinfo`: `-O2 -g -march=native`
