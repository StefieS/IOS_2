# IOS - Project 2

Ski Bus Synchronization - A multi-process simulation using semaphores and shared memory.

## 📋 What It Does

Simulates a ski bus system where:
- A bus travels between stops and picks up skiers
- Skiers arrive at random stops and wait for the bus
- The bus has limited capacity
- All processes synchronize using semaphores

## 🔨 How to Build

Compile:
```bash
make
```

Run:
```bash
./proj2 L Z K TL TB
```

**Arguments:**
- `L` - Number of skiers (L < 20000)
- `Z` - Number of bus stops (0 < Z ≤ 10)
- `K` - Bus capacity (10 ≤ K ≤ 100)
- `TL` - Max wait time for skier arrival in microseconds (0 ≤ TL ≤ 10000)
- `TB` - Max travel time between stops in microseconds (0 ≤ TB ≤ 1000)

**Example:**
```bash
./proj2 15 3 20 100 100
```

Clean:
```bash
make clean
```

## 📄 Output

The program creates `proj2.out` with timestamped events showing:
- Bus arrivals and departures
- Skiers boarding
- Final destination arrivals

## 📁 Files

```
IOS_2/
├── proj2.c      # Main program with bus and skier processes
└── Makefile     # Build configuration
```

## 🔧 Key Features

- **Process synchronization** using POSIX semaphores
- **Shared memory** for inter-process communication
- **Fork-based** multi-process architecture
- **Random timing** for realistic simulation

## 🎓 Course Info

**Course**: IOS (Operating Systems)  
**School**: FIT VUT Brno  
**Project**: Project 2 - Process Synchronization

## ⚙️ Requirements

- GCC compiler
- POSIX semaphores
- Shared memory support
- Make

## ✍️ Author

StefieS

---

**Note**: Academic project - follow your school's academic integrity policy.
