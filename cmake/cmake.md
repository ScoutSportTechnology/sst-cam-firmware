### CMake Presets Configuration

This section describes how to configure and build the project using CMake presets. CMake presets provide predefined build configurations that simplify the build process.

## Listing Available Presets
To view all available build presets configured for this project, run:

```bash
cmake --list-presets
```

This command will display all configured profiles/presets with their descriptions and default settings.

## Setting the Profile
To configure and generate the build system with a specific preset:

```bash
cmake --preset <profile found>
```

Replace `<profile found>` with the name of the preset you want to use. For example: `cmake --preset debug` or `cmake --preset release`.

This command will:
- Configure the project with the preset's settings
- Generate the necessary build files for your configured build system
- Apply all preset-specific compiler flags, options, and environment variables

> **Note:** Always list available presets first to ensure you use a valid profile name.


## Building the Project Using the Preset
Once you have configured the project with the desired preset, you can build the project by running:

```bash
cmake --build --preset <profile found>
```

Replace `<profile found>` with the name of the preset you used during configuration. For example: `cmake --build --preset debug` or `cmake --build --preset release`.

This command will:
- Compile the source code according to the preset's configuration
- Generate the output files in the specified build directory

> **Note:** Ensure that you have successfully configured the project before attempting to build it.