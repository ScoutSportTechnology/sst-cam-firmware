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

## 4. Using Conan with CMake

Configure CMake using the generated toolchain file:

**Debug:**

```sh
cmake -S . -B build-debug \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=build-debug/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Debug
```

**Release:**

```sh
cmake -S . -B build-release \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=build-release/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release
```

**Build:**

```sh
cmake --build build-debug
```

or

```sh
cmake --build build-release
```

## 5. Notes

- Always run Conan and CMake inside MSYS2 CLANG64, not PowerShell.
- `conanfile.txt` stays at the project root.
- Profiles live under `cmake/conan/profiles/` to keep the repository organized.

