# noros (C++) — dependencies

noros links **zero ROS libraries** and pulls in **no third-party packages**.
Everything it needs ships with a standard C++ toolchain — that is the whole
point of the library.

## Required (to compile any program that uses noros)

- A **C++17** compiler — g++ ≥ 7, clang ≥ 5, or MSVC 2017+.
- The platform's **sockets + threads**, which are part of the OS/toolchain, not
  a package you install:
  - **Linux / macOS / WSL** — POSIX sockets (libc) + **pthreads** → link `-pthread`.
  - **Windows (MinGW)** — **Winsock2** → link `-lws2_32`.
  - **Windows (MSVC)** — Winsock2 is auto-linked via `#pragma comment(lib, "ws2_32")`; nothing to add.

That is the complete list. **No Boost, no ROS, no console_bridge, no message
libraries, no CMake-time codegen.**

## Optional (only to build the bundled examples the convenient way)

- **CMake ≥ 3.10** — drives `cmake -S . -B build && cmake --build build -j`.
  You do **not** need CMake to use noros: copy `include/noros.hpp` into your
  project and compile a single `.cpp` by hand, e.g.

  ```bash
  # Linux / macOS / WSL
  g++ -std=c++17 -pthread your_app.cpp noros_impl.cpp -o your_app
  # Windows, MinGW
  g++ -std=c++17 your_app.cpp noros_impl.cpp -o your_app.exe -lws2_32
  ```

## Standard-library headers used

`<atomic> <chrono> <cstdint> <cstdlib> <cstring> <functional> <map> <memory>
<mutex> <string> <thread> <vector>` plus the platform socket headers
(`<sys/socket.h>` … on POSIX, `<winsock2.h>` on Windows) — all part of a
conforming C++17 install.
