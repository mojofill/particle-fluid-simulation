#ifndef VK_TYPES_H
#define VK_TYPES_H

#include <stdlib.h>
#include <stdio.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#define GLFW_INCLUDE_NONE
// for protection against vulkan
#include <GLFW/glfw3.h>
#include "world/camera.h"

#define MAX_FRAMES_IN_FLIGHT 2

enum RenderTypes {
    RASTERIZATION,
    RAYMARCH_VOLUMETRIC,
    RAYMARCH_FLUID,
    // SCREENSPACE,
    RENDER_TYPE_COUNT
};

typedef struct CellEntry {
    uint32_t key;
    uint32_t particleIndex;
} CellEntry;

typedef struct PushConstantsBitonicSort {
    int algorithm;
    int h;
} PushConstantsBitonicSort;

typedef struct PushConstantsMultiRadixSortHistograms {
    uint32_t g_num_elements; // == NUM_ELEMENTS
    uint32_t g_shift; // (*)
    uint32_t g_num_workgroups; // == NUMBER_OF_WORKGROUPS as defined in the section above
    uint32_t g_num_blocks_per_workgroup; // == NUM_BLOCKS_PER_WORKGROUP
} PushConstantsMultiRadixSortHistograms;

typedef struct PushConstantsMultiRadixSort {
    uint32_t g_num_elements; // == NUM_ELEMENTS
    uint32_t g_shift; // (*)
    uint32_t g_num_workgroups; // == NUMBER_OF_WORKGROUPS as defined in the section above
    uint32_t g_num_blocks_per_workgroup; // == NUM_BLOCKS_PER_WORKGROUP
} PushConstantsMultiRadixSort;

typedef struct UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    int numParticles;
    float particleRadius;
    float smoothingRadius;
    float g;
    vec3 minXYZ;
    float dt;
    vec3 maxXYZ;
    float pad0;
    float particleMass;
    float pressureMultiplier;
    float targetDensity;
    float visocosityStrength;
    float volume;
    float nearPressureMultiplier;
    int width;
    int height;
    float stepSize;
    int frameCount;
    float padA;
    float padB;
    Camera camera;
    int renderType;
    float densityCellSize;
    float padC;
    float padD;
    vec3 extinctionCoefficients;
    float padE;
    vec3 dirToSun;
    float lightMarchStepSize;
    float fov;
    int numRefractions;
    float surfaceDensityThreshold;
    float indexOfRefraction;
} UniformBufferObject;

typedef struct Vertex2D {
    float pos[2];
} Vertex2D;

typedef struct Vertex3D {
    float pos[3];
    float color[3];
} Vertex3D;

typedef struct vk_context {
    // GLFW + Vulkan initialization
    GLFWwindow *window;
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkSurfaceKHR surface;
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    uint32_t queueFamilyCount;
    uint32_t graphicsFamilyIndex;
    uint32_t presentFamilyIndex;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    // Swapchain and swapchain support
    VkSurfaceFormatKHR surfaceFormat;
    VkPresentModeKHR presentMode;
    VkSwapchainKHR swapchain;
    uint32_t swapchainImageCount;
    VkImage *swapchainImages;
    VkImageView *swapchainImageViews;

    // Vertex buffer information
    VkVertexInputBindingDescription bindingDescs[2]; // works for now, in the future when using more types of vertices, need a more robust system
    VkVertexInputAttributeDescription attrDescs[3]; // come back here when adding more attributes to a vertex
    VkBuffer fullScreenQuadVertexBuffer;
    VkDeviceMemory fullScreenQuadVertexBufferMemory;

    VkBuffer sphereMeshBuffer;
    VkDeviceMemory sphereMeshMemory; // using instanced drawing

    VkBuffer positionsBuffers[2];
    VkDeviceMemory positionsMemories[2];

    // technically dont need, but just for testing:
    VkBuffer colorsBuffers[2];
    VkDeviceMemory colorsMemories[2];

    VkBuffer velocitiesBuffers[2];
    VkDeviceMemory velocitiesMemories[2];

    VkBuffer cellEntriesBuffers[2];
    VkDeviceMemory cellEntriesMemories[2];

    VkBuffer predictedPositionsBuffers[2];
    VkDeviceMemory predictedPositionsMemories[2];

    VkBuffer predictedVelocitiesBuffers[2];
    VkDeviceMemory predictedVelocitiesMemories[2];

    VkBuffer densitiesBuffers[2];
    VkDeviceMemory densitiesMemories[2];

    VkBuffer startIndicesBuffer;
    VkDeviceMemory startIndicesMemory;

    VkBuffer endIndicesBuffer;
    VkDeviceMemory endIndicesMemory;

    VkBuffer histogramsBuffer;
    VkDeviceMemory histogramsMemory;

    VkBuffer nearDensitiesBuffers[2];
    VkDeviceMemory nearDensitiesMemories[2];

    int sphereVertexCount;

    // Render pass + graphics pipeline
    VkRenderPass renderPass;
    VkPipelineLayout rasterizeGraphicsPipelineLayout;
    VkPipelineLayout raymarchGraphicsPipelineLayout;
    VkPipeline rasterizeGraphicsPipeline;
    VkPipeline raymarchGraphicsPipeline;

    // compute shaders, pipeline and layout
    VkPipelineLayout updateCellEntriesComputePipelineLayout;
    VkPipelineLayout bitonicSortComputePipelineLayout;
    VkPipelineLayout updateVelocitiesComputePipelineLayout;
    VkPipelineLayout predictPositionsComputePipelineLayout;
    VkPipelineLayout calculateDensitiesComputePipelineLayout;
    VkPipelineLayout clearStartIndicesComputePipelineLayout;
    VkPipelineLayout updateStartIndicesComputePipelineLayout;
    VkPipelineLayout applyPressureAndViscosityForceComputePipelineLayout;
    VkPipelineLayout scatterPassComputePipelineLayout;
    VkPipelineLayout multiRadixSortComputePipelineLayout;
    VkPipelineLayout multiRadixSortHistogramsComputePipelineLayout;
    VkPipelineLayout volumetricFluidComputePipelineLayout;
    VkPipelineLayout updateDensityMapComputePipelineLayout;
    VkPipelineLayout calculateLightVolumeComputePipelineLayout;
    VkPipelineLayout properFluidComputePipelineLayout;
    VkPipeline updateCellEntriesComputePipeline;
    VkPipeline bitonicSortComputePipeline;
    VkPipeline updateVelocitiesComputePipeline;
    VkPipeline predictPositionsComputePipeline;
    VkPipeline calculateDensitiesComputePipeline;
    VkPipeline clearStartIndicesComputePipeline;
    VkPipeline updateStartIndicesComputePipeline;
    VkPipeline applyPressureAndViscosityForceComputePipeline;
    VkPipeline scatterPassComputePipeline;
    VkPipeline multiRadixSortComputePipeline;
    VkPipeline multiRadixSortHistogramsComputePipeline;
    VkPipeline volumetricFluidComputePipeline;
    VkPipeline updateDensityMapComputePipeline;
    VkPipeline calculateLightVolumeComputePipeline;
    VkPipeline properFluidComputePipeline;
    VkFramebuffer *swapchainFramebuffers;

    // Descriptors (uniforms)
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSets[2];

    int width;
    int height;

    ivec3 raymarchCellSize;
    ivec3 lightVolumeSize;

    int renderType;

    // Uniform buffer
    VkBuffer uniformBuffer;
    VkDeviceMemory uniformBufferMemory;
    void *uniformBufferMapped;

    // depth buffering
    VkImage depthImage; // only need one, because only one draw operation at a time. do not need for all MAX_FRAMES_IN_FLIGHT
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    // Storage image
    VkImage storageImage;
    VkImageView storageImageView;
    VkDeviceMemory storageImageMemory;
    VkSampler storageSampler;

    // Density map
    VkImage densityMap;
    VkImageView densityMapImageView;
    VkDeviceMemory densityMapMemory;
    VkSampler densityMapSampler;

    // Light volume
    VkImage lightVolume;
    VkImageView lightVolumeImageView;
    VkDeviceMemory lightVolumeMemory;
    VkSampler lightVolumeSampler;

    // Dynamic pipeline bindings
    VkViewport viewport;
    VkRect2D scissor;

    // Synchronization objects
    VkSemaphore *imageAvailableSemaphores;
    VkSemaphore *renderFinishedSemaphores;
    VkFence *inFlightFences;
    uint8_t framebufferResized;

    // Commands
    VkCommandPool commandPool;
    VkCommandBuffer *commandBuffers;

    // Query pool
    VkQueryPool queryPool;

    Camera camera;

    int numParticles;
    int ping;

    vec3 minXYZ;
    vec3 maxXYZ;

    vec3 dirToSun;

    float particleRadius;
    int numSubsteps;
    int pause;
    int slow;
    int numBlocksPerWorkGroup;
    int readTimeStamps;
    int numFramesRan;
    int totalFPS;
    float timestampPeriod;
    float smoothingRadius;
    float densityCellSize;
    float lightMarchStepSize;

    double predictMS;
    double cellMS;
    double sortMS;
    double scatterMS;
    double clearStartMS;
    double updateStartMS;
    double densityMS;
    double pressureMS;
    double velocityMS;
    double renderMS;
    double totalMS;

    vec3 extinctionCoefficients;

    // function pointers
    void (*initializePositionsBuffer) (struct vk_context *vko);
    void (*initializePredictedPositionsBuffer) (struct vk_context *vko);
    void (*initializeVelocitiesBuffer) (struct vk_context *vko);
    void (*initializePredictedVelocitiesBuffer) (struct vk_context *vko);
    void (*initializeCellEntriesBuffer) (struct vk_context *vko);
    void (*initializeDensitiesBuffer) (struct vk_context *vko);
    void (*initializeStartIndicesBuffer) (struct vk_context *vko);
    void (*initializeColorsBuffer) (struct vk_context *vko);
    void (*initializeHistogramsBuffer) (struct vk_context *vko);
    void (*initializeEndIndicesBuffer) (struct vk_context *vko);
    void (*initializeNearDensitiesBuffer) (struct vk_context *vko);
    void (*initializeDensityMap) (struct vk_context *vko);
} vk_context;

#endif