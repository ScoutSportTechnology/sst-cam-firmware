#!/bin/bash
set -e  # Exit immediately if a command exits with a non-zero status

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR/.."

# Source environment variables and activate virtual environment
echo "Sourcing environment variables and activating virtual environment..."
source "$SCRIPT_DIR/setup_env.sh"

# Install additional system dependencies (if needed)
echo "Installing additional system dependencies..."
sudo apt install -y rapidjson-dev

# Initialize variables
DOWNLOAD_RESOURCES_FLAG=""
PYHAILORT_WHL=""
PYTAPPAS_WHL=""
INSTALL_TEST_REQUIREMENTS=false
TAG="25.3.1"

# Parse command-line arguments
while [[ "$#" -gt 0 ]]; do
    case $1 in
        --pyhailort) PYHAILORT_WHL="$2"; shift ;;
        --pytappas) PYTAPPAS_WHL="$2"; shift ;;
        --test) INSTALL_TEST_REQUIREMENTS=true ;;
        --all) DOWNLOAD_RESOURCES_FLAG="--all" ;;
        --tag) TAG="$2"; shift ;;
        *) echo "Unknown parameter passed: $1"; exit 1 ;;
    esac
    shift
done

# Install specified Python wheels
if [[ -n "$PYHAILORT_WHL" ]]; then
    echo "Installing pyhailort wheel: $PYHAILORT_WHL"
    uv pip install "$PYHAILORT_WHL"
fi

if [[ -n "$PYTAPPAS_WHL" ]]; then
    echo "Installing pytappas wheel: $PYTAPPAS_WHL"
    uv pip install "$PYTAPPAS_WHL"
fi

# Install the required Python dependencies
echo "Installing required Python dependencies..."
uv pip install -r "$SCRIPT_DIR/requirements.txt"

# Install Hailo Apps Infrastructure from specified tag/branch
echo "Installing Hailo Apps Infrastructure from version: $TAG..."
uv pip install --no-build-isolation "git+https://github.com/hailo-ai/hailo-apps-infra.git@$TAG"

# Install test requirements if needed
if [[ "$INSTALL_TEST_REQUIREMENTS" == true ]]; then
    echo "Installing test requirements..."
    uv pip install -r "$SCRIPT_DIR/tests/test_resources/requirements.txt"
fi

# Download resources needed for the pipelines
if [ -f "$SCRIPT_DIR/download_resources.sh" ]; then
    echo "Downloading resources needed for the pipelines..."
    "$SCRIPT_DIR/download_resources.sh" $DOWNLOAD_RESOURCES_FLAG
else
    echo "⚠️  download_resources.sh not found. Skipping."
fi

echo "✅ Installation completed successfully."
