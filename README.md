# C++ Bindings for Windows

[![Build](https://github.com/Serial-IO/cpp-bindings-windows/actions/workflows/build_binary.yml/badge.svg)](https://github.com/Serial-IO/cpp-bindings-windows/actions/workflows/build_binary.yml)
[![JSR](https://jsr.io/badges/@serial/cpp-bindings-windows)](https://jsr.io/@serial/cpp-bindings-windows)

Windows DLL for serial communication. It implements the
[`cpp-core`](https://github.com/Serial-IO/cpp-core) interface and provides functions for discovering, monitoring,
opening, configuring, reading from, and writing to serial ports.

## Requirements

- CMake 3.30 or newer
- Git
- A compiler with C++23 support
- One of:
  - Windows with Visual Studio 2022 and the C++ workload
  - Linux with an x86-64 MinGW-w64 toolchain for cross-compilation

CMake downloads `cpp-core` and GoogleTest automatically during configuration.

## Build on Windows

```powershell
git clone https://github.com/Serial-IO/cpp-bindings-windows.git
cd cpp-bindings-windows
cmake --preset windows-vs-release
cmake --build --preset windows-vs-release --config Release --target cpp_bindings_windows
```

The DLL is written below `build/Release/`.

Official release and JSR artifacts currently target `x86_64-windows-msvc`.
Release DLLs statically include the MSVC runtime and expose the complete C API
described by `cpp-core` 2.0.1.

## Cross-compile with MinGW

The MinGW preset provides a local compile and link check from Linux:

```sh
cmake --preset windows-mingw-release
cmake --build --preset windows-mingw-release \
  --target cpp_bindings_windows cpp_bindings_windows_tests
```

The DLL and test executable are written to `build/mingw/`. The tests must be
run on Windows (or in a compatible Windows runtime); cross-compilation alone
does not execute them.

## Tests

Build and run the C++ suite on Windows:

```powershell
cmake --build --preset windows-vs-release --config Release --target cpp_bindings_windows_tests
ctest --test-dir build -C Release --output-on-failure
```

Tests that require a serial device use `SERIAL_TEST_PORT` and are skipped when
no suitable device is available.

The optional Deno FFI smoke tests require Deno 2 and a built DLL:

```powershell
cd integration_tests
deno task test
```

## FFI metadata

Release and JSR packages include `x86_64-windows-msvc` API metadata generated
from the public `cpp-core` headers with
[ASTrein](https://github.com/Katze719/ASTrein). It describes exported symbols,
types, callbacks, default values, and API documentation for downstream FFI
adapter generators.

## License

This project is licensed under the [GNU Lesser General Public License v3.0](LICENSE).
