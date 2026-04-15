# Post Office Service Simulation

A concurrent system simulation of a post office service center, implemented in C using System V IPC mechanisms (shared memory, message queues, and semaphores).

## Overview

This project simulates a multi-service customer queuing system with:
- **Director**: Main orchestrator that initializes system resources and manages the simulation lifecycle
- **Users**: Simulated customers arriving at the post office and requesting services
- **Operators**: Workers serving customers at dedicated service desks
- **Ticket Dispenser**: Automated system issuing queue tickets to arriving users

## Architecture

### Service Types

The system supports 6 different service categories:

| Service | Description |
|---------|-------------|
| `SERVICE_PACKAGES` | Package handling and shipping |
| `SERVICE_LETTERS` | Letter/mail services |
| `SERVICE_BANKING` | Banking transactions |
| `SERVICE_PAYMENTS` | Bill payments |
| `SERVICE_FINANCIAL` | Financial services |
| `SERVICE_GIFTS` | Gift-related services |

### IPC Mechanisms

| Resource | Key | Purpose |
|----------|-----|---------|
| Shared Memory (Config) | `0x0209` | Simulation parameters |
| Shared Memory (Counters) | `0x1234` | Global counters |
| Shared Memory (Workers) | `0x020A` | Operator state tracking |
| Shared Memory (Users) | `0x020B` | User information |
| Shared Memory (Stats) | `0x020C` | Simulation statistics |
| Shared Memory (Queues) | `0x020D` | Service queue states |
| Message Queue | `0x5678` | Inter-process communication |
| Semaphore Set | `0x9100` | Synchronization |

## Project Structure

```
.
├── Makefile              # Build automation
├── src/
│   ├── director.c        # Main orchestrator process
│   ├── user.c            # Customer simulation
│   ├── operator.c        # Worker process
│   ├── erogatore_ticket.c # Ticket dispenser
│   ├── headers/
│   │   ├── commons.h     # Common definitions & constants
│   │   ├── director.h    # Director interface
│   │   ├── user.h        # User process interface
│   │   ├── operator.h    # Operator interface
│   │   ├── erogatore_ticket.h
│   │   └── logger.h      # Logging utilities
│   └── config/
│       ├── config_timeout.conf   # Normal simulation config
│       └── config_explode.conf   # High-load config
├── build/                # Compiled binaries (generated)
└── log/                  # Log output directory
```

## Build Instructions

### Requirements

- GCC compiler
- POSIX-compliant OS (Linux/Unix/macOS)
- System V IPC support

### Compilation

```bash
# Build all binaries
make compile

# Build with debug symbols
make compile_debug

# Build and run with timeout config
make run_tm

# Build and run with high-load config
make run_ex

# Run with Valgrind (memory checking)
make run_valgrind

# Clean build artifacts
make clear
```

### Generated Binaries

| Binary | Description |
|--------|-------------|
| `build/director` | Main simulation controller |
| `build/user` | Customer process executable |
| `build/operator` | Worker process executable |
| `build/erogatore_ticket` | Ticket dispenser executable |

## Configuration

Configuration files define simulation parameters:

```
NOF_WORKER_SEATS   6    # Seats per service type
NOF_WORKERS       18    # Total operators
NOF_USERS        120    # Customers to simulate
NOF_PAUSE          3    # Pause duration
P_SERV_MIN        20    # Min service time
P_SERV_MAX        40    # Max service time
SIM_DURATION       5    # Simulation scale
N_NANO_SECONDS 500000  # Nanosecond precision
EXPLODE_THRESHOLD 400  # Queue overflow limit
```

## Running the Simulation

### Normal Load
```bash
make run_tm
# or
./build/director src/config/config_timeout.conf
```

### High Load (Stress Test)
```bash
make run_ex
# or
./build/director src/config/config_explode.conf
```

### Debug Mode
```bash
make run_debug
```

## System Behavior

1. **Director** initializes all IPC resources and spawns child processes
2. **Ticket Dispenser** waits for user requests and assigns ticket numbers
3. **Users** arrive randomly, request tickets, and wait for service assignment
4. **Operators** serve users based on service type and availability
5. System collects statistics on wait times, service times, and queue lengths
6. On shutdown, all IPC resources are properly cleaned up

## Signal Handling

The director handles `SIGINT` (Ctrl+C) and `SIGTERM` for graceful shutdown:
- Sets shutdown flag for child processes
- Waits for active operations to complete
- Releases all IPC resources
- Outputs final statistics

## Logging

- Debug and info logs are written to the `log/` directory
- Use `DEBUGLOG` flag for verbose output
- Valgrind integration available for memory leak detection

## Clean Up

If the simulation crashes or leaves stale IPC resources:

```bash
# View active IPC resources
ipcs -a

# Remove specific resources manually
ipcrm -M <shm_key>    # Remove shared memory
ipcrm -Q <msg_key>    # Remove message queue
ipcrm -S <sem_key>    # Remove semaphore
```

The director also includes `cleanup_stale_ipc_resources()` to handle this automatically on startup.

## Course Information

Project for **Sistemi Operativi (Operating Systems)** course, 2025.
