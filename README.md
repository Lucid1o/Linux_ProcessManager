# Pulse — A Human-Centered Linux Process Manager

**Pulse** is a Linux desktop process manager built with **C++20, Qt 6, and Linux `/proc` telemetry**. Instead of presenting a long, flat list of process IDs, Pulse groups related processes into application-level views and combines process controls with a live system dashboard.

The project explores a practical systems-programming question: *How can low-level process and resource information become safer and easier to understand for desktop users?*

> **Project status:** Active development / prototype. Some behavior depends on the Linux distribution, desktop environment, permissions, and available system interfaces.

## Highlights

- **Application-centric process grouping** — combines cgroup information, process ancestry, and `.desktop` metadata to present related processes together.
- **Live system overview** — CPU, memory, swap, disk I/O, and network activity, with compact custom-drawn trend graphs.
- **Hierarchical process inspection** — explore application, parent, and child process relationships.
- **Beginner and Advanced views** — keep the default view approachable while making additional process details available when needed.
- **Process lifecycle controls** — request graceful termination or force termination, with confirmation for destructive actions.
- **Safety checks** — guard against signaling critical PIDs and check process start-time information before signaling to reduce PID-reuse risk.
- **Rule-based recommendations** — surface configurable CPU, memory, and zombie-process observations without automatically killing processes.
- **Testable architecture** — separates core process logic from Linux data collection and the Qt interface.

## Screenshots

Screenshots can be added here as the interface stabilizes. For example:

```md
![Pulse dashboard](docs/images/dashboard.png)
```

## Architecture

Pulse is organized into three main code layers plus the Linux interfaces it reads:

```text
┌─────────────────────────────────────────────┐
│                 Qt 6 UI                     │
│ Main window · Dashboard · Process tree      │
│ Recommendations · Action dialogs            │
└──────────────────────┬──────────────────────┘
                       │
┌──────────────────────▼──────────────────────┐
│                Core (C++20)                 │
│ Application grouping · Process management   │
│ Rule and recommendation engine               │
└──────────────────────┬──────────────────────┘
                       │ interfaces
┌──────────────────────▼──────────────────────┐
│              Collector layer                │
│ Linux procfs collector · Simulated collector│
└──────────────────────┬──────────────────────┘
                       │
┌──────────────────────▼──────────────────────┐
│ Linux procfs / sysfs and POSIX signals      │
└─────────────────────────────────────────────┘
```

### Main components

| Component | Responsibility |
|---|---|
| `ui/` | Qt widgets, dashboard, process tree model, and user interactions |
| `core/` | Application grouping, process lifecycle operations, and rule evaluation |
| `collector/` | System and per-process metrics collection |
| `resources/` | UI resources, styles, and default recommendation rules |
| `packaging/` | Launch and packaging helper scripts |

The core library is designed to remain independent of Qt, which makes its logic easier to test without starting the graphical interface.

## Technology

- **C++20**
- **Qt 6 Widgets**
- **CMake**
- **Ninja** (recommended build generator)
- **Linux procfs and sysfs**
- **CTest** for running the project's test suite

## Build from source

### 1. Install dependencies

On Ubuntu or Debian-based distributions:

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build qt6-base-dev
```

On Fedora:

```bash
sudo dnf install -y gcc-c++ cmake ninja-build qt6-qtbase-devel
```

Package names may differ on other distributions.

### 2. Clone the repository

```bash
git clone https://github.com/Lucid1o/Linux_ProcessManager.git
cd Linux_ProcessManager
```

If you rename the GitHub repository, use its updated clone URL.

### 3. Configure and build

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

### 4. Run Pulse

```bash
./build/ui/pulse
```

Alternatively, use the launcher:

```bash
./packaging/run_pulse.sh
```

## Run tests

Build with testing enabled (it is enabled by default in this project), then run:

```bash
ctest --test-dir build --output-on-failure
```

The tests cover core functionality such as application grouping, termination behavior, rule evaluation, and collector behavior. Test coverage and platform-specific behavior may evolve as the project develops.

## Process controls and permissions

Pulse uses POSIX process signals for lifecycle actions. A normal termination request generally uses `SIGTERM`; force termination uses `SIGKILL` after confirmation.

- You can normally manage processes owned by your user.
- Processes owned by `root` or another user may reject the request with a permission error.
- **Do not run a process manager with elevated privileges unless you understand the consequences.**
- Pulse is intended to inform the user and perform explicitly requested actions; its recommendation engine does not automatically terminate processes.

## Linux compatibility notes

Pulse relies on Linux-specific interfaces such as `/proc` and `/sys`, so Linux is the intended runtime platform. The simulated collector can support development and demonstration scenarios, but it does not make the real Linux collector portable to Windows.

Behavior can vary across distributions and desktop environments. In containers, the visible process list is limited by the container's PID namespace and permissions. Systems that do not use systemd may not expose the same application cgroup information; Pulse can fall back to other grouping heuristics.

## Project structure

```text
.
├── CMakeLists.txt
├── core/
│   ├── include/
│   ├── src/
│   └── tests/
├── collector/
│   ├── include/
│   └── src/
├── ui/
│   ├── include/
│   └── src/
├── resources/
└── packaging/
```

## Roadmap

Potential next steps:

- [ ] Add screenshots and a short demo GIF
- [ ] Improve application grouping across more desktop environments
- [ ] Add more automated tests and CI builds
- [ ] Improve accessibility and keyboard navigation
- [ ] Expand configurable system-health rules
- [ ] Explore optional container-aware process grouping

## Background and attribution

The collector uses Linux system interfaces and draws on publicly documented procfs-based monitoring techniques. The original project README identifies **btop++** as a reference for collection approaches. If code from another project is reused or adapted, retain the relevant copyright notices and license terms and document the exact files or techniques involved.

## Contributing

Contributions, bug reports, and suggestions are welcome. Before opening a pull request:

1. Keep changes focused and explain the problem being solved.
2. Build the project locally.
3. Run `ctest --test-dir build --output-on-failure`.
4. Include reproduction steps for bugs and screenshots for UI changes where useful.

## License

No license file was present in the supplied project snapshot. Until a license is added to the repository, assume that all rights are reserved and that reuse or redistribution is not automatically permitted. Add a `LICENSE` file if you intend to publish the project under an open-source license.

---

**Pulse** — clearer process visibility, more deliberate process control.
