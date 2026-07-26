@echo off

mkdir src\spvs 2>nul

echo Compiling shaders...
glslc src\shaders\shader.vert -o src\spvs\vert.spv
glslc src\shaders\shader.frag -o src\spvs\frag.spv
glslc src\shaders\updateCellEntries.comp -o src\spvs\updateCellEntries.spv
glslc src\shaders\bitonicSort.comp -o src\spvs\bitonicSort.spv
glslc src\shaders\updateVelocities.comp -o src\spvs\updateVelocities.spv
glslc src\shaders\predictPositions.comp -o src\spvs\predictPositions.spv
glslc src\shaders\calculateDensities.comp -o src\spvs\calculateDensities.spv
glslc src\shaders\clearStartIndices.comp -o src\spvs\clearStartIndices.spv
glslc src\shaders\updateStartIndices.comp -o src\spvs\updateStartIndices.spv
glslc src\shaders\applyPressureAndViscosityForce.comp -o src\spvs\applyPressureAndViscosityForce.spv
glslc src\shaders\scatterPass.comp -o src\spvs\scatterPass.spv
glslc src\shaders\multiRadixSort.comp -o src\spvs\multiRadixSort.spv --target-env=vulkan1.1
glslc src\shaders\multiRadixSortHistograms.comp -o src\spvs\multiRadixSortHistograms.spv --target-env=vulkan1.1

echo Configuring CMake...
cmake -S . -B build ^
  -G "MinGW Makefiles" ^
  -DCMAKE_C_COMPILER=C:/MinGW/bin/gcc.exe ^
  -DCMAKE_MAKE_PROGRAM=C:/MinGW/bin/mingw32-make.exe ^
  -DCMAKE_BUILD_TYPE=Release ^
  -Dglfw3_DIR=C:/glfw-mingw/lib/cmake/glfw3

echo Building...
cmake --build build

echo Running...
build\vk_app.exe