#!/bin/zsh

# Exit on errors, undefined vars, and failed pipes
set -euo pipefail

BUILD_DIR="build"
SPV_DIR="src/spvs"

echo "Checking dependencies..."

for cmd in cmake glslc; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "Error: '$cmd' is not installed."
        exit 1
    fi
done

echo "Creating directories..."
mkdir -p "$SPV_DIR"

echo "Compiling shaders..."
glslc src/shaders/shader.vert -o "$SPV_DIR/vert.spv"
glslc src/shaders/shader.frag -o "$SPV_DIR/frag.spv"
glslc src/shaders/updateCellEntries.comp -o "$SPV_DIR/updateCellEntries.spv"
glslc src/shaders/bitonicSort.comp -o "$SPV_DIR/bitonicSort.spv"
glslc src/shaders/updateVelocities.comp -o "$SPV_DIR/updateVelocities.spv"
glslc src/shaders/predictPositions.comp -o "$SPV_DIR/predictPositions.spv"
glslc src/shaders/calculateDensities.comp -o "$SPV_DIR/calculateDensities.spv"
glslc src/shaders/clearStartIndices.comp -o "$SPV_DIR/clearStartIndices.spv"
glslc src/shaders/updateStartIndices.comp -o "$SPV_DIR/updateStartIndices.spv"
glslc src/shaders/applyPressureAndViscosityForce.comp -o "$SPV_DIR/applyPressureAndViscosityForce.spv"
glslc src/shaders/scatterPass.comp -o "$SPV_DIR/scatterPass.spv"
glslc src/shaders/multiRadixSort.comp -o "$SPV_DIR/multiRadixSort.spv" --target-env=vulkan1.1
glslc src/shaders/raymarchShader.vert -o "$SPV_DIR/raymarchVert.spv"
glslc src/shaders/raymarchShader.frag -o "$SPV_DIR/raymarchFrag.spv"
glslc src/shaders/updateDensityMap.comp -o "$SPV_DIR/updateDensityMap.spv"
glslc src/shaders/calculateLightVolume.comp -o "$SPV_DIR/calculateLightVolume.spv"
glslc src/shaders/properFluid.comp -o "$SPV_DIR/properFluid.spv"

echo "Configuring CMake..."
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release

echo "Building..."
cmake --build "$BUILD_DIR" --parallel

echo "Running..."
EXECUTABLE="$BUILD_DIR/vk_app"

if [ ! -f "$EXECUTABLE" ]; then
    echo "Error: executable not found at $EXECUTABLE"
    exit 1
fi

"$EXECUTABLE"