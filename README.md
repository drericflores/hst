````markdown
# Hardware Stress Testing Tool (HST)

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C++-17-blue.svg)]()
[![Qt](https://img.shields.io/badge/Qt-5%2F6-lightgrey.svg)]()

A cross-platform **C++/Qt GUI application** for hardware stress testing and benchmarking.  
Designed and developed by **Dr. Eric O. Flores**.

---

## ✨ Features

- **Stress tests**:
  - **CPU** and **RAM** via [`stress-ng`](https://manpages.ubuntu.com/manpages/jammy/en/man1/stress-ng.1.html)
  - **GPU** via [`glmark2`](https://github.com/glmark2/glmark2)
  - **Disk** via [`fio`](https://github.com/axboe/fio)
  - **Network** via [`iperf3`](https://iperf.fr/)
- **System dashboard** with live semicircular gauges:
  - CPU utilization
  - Memory usage (used / total)
  - Root disk usage (used / total)
- **Progress monitoring**:
  - Start / Stop controls
  - ETA and progress bar
  - Safe termination of stress processes
- **Output viewer**:
  - Live console output (stdout / stderr)
  - Clear output
  - Save output to text
  - Automatic log file per run under `~/HardwareStressTest/logs`
- **Theme support**:
  - Light mode / Dark mode
  - Optional color-coded gauge captions
- **Dependency checking** (Tools → Check Dependencies)

---

## 📦 Dependencies

The GUI is written in C++17 with **Qt5 or Qt6**.  
Stress tests depend on external CLI tools.

### Runtime tools
- `stress-ng` – CPU/RAM stress
- `glmark2` – GPU benchmark
- `fio` – disk benchmark
- `iperf3` – network test

On Ubuntu / Pop!\_OS:

```bash
sudo apt install stress-ng glmark2 fio iperf3
````

### Build dependencies

* CMake ≥ 3.16
* g++ or clang with C++17
* Qt5 or Qt6 development packages

For Qt6:

```bash
sudo apt install qt6-base-dev build-essential cmake
```

For Qt5:

```bash
sudo apt install qtbase5-dev build-essential cmake
```

---

## 🔧 Build Instructions

```bash
git clone https://github.com/drericflores/hst.git
cd hst
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
./hst
```

---

## 🚀 Usage

1. Launch the application (`hst`).
2. Select a **test** (CPU, RAM, GPU, Disk, Network).
3. Configure options (workers, duration, size, server IP).
4. Click **Start** to run the test.
5. View **live output** and **system gauges**.
6. Save logs (File → Save Output As…) or open log folder.

---

## 📂 Logs

All logs are stored in:

```
~/HardwareStressTest/logs
```

Each run generates a timestamped log file with command and output.

---

## 📸 Screenshots

*(add screenshots here once available)*

---

## ⚖️ License

This project is licensed under the **MIT License**.
See [LICENSE](LICENSE) for details.

---

## 👨‍💻 Author

**Dr. Eric O. Flores**

* Precision Measurement Equipment Calibrator
* Computer Programmer, AI Engineer, Electronics Technician

> *“Professional stress testing and monitoring for Linux systems, wrapped in a simple Qt GUI.”*

---

```

---

Would you like me to also create a **`LICENSE` file (MIT)** to go with it, so GitHub automatically displays the license badge on the repo?
```
