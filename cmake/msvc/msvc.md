# Install MSVC (Visual Studio C++ toolchain) + Clang on Windows, and verify they work

This guide installs a **Windows-native** C/C++ toolchain (MSVC) and the **LLVM/Clang tools** inside Visual Studio, then shows how to test the install from the command line.

Sources:
- MSVC / Visual Studio C++ overview: Visual Studio C++ features page. :contentReference[oaicite:0]{index=0}
- Clang support for MSBuild projects: Microsoft Learn (Clang/LLVM support in Visual Studio projects). :contentReference[oaicite:1]{index=1}
- Command-line tools / using MSVC from the command line: Microsoft Learn. :contentReference[oaicite:2]{index=2}

---

## 1) Install MSVC (C++ build tools) using Visual Studio Installer

### A) Install Visual Studio (or Build Tools)
1. Install **Visual Studio Community** (or **Visual Studio Build Tools**) from the Visual Studio website (C++). :contentReference[oaicite:3]{index=3}
2. Open **Visual Studio Installer** → select your installed edition → **Modify**.

### B) Select the required workload
In **Workloads**, check:
- ✅ **Desktop development with C++** (this is the core MSVC toolchain install). :contentReference[oaicite:4]{index=4}

### C) Confirm key components (Recommended/Optional list)
Still inside the Installer (either under the workload’s optional components or “Individual components”), ensure you have:
- ✅ **MSVC v143 - VS 2022/2026 C++ x64/x86 build tools** (wording varies by VS version)
- ✅ **Windows 10 SDK or Windows 11 SDK**
- ✅ **CMake tools for Windows** (recommended)
- ✅ **Ninja** (optional but recommended)

> If you can build in a “Native Tools Command Prompt” and `cl.exe` is found, you’re good.

---

## 2) Verify MSVC is installed correctly

### A) Open the correct command prompt
Open from Start Menu:
- **x64 Native Tools Command Prompt for VS**

This prompt sets up the environment variables for MSVC command-line tools. :contentReference[oaicite:5]{index=5}

### B) Test `cl` (compiler) and `link` (linker)
Run:

```bat
where cl
cl
where link
link
```bat
where cl
cl
where link
link
```

Expected:
- `where cl` prints a path like:
  `...\VC\Tools\MSVC\...\bin\Hostx64\x64\cl.exe`
- `cl` prints the MSVC compiler banner
- `link` prints the MSVC linker banner

These confirm the MSVC compiler and linker are installed and active.

### C) Optional: quick compile test
Create and build a minimal program:

```bat
cd /d %TEMP%
echo int main(){return 0;} > hello.cpp
cl /nologo hello.cpp
hello.exe
```

If this builds and runs, MSVC is working correctly.

---

## 3) Install Clang (clang-cl) via Visual Studio Installer

Visual Studio provides official support for LLVM/Clang through optional components.

### A) Add Clang components
Open **Visual Studio Installer** → **Modify** → **Individual components**
Search for `clang` and enable:

- **C++ Clang Compiler for Windows**
- **MSBuild support for LLVM (clang-cl) toolset**

Apply the changes and let the installer finish.

---

## 4) Verify Clang tools are installed correctly

Open **x64 Native Tools Command Prompt for VS** and run:

```bat
where clang
clang --version

where clang-cl
clang-cl --version

where clang-format
clang-format --version

where clang-tidy
clang-tidy --version

where clangd
clangd --version
```

Expected:
- All tools resolve to paths under:
  `...\Microsoft Visual Studio\...\VC\Tools\Llvm\...\bin`
- `clang-cl --version` shows:
  `Target: x86_64-pc-windows-msvc`

This confirms Clang is installed correctly and targets the MSVC ABI.

---

## Notes

- Do NOT manually add MSVC paths to `PATH`
- Always use **x64 Native Tools Command Prompt for VS** or `VsDevCmd.bat`
- Clang tools installed via Visual Studio automatically integrate with MSVC

At this point:
- MSVC is installed and verified
- Clang / clang-cl / clangd / clang-tidy / clang-format are installed
- The system is ready for Conan + CMake using the MSVC ABI
