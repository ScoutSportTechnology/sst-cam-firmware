#!/bin/bash

# Function to check if the script is being sourced
is_sourced() {
    if [ -n "$ZSH_VERSION" ]; then
        [[ -o sourced ]]
    elif [ -n "$BASH_VERSION" ]; then
        [[ "${BASH_SOURCE[0]}" != "$0" ]]
    else
        echo "Unsupported shell. Please use bash or zsh."
        return 1
    fi
}

# Only proceed if the script is being sourced
if is_sourced; then
    echo "🔧 Setting up StrixCam environment with Hailo support..."

    # Hailo supported versions
    REQUIRED_VERSION=("3.30.0" "3.31.0" "3.32.0")

    # Always use .venv in project root
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
    VENV_PATH="$ROOT_DIR/.venv"

    if [[ "$VIRTUAL_ENV" == "$VENV_PATH"* ]]; then
        echo "✅ You are already in the .venv virtual environment."
    else
        if [ -d "$VENV_PATH" ]; then
            echo "Activating .venv from $VENV_PATH..."
            source "$VENV_PATH/bin/activate"
        else
            echo "❌ Error: .venv not found in $ROOT_DIR. Please run 'uv venv' first."
            return 1
        fi
    fi

    # Detect installed Hailo SDK
    if pkg-config --exists hailo-tappas-core; then
        TAPPAS_VERSION=$(pkg-config --modversion hailo-tappas-core)
        TAPPAS_POST_PROC_DIR=$(pkg-config --variable=tappas_postproc_lib_dir hailo-tappas-core)
    elif pkg-config --exists hailo_tappas; then
        TAPPAS_VERSION=$(pkg-config --modversion hailo_tappas)
        TAPPAS_WORKSPACE=$(pkg-config --variable=tappas_workspace hailo_tappas)
        export TAPPAS_WORKSPACE
        echo "TAPPAS_WORKSPACE set to $TAPPAS_WORKSPACE"
        TAPPAS_POST_PROC_DIR="${TAPPAS_WORKSPACE}/apps/h8/gstreamer/libs/post_processes/"
    else
        echo "❌ Error: No Hailo TAPPAS SDK installation found."
        return 1
    fi

    # Validate version
    version_match=0
    for version in "${REQUIRED_VERSION[@]}"; do
        if [ "$TAPPAS_VERSION" = "$version" ]; then
            version_match=1
            break
        fi
    done

    if [ "$version_match" -eq 1 ]; then
        echo "✅ TAPPAS_VERSION is $TAPPAS_VERSION"
    else
        echo "❌ TAPPAS_VERSION $TAPPAS_VERSION is not supported. Required: ${REQUIRED_VERSION[*]}"
        return 1
    fi

    export TAPPAS_POST_PROC_DIR
    echo "TAPPAS_POST_PROC_DIR set to $TAPPAS_POST_PROC_DIR"

    # Get device architecture
    output=$(hailortcli fw-control identify | tr -d '\0')
    device_arch=$(echo "$output" | grep "Device Architecture" | awk -F": " '{print $2}')
    if [ -z "$device_arch" ]; then
        echo "❌ Error: Device Architecture not found. Check Hailo device connection."
        return 1
    fi

    export DEVICE_ARCHITECTURE="$device_arch"
    echo "✅ DEVICE_ARCHITECTURE is set to: $DEVICE_ARCHITECTURE"

    echo "🎉 Environment successfully set up for StrixCam + Hailo!"
else
    echo "⚠️  This script must be sourced. Use: '. hailo/setup_env.sh'"
fi
