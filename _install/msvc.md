# Install MSVC + Clang + Ninja + vcpkg on Windows (and verify everything works)

This guide sets up a Windows-native C/C++ toolchain using MSVC, LLVM/Clang (clang-cl), Ninja, and vcpkg, all integrated through Visual Studio and usable from VS Code.

At the end:

* MSVC works
* clang-cl works (MSVC ABI)
* Ninja works
* vcpkg works (manifest mode)
* Conan + CMake are ready to plug in

---

## 1) Install MSVC (Visual Studio C++ toolchain)

### A) Install Visual Studio

Install Visual Studio Community (or Build Tools)
[https://visualstudio.microsoft.com/](https://visualstudio.microsoft.com/)

Open Visual Studio Installer → Modify

### B) Select workload

Enable:

* Desktop development with C++

### C) Required components

Ensure these are checked (names vary slightly by VS version):

* MSVC v143 C++ x64/x86 build tools
* Windows 10 SDK or Windows 11 SDK
* CMake tools for Windows
* Ninja

If cl.exe works in the developer prompt, this part is done.

---

## 2) Verify MSVC from the command line

### A) Open the correct shell

From Start Menu:

* x64 Native Tools Command Prompt for VS
  (or Developer PowerShell for VS)

### B) Verify compiler and linker

```bat
where cl
cl
where link
link
```

Expected:

* cl.exe under VC\Tools\MSVC...\bin\Hostx64\x64
* MSVC compiler banner
* MSVC linker banner

### C) Quick compile test

```bat
cd /d %TEMP%
echo int main(){return 0;} > hello.cpp
cl /nologo hello.cpp
hello.exe
```

If this runs, MSVC is correct.

---

## 3) Install Clang (clang-cl) via Visual Studio

Open Visual Studio Installer → Modify → Individual components

Enable:

* C++ Clang Compiler for Windows
* MSBuild support for LLVM (clang-cl)

Apply changes.

---

## 4) Verify Clang tools

From x64 Native Tools Command Prompt for VS:

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

* All tools resolve under Microsoft Visual Studio...\VC\Tools\Llvm...\bin
* clang-cl target shows: x86_64-pc-windows-msvc

---

## 5) vcpkg setup (important gotchas)

### A) Set VCPKG_ROOT (once, globally)

PowerShell:

```powershell
$env:VCPKG_ROOT="C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
$env:PATH="$env:VCPKG_ROOT;$env:PATH"
```

Reference:
[https://learn.microsoft.com/en-us/vcpkg/get_started/get-started-vscode?pivots=shell-powershell](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started-vscode?pivots=shell-powershell)

### B) Initialize the Visual Studio vcpkg copy

After you install or update Visual Studio with the vcpkg component checked, vcpkg is installed inside the Visual Studio installation directory.

You can run vcpkg commands from:

* Developer Command Prompt for Visual Studio
* Developer PowerShell for Visual Studio
* Terminals embedded in the IDE

Before using it with the IDE, initialize integration once:

```bat
vcpkg integrate install
```

Important notes:

* Run this command with administrator privileges.
* The Visual Studio copy of vcpkg supports manifests but does not support classic mode.
* Classic mode requires write access to the vcpkg install directory, but this copy lives in Program Files.
* If you need classic mode, clone vcpkg somewhere outside Program Files and run integrate from there.

Reference:
[https://learn.microsoft.com/en-us/vcpkg/get_started/get-started-vscode?pivots=shell-powershell](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started-vscode?pivots=shell-powershell)

---

## 6) Correct VS Code workflow (do this every time)

This is the workflow confirmed during toolchain debugging:

1. Open the developer environment (x64)
2. cd into the project folder
3. Open VS Code from that same shell

Example:

```bat
REM 1) Open: x64 Native Tools Command Prompt for VS

REM 2) Go to your repo
cd /d C:\path\to\your\project

REM 3) Launch VS Code with the toolchain environment loaded
code .
```

Reference:
[https://code.visualstudio.com/docs/cpp/config-msvc#_prerequisites](https://code.visualstudio.com/docs/cpp/config-msvc#_prerequisites)

---

## Notes

* Do NOT manually add MSVC paths to PATH
* Always use x64 Native Tools Command Prompt or VsDevCmd.bat
* Use clang-cl (not clang++) on Windows for MSVC ABI compatibility
* Ninja is recommended for fast builds
* Visual Studio vcpkg is manifest-only by design

---

## Final checklist

* MSVC installed and verified
* Clang / clang-cl / clangd / clang-tidy / clang-format installed
* Ninja installed
* vcpkg initialized and integrated
* VS Code launched from a developer shell
