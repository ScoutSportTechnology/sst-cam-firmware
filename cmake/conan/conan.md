# Conan Setup and Usage (MSYS2 Clang64)

This guide explains how to install Conan and how to use the project's Conan profiles.

---

## 1. Install Conan

Inside the **MSYS2 CLANG64** terminal:

```sh
pip install --upgrade pip
pip install conan
```

Verify installation:

```sh
conan --version
```

You should see something like:

```
Conan version 2.x.x
```
## 1.1 Create a default Conan profile

Before using custom profiles, initialize Conan with a default profile:

```sh
conan profile detect --force
```

This creates a default profile that Conan uses as a baseline.

## 2. Conan profiles in this project

Profiles are stored under:

```
cmake/conan/profiles/msys2/clang64/
```

Example structure:

```
cmake/
  conan/
    profiles/
      msys2/
        clang64/
          debug.profile
          release.profile
```

Each profile configures:

- Clang 17
- C++23 standard
- libstdc++11 ABI
- Debug or Release mode
- Ninja generator
- PATH for MSYS2 CLANG64

## 3. Installing dependencies with Conan

**Debug build:**

```sh
conan install -pr cmake/conan/profiles/msys2/clang64/debug.profile -b missing .
```

**Release build:**

```sh
conan install -pr cmake/conan/profiles/msys2/clang64/release.profile -b missing .
```

This generates:

- `conan_toolchain.cmake`
- dependency files
- environment files

All inside the selected build folder.


## 5. Notes

- Always run Conan and CMake inside MSYS2 CLANG64, not PowerShell.
- `conanfile.txt` stays at the project root.
- Profiles live under `cmake/conan/profiles/` to keep the repository organized.

