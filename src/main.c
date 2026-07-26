#include "renderer/renderer.h"
#include <stdlib.h>
#include <time.h>
#include <math.h>

// #define TESTING
#define READ_TIMESTAMPS
#define NUM_BLOCKS_PER_WORKGROUP (1 << 7) // this is a parameter for radix sort
// #define PAUSE_ON_START

#define NUM_PARTICLES (1 << 18)
#define PARTICLE_RADIUS 0.2
#define SMOOTHING_RADIUS 2.5
#define GRAVITY 100
#define NUM_SUBSTEPS 1
#define DELTA_TIME 0.01
#define PARTICLE_MASS 1.0f
#define PRESSURE_MULTIPLIER 15000
#define TARGET_DENSITY 1.0
#define VISCOSITY_STRENGTH 0.0

float volume; // initialize this during runtime

void updateUniforms(vk_context *vko, uint32_t currentFrame) {
    UniformBufferObject ubo = {0};
    glm_mat4_identity(ubo.model);

    Camera cam = vko->camera;

    vec3 forward;
    glm_vec3_add(cam.pos, cam.dir, forward);
    glm_lookat(cam.pos, forward, (vec3){0.0f, 0.0f, 1.0f}, ubo.view);

    float width = (float) vko->surfaceCapabilities.currentExtent.width;
    float height = (float) vko->surfaceCapabilities.currentExtent.height;
    glm_perspective(GLM_PI_4, width / height, 0.1f, 10000.0f, ubo.proj);

    ubo.proj[1][1] *= -1; // flip image upside down bc vulkan and opengl y axes are flipped

    ubo.numParticles = NUM_PARTICLES;
    ubo.particleRadius = PARTICLE_RADIUS;
    ubo.smoothingRadius = SMOOTHING_RADIUS;
    ubo.g = GRAVITY;
    ubo.dt = DELTA_TIME / (float) NUM_SUBSTEPS;
    ubo.particleMass = PARTICLE_MASS;
    ubo.pressureMultiplier = PRESSURE_MULTIPLIER;
    ubo.targetDensity = TARGET_DENSITY;
    ubo.visocosityStrength = VISCOSITY_STRENGTH;
    ubo.volume = volume;

    glm_vec3_copy(vko->minXYZ, ubo.minXYZ);
    glm_vec3_copy(vko->maxXYZ, ubo.maxXYZ);

    memcpy(vko->uniformBufferMapped, &ubo, sizeof(UniformBufferObject));
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        vk_context *vko = glfwGetWindowUserPointer(window);
        vko->initializePositionsBuffer(vko);
        vko->initializePredictedPositionsBuffer(vko);
        vko->initializeVelocitiesBuffer(vko);
        vko->initializePredictedVelocitiesBuffer(vko);
        vko->initializeCellEntriesBuffer(vko);
        vko->initializeDensitiesBuffer(vko);
        vko->initializeStartIndicesBuffer(vko);
        vko->initializeColorsBuffer(vko);
        vko->initializeHistogramsBuffer(vko);
        vko->initializeEndIndicesBuffer(vko);
    }
    else if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        vk_context *vko = glfwGetWindowUserPointer(window);
        vko->pause = vko->pause == 0 ? 1 : 0;
    }
}

void mainLoop(vk_context *vko) {
    uint32_t currentFrame = 0;

    double xpos;
    double ypos;

    glfwGetCursorPos(vko->window, &xpos, &ypos);

    vko->camera.lastX = xpos;
    vko->camera.lastY = ypos;

    glfwSetKeyCallback(vko->window, keyCallback);

    while (!glfwWindowShouldClose(vko->window)) {
        glfwPollEvents();
        if (glfwGetKey(vko->window, GLFW_KEY_ESCAPE)) {
            glfwSetWindowShouldClose(vko->window, VK_TRUE);
        }
        cameraProcessInputs(&vko->camera, vko->window);
        updateUniforms(vko, currentFrame);
        drawFrame(vko, &currentFrame);

        // vko->pause = 1;
        if (!vko->pause) vko->numFramesRan++;
    }

    printf("GPU average stats:");
    printf(
        "\n"
        "Predict:        %8.3f ms\n"
        "Cell Entries:   %8.3f ms\n"
        "Sort:           %8.3f ms\n"
        "Scatter:        %8.3f ms\n"
        "Clear Start:    %8.3f ms\n"
        "Update Start:   %8.3f ms\n"
        "Density:        %8.3f ms\n"
        "Pressure:       %8.3f ms\n"
        "Velocity:       %8.3f ms\n"
        "Render:         %8.3f ms\n"
        "--------------------------------\n"
        "Total GPU:      %8.3f ms\n"
        "GPU FPS:        %8.1f\n\n",
        vko->predictMS / (float) vko->numFramesRan,
        vko->cellMS / (float) vko->numFramesRan,
        vko->sortMS / (float) vko->numFramesRan,
        vko->scatterMS / (float) vko->numFramesRan,
        vko->clearStartMS / (float) vko->numFramesRan,
        vko->updateStartMS / (float) vko->numFramesRan,
        vko->densityMS / (float) vko->numFramesRan,
        vko->pressureMS / (float) vko->numFramesRan,
        vko->velocityMS / (float) vko->numFramesRan,
        vko->renderMS / (float) vko->numFramesRan,
        vko->totalMS / (float) vko->numFramesRan,
        vko->totalFPS / (float) vko->numFramesRan
    );
}

void initializeFullScreenQuadVertexBuffer(vk_context *vko) {
    // recall that order of vertices is important as well. need to make sure right hand rule works
    // y is flipped, so -1 is top and 1 is bottom
    Vertex2D vertices[] = {     // don't need color
        { { -1.0f, -1.0f } }, // { 1.0f, 0.0f, 0.0f } }, // top left
        { {  1.0f,  1.0f } }, // { 0.0f, 1.0f, 0.0f } }, // bottom right
        { { -1.0f,  1.0f } }, // { 0.0f, 0.0f, 1.0f } }, // bottom left
        { {  1.0f,  1.0f } }, // { 1.0f, 0.0f, 0.0f } }, // bottom right
        { { -1.0f, -1.0f } }, // { 0.0f, 1.0f, 0.0f } }, // top left
        { {  1.0f, -1.0f } }, // { 0.0f, 0.0f, 1.0f } }  // top right
    };

    uploadDataToBufferViaStaging(vko, vertices, sizeof(vertices), &vko->fullScreenQuadVertexBuffer, &vko->fullScreenQuadVertexBufferMemory, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 0);
}

void initializeSphereMeshBuffer(vk_context *vko) {
    Vertex2D sphereMeshVertices[6] = {
        {{-1, -1}},
        {{1, -1}},
        {{1, 1}},
        {{1, 1}},
        {{-1, -1}},
        {{-1, 1}},
    };

    uploadDataToBufferViaStaging(
        vko,
        sphereMeshVertices,
        sizeof(sphereMeshVertices),
        &vko->sphereMeshBuffer,
        &vko->sphereMeshMemory,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        0
    );
    
    vko->sphereVertexCount = 6;
}

void initializeCamera(vk_context *vko) {
    vko->camera = (Camera) {
        .pos = {0, -100, 10},
        .pitch = 0,
        .yaw = M_PI_2
    };

    vko->camera.dir[0] = cosf(vko->camera.pitch) * cosf(vko->camera.yaw);
    vko->camera.dir[1] = cosf(vko->camera.pitch) * sinf(vko->camera.yaw);
    vko->camera.dir[2] = sinf(vko->camera.pitch);
    glm_normalize(vko->camera.dir);

    vko->camera.lastX = vko->width/2;
    vko->camera.lastY = vko->height/2;
}

bool isPowerOfTwo(int n) {
    return (n > 0) && ((n & (n - 1)) == 0);
}

static float randf() {
    return rand() / (float) RAND_MAX;
}

void swap(CellEntry *arr, int a, int b) {
    CellEntry t = arr[a];
    arr[a].key = arr[b].key;
    arr[b].key = t.key;
}

void shuffle(CellEntry *arr, int length) {
    for (int i = 0; i < length; i++) {
        int j = (int) (randf() * length);
        swap(arr, i, j);
    }
}

void testSort(vk_context *vko, void (*sortingFunction) (vk_context*, VkCommandBuffer)) {
    updateUniforms(vko, 0);
    glfwDestroyWindow(vko->window);
    
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(vko);

    // initialize src buffer with random data

    VkBuffer srcBuffer;
    VkDeviceMemory srcBufferMemory;
    createBuffer(vko, NUM_PARTICLES * sizeof(CellEntry), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &srcBuffer, &srcBufferMemory);

    void *srcData;
    vkMapMemory(vko->device, srcBufferMemory, 0, NUM_PARTICLES * sizeof(CellEntry), 0, &srcData);
    for (int i = 0; i < NUM_PARTICLES; i++) {
        ((CellEntry*) srcData)[i] = (CellEntry) { .key = (int) (randf() * 5), .particleIndex = i };
    }

    shuffle((CellEntry*) srcData, NUM_PARTICLES);

    printf("unsorted keys:\n");
    printf("[");
    for (int i = 0; i < NUM_PARTICLES - 1; i++) {
        printf("%d, ", ((CellEntry*) srcData)[i].key);
    }
    printf("%d]\n", ((CellEntry*) srcData)[NUM_PARTICLES - 1].key);

    copyBuffer(vko, srcBuffer, vko->cellEntriesBuffers[vko->ping], NUM_PARTICLES * sizeof(CellEntry));

    vkDestroyBuffer(vko->device, srcBuffer, NULL);
    vkFreeMemory(vko->device, srcBufferMemory, NULL);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vko->bitonicSortComputePipelineLayout, 0, 1, &vko->descriptorSets[vko->ping], 0, NULL);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vko->bitonicSortComputePipeline);

    // ---------------------- HERE IS SORTING CODE --------------------------
    
    sortingFunction(vko, commandBuffer);

    // ------------------- HERE IS END OF SORTING CODE ----------------------

    // need to make sure cellEntries gets fully sorted before working on start indices
    createBufferMemoryBarrier(
        vko, vko->cellEntriesBuffers[vko->ping],
        commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT
    );

    // clear start indices
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vko->clearStartIndicesComputePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vko->clearStartIndicesComputePipelineLayout, 0, 1, &vko->descriptorSets[vko->ping], 0, NULL);
    vkCmdDispatch(commandBuffer, (vko->numParticles + 3) / 4, 1, 1);

    createBufferMemoryBarrier(
        vko,
        vko->startIndicesBuffer,
        commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT // update start indices compute shader needs to write to this after (does not read)
    );

    // then update start indices
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vko->updateStartIndicesComputePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vko->updateStartIndicesComputePipelineLayout, 0, 1, &vko->descriptorSets[vko->ping], 0, NULL);
    vkCmdDispatch(commandBuffer, (vko->numParticles + 3) / 4, 1, 1);

    endSingleTimeCommands(vko, commandBuffer);

    // now read cell entries
    commandBuffer = beginSingleTimeCommands(vko);
    VkBuffer dstBuffer;
    VkDeviceMemory dstBufferMemory;
    createBuffer(vko, NUM_PARTICLES * sizeof(CellEntry), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &dstBuffer, &dstBufferMemory);
    copyBuffer(vko, vko->cellEntriesBuffers[vko->ping], dstBuffer, NUM_PARTICLES * sizeof(CellEntry));

    void *data;
    vkMapMemory(vko->device, dstBufferMemory, 0, NUM_PARTICLES * sizeof(CellEntry), 0, &data);
    
    printf("array should be sorted based on keys.\n");
    printf("[");
    for (int i = 0; i < NUM_PARTICLES - 1; i++) {
        printf("(key=%d, particleIndex=%d), ", ((CellEntry*) data)[i].key, ((CellEntry*) data)[i].particleIndex);
    }
    printf("(key=%d, particleIndex=%d)]\n", ((CellEntry*) data)[NUM_PARTICLES - 1].key, ((CellEntry*) data)[NUM_PARTICLES - 1].particleIndex);

    printf("just keys:\n");
    printf("[");
    for (int i = 0; i < NUM_PARTICLES - 1; i++) {
        printf("%d, ", ((CellEntry*) data)[i].key);
    }
    printf("%d]\n", ((CellEntry*) data)[NUM_PARTICLES - 1].key);

    int sortedArray[NUM_PARTICLES];
    for (int i = 0; i < NUM_PARTICLES; i++) {
        sortedArray[((CellEntry*) data)[i].particleIndex] = ((CellEntry*) data)[i].key;
    }

    printf("sorted array:\n[");
    for (int i = 0; i < NUM_PARTICLES - 1; i++) {
        printf("%d, ", sortedArray[i]);
    }
    printf("%d]\n", sortedArray[NUM_PARTICLES - 1]);

    vkUnmapMemory(vko->device, dstBufferMemory);
    vkDestroyBuffer(vko->device, dstBuffer, NULL);
    vkFreeMemory(vko->device, dstBufferMemory, NULL);

    // now time to read the start indices
    createBuffer(vko, NUM_PARTICLES * sizeof(int), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &dstBuffer, &dstBufferMemory);
    copyBuffer(vko, vko->startIndicesBuffer, dstBuffer, NUM_PARTICLES * sizeof(int));

    void *startIndicesData;
    vkMapMemory(vko->device, dstBufferMemory, 0, NUM_PARTICLES * sizeof(int), 0, &startIndicesData);

    printf("start indices:\n[");
    for (int i = 0; i < NUM_PARTICLES - 1; i++) {
        printf("%d, ", ((int*) startIndicesData)[i]);
    }
    printf("%d]\n", ((int*) startIndicesData)[NUM_PARTICLES - 1]);

    vkUnmapMemory(vko->device, dstBufferMemory);
    vkDestroyBuffer(vko->device, dstBuffer, NULL);
    vkFreeMemory(vko->device, dstBufferMemory, NULL);
}

void printArray(int *arr, int length) {
    printf("[");
    for (int i = 0; i < length - 1; i++) {
        printf("%d, ", arr[i]);
    }
    printf("%d]\n", arr[length - 1]);
}

int main() {
    volume = 2.0f * M_PI * pow(SMOOTHING_RADIUS, 5) / 15.0f;

    srand(0);
    vk_context vko = {0};
    vko.numParticles = NUM_PARTICLES;
    vko.particleRadius = PARTICLE_RADIUS;
    
    float s = 50;
    vec3 minXYZ = {-1.5 * s, -0.75 * s, -1.00 * s};
    vec3 maxXYZ = {1.5 * s, 0.75 * s, 1.00 * s};
    
    vko.numSubsteps = NUM_SUBSTEPS;
    
    #ifdef PAUSE_ON_START
        vko.pause = 1;
    #else
        vko.pause = 0;
    #endif

    #ifdef READ_TIMESTAMPS
        vko.readTimeStamps = 1;
    #else
        vko.readTimeStamps = 0;
    #endif

    vko.numFramesRan = 0;

    vko.ping = 0;
    vko.numBlocksPerWorkGroup = NUM_BLOCKS_PER_WORKGROUP;

    // if (!isPowerOfTwo(NUM_PARTICLES)) {
    //     printf("NUM_PARTICLES must be a power of 2\n");
    //     exit(1);
    // }

    /*
    1. test spatial look up code for 3d (create a cellEntries buffer, where cellEntries[particleIndex] = cellKey). that belongs as its own compute shader
    2. apply gravity, (optional: viscosity), and predict next positions (into a new buffer)
    3. calculate densities
    4. calculate and apply pressure forces on velocities
    5. update positions and resolve collisions with bounding box
    */

    glm_vec3_copy(minXYZ, vko.minXYZ);
    glm_vec3_copy(maxXYZ, vko.maxXYZ);

    initializeCamera(&vko);
    initRenderer(&vko);
    initializeFullScreenQuadVertexBuffer(&vko);
    initializeSphereMeshBuffer(&vko);

    #ifdef TESTING
        testSort(&vko, multiRadixSort);
    #else
        mainLoop(&vko);
    #endif
    cleanupRenderer(&vko);
    return 0;
}
