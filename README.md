# Pulse — Application-Centric Process & System Manager for Linux

**Pulse** is a next-generation, application-centric process and system manager designed for modern Linux desktop environments. Traditional system monitors (such as `htop`, `top`, or standard task managers) present a flat, overwhelming list of hundreds of disconnected PIDs. In contrast, Pulse groups processes into semantic, user-facing applications (such as Google Chrome, Visual Studio Code, Spotify, and system background services) and provides unified application-level lifecycle controls, system telemetry sparklines, an adaptive Beginner/Advanced inspection view, and an automated rule-based recommendation engine.

---

## Architecture Diagram

Pulse enforces a strict four-layer architecture with zero leaky abstractions between the presentation, domain, and operating system layers:

```
+-------------------------------------------------------------------------+
|                                LAYER 1                                  |
|                             PULSE UI                                    |
|   (Qt 6 Widgets, C++20, Dark Theme #080A0A, Custom QPainter Sparklines) |
|   - MainWindow, DashboardWidget, SparklineWidget (NO Qt Charts)         |
|   - ProcessTreeModel (QAbstractItemModel, in-place node update)         |
|   - RecommendationsPanel, ActionDialogs (Graceful 5s countdown)         |
+-------------------------------------------------------------------------+
                                    |
                                    | Qt Signals & Slots / View Models
                                    v
+-------------------------------------------------------------------------+
|                                LAYER 2                                  |
|                            PULSE CORE                                   |
|       (Pure C++20 Static Library — ZERO Qt & GUI Dependencies)          |
|   - Application Grouping Engine (4 heuristics: cgroup, tree, .desktop)  |
|   - Process Lifecycle Manager (Tree-aware leaf-first termination)       |
|   - Safety Guards (PID 1, kernel threads, self-PID, PID reuse check)    |
|   - Rule & Insight Engine (JSON-configured, duration & state tracking)  |
+-------------------------------------------------------------------------+
                                    |
                                    | ISystemCollector & IProcessController
                                    v
+-------------------------------------------------------------------------+
|                                LAYER 3                                  |
|                           COLLECTOR LAYER                               |
|   - ISystemCollector interface                                          |
|   - LinuxProcCollector (Direct /proc & /sys parser, btop++ reference)   |
|   - Configurable proc_root (Supports offline test fixtures & mock procfs|
|   - SimulatedCollector (Living mock processes for Windows/demo mode)    |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
|                                LAYER 4                                  |
|                           LINUX SUBSYSTEM                               |
|   - /proc/stat, /proc/meminfo, /proc/diskstats, /proc/net/dev           |
|   - /proc/<pid>/stat, status, cmdline, cgroup, exe                      |
|   - POSIX signals: SIGTERM (15), SIGKILL (9), SIGSTOP (19), SIGCONT (18)|
+-------------------------------------------------------------------------+
```

---

## Core Features

### 1. Application-Centric Management & Heuristic Grouping
Pulse groups raw processes using four heuristics executed in strict priority order:
1. **systemd cgroup app scopes**: Parses `/proc/<pid>/cgroup` for `app-*.scope`, `app-flatpak-*.scope`, or `app-gnome-*.scope` (unescaping systemd hex encodings like `\x2d`).
2. **Process tree traversal**: Climbs the ancestry tree to find the topmost non-PID-1 application root within the session.
3. **Freedesktop `.desktop` matching**: Queries `.desktop` database (in `/usr/share/applications`, `~/.local/share/applications`) to map executable names to friendly display names and icons.
4. **Fallback**: Cleanly formats `/proc/<pid>/comm` or executable basename.
Displays aggregate CPU %, total memory (RSS), swap, and process count per application.

### 2. Live System Dashboard with Custom QPainter Sparklines
* Zero dependency on Qt Charts. All graphs are drawn using high-performance, anti-aliased `QPainter` widgets.
* Telemetry cards:
  - **CPU Utilization**: Total CPU % sparkline with smooth gradient fill, plus per-core mini load bars.
  - **Memory & Swap**: Used/Total RAM gauge, swap memory meter, and usage trend.
  - **Disk I/O**: Real-time read and write throughput (KB/s, MB/s).
  - **Network Traffic**: Inbound and outbound bandwidth utilization.
* Updated live every 1 second.

### 3. Hierarchical Process Tree View
* Shows `Application -> Parent Process -> Child Processes` hierarchy.
* Custom `ProcessTreeModel` performs incremental, in-place node synchronization: **expanded rows never collapse during 1-second refresh cycles**.

### 4. Application-Level Lifecycle Controls & Safety Guards
* **Close Normally**: Dispatches `SIGTERM` to the app's root process.
* **Force Close**: Prompts confirmation dialog before dispatching `SIGKILL`.
* **Close Entire Application**: Leaf-first post-order traversal terminating all children before parents (preventing orphans), featuring a 5-second graceful countdown and escalation to `SIGKILL`.
* **Safety Guards**:
  - Strictly prevents signaling PID 1 (`systemd`/`init`), PID 2 / kernel threads, or Pulse itself.
  - Checks `/proc/<pid>/stat` start timestamp before signal dispatch to prevent **PID reuse race conditions**.
  - Gracefully handles `EPERM` (permission denied) and `ESRCH` (process already exited) with informative dialogs.

### 5. Beginner / Advanced Modes
* **Beginner Mode (Default)**: Clean, clutter-free view showing Application Name, CPU %, Memory, and Process Count.
* **Advanced Mode**: Toggled via toolbar; adds PID, PPID, Threads, Priority/Nice, State, User, and full Command Line.

### 6. Rule-Based Recommendations Engine
* Reads JSON rule definitions from `~/.config/pulse/rules.json` (falls back to shipped defaults in `resources/rules.json`).
* Tracks condition durations continuously across refresh ticks.
* Default rules:
  - **High Memory**: Application consuming > 25% of RAM for > 5 minutes $\rightarrow$ recommends closing tabs or restarting.
  - **High CPU**: Application consuming > 80% CPU for > 2 minutes $\rightarrow$ recommends review.
  - **Zombie Process**: Detects defunct processes with state `'Z'` $\rightarrow$ recommends parent process health check.
* Recommendations panel allows dismissing alerts. The prototype strictly informs and **never auto-kills**.

---

## btop++ Collection Reference & Attribution

Pulse's collector layer is adapted from the Linux collection methodologies of [btop++](https://github.com/aristocratos/btop) (Copyright 2021 Jakob P. Liljenberg, Apache License 2.0).

Specific techniques adapted from btop++:
1. **CPU Usage Delta Calculation**: Reading `/proc/stat` total and idle jiffies across sampling ticks.
2. **Process CPU Percentage**: Calculating `(utime + stime)` delta divided by system total jiffies delta.
3. **Memory Metrics**: Parsing `MemAvailable`, `MemTotal`, `Buffers`, `Cached`, and `Swap` from `/proc/meminfo`.
4. **Disk I/O**: Parsing sector reads/writes from `/proc/diskstats` and converting to byte rates.
5. **Network I/O**: Parsing interface RX/TX bytes from `/proc/net/dev`.

---

## Installation & Build Instructions

### Ubuntu / Debian (22.04+)
```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build qt6-base-dev
```

### Fedora
```bash
sudo dnf install -y gcc-c++ cmake ninja-build qt6-qtbase-devel
```

### Building the Project
```bash
# Clone the repository
git clone <repo-url> pulse
cd pulse

# Configure with CMake and Ninja
cmake -S . -B build -G Ninja

# Build all targets (core library, collector, UI, and unit tests)
cmake --build build
```

### Running the Application
```bash
# Launch via direct binary
./build/ui/pulse

# Or launch via packaging script
./packaging/run_pulse.sh

# Headless / offscreen test (e.g. on CI or remote server)
QT_QPA_PLATFORM=offscreen ./build/ui/pulse
```

### Running Unit Tests
```bash
ctest --test-dir build --output-on-failure
```

---

## Packaging & Integration

* FreeDesktop integration: `packaging/pulse.desktop`
* Run scripts: `packaging/run_pulse.sh` (Linux) and `packaging/run_pulse.bat` (Windows).

---

## Known Limitations

1. **Unprivileged Process Management**: Pulse runs in user space. Signaling processes owned by `root` or other users requires elevating permissions via `pkexec` / `sudo`.
2. **Container Boundary Awareness**: When running inside Docker or Flatpak, `/proc` PID namespace isolation prevents viewing processes outside the container namespace unless `--pid=host` is granted.
3. **Non-systemd Init Systems**: On runit/OpenRC distributions without systemd, cgroup scopes (`app-*.scope`) are absent; Pulse automatically falls back to process tree climbing and `.desktop` file matching.

---

## Roadmap

* [ ] **Plugin System**: Dynamic loading of metrics collectors (GPU telemetry via NVML/ROCm, eBPF network monitors).
* [ ] **Container & Pod Awareness**: Automatic detection and grouping of Docker, Podman, and Kubernetes container scopes.
* [ ] **Remote Daemon & Agent**: Client-server architecture to manage remote Linux servers over TLS/gRPC.
* [ ] **Security Insights**: Heuristics for detecting privilege escalation, suspicious `/tmp` executable execution, and hidden processes (`/proc` hidepid).
