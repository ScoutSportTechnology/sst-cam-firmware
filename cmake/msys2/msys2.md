# MSYS2 + Clang64 Toolchain Setup

This guide explains how to install and configure MSYS2 with the Clang64 toolchain for building the project.

---

## 1. Install MSYS2

Download MSYS2 from:

https://www.msys2.org/

Install it to the default path: `C:\msys64`.

---

## 2. Open MSYS2 CLANG64 terminal

Use:

**Start Menu → MSYS2 CLANG64**

Do not use MSYS, MinGW64, or UCRT64.

---

## 3. Update the system

```sh
pacman -Syu
```

If requested, close the terminal, reopen CLANG64, and run the command again until fully updated.

---

## 4. Install Clang toolchain and development tools

Install the Clang/LLVM toolchain bundle:

```sh
pacman -S mingw-w64-clang-x86_64-toolchain
```

Install additional tools:

```sh
pacman -S \
  mingw-w64-clang-x86_64-toolchain \
  mingw-w64-clang-x86_64-clang-tools-extra \
  mingw-w64-clang-x86_64-cmake \
  mingw-w64-clang-x86_64-ninja \
  mingw-w64-clang-x86_64-lldb \
  mingw-w64-clang-x86_64-ccache \
  mingw-w64-clang-x86_64-gdb \
  mingw-w64-clang-x86_64-python \
  mingw-w64-clang-x86_64-python-pip \
  git
```

This installs:

- clang / clang++
- clangd (language server)
- clang-format
- cmake
- ninja
- lldb
- gdb
- ccache
- python + pip

---

## 5. PATH notes (Windows)

If you always work inside MSYS2 CLANG64, no PATH changes are required.

If you want to invoke tools from PowerShell/CMD, add:

- `C:\msys64\clang64\bin`
- `C:\msys64\usr\bin` (optional)

---

## 6. Verify installation

Run:

```sh
clang --version
clangd --version
cmake --version
ninja --version
lldb --version
ccache --version
gdb --version
python --version
pip --version
```

All tools should be located under the `C:\msys64\clang64\bin` directory.