#include "renderer.h"
#include <stdlib.h>

#define DEBUG

static void initWindow(vk_context *vko) {
    if (!glfwInit()) {
        printf("Failed to initialize GLFW\n");
        exit(1);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // necessary to use Vulkan
    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    vko->window = glfwCreateWindow(mode->width, mode->height, "Fluid Particle Simulation", monitor, NULL);
    vko->width = mode->width;
    vko->height = mode->height;
    if (!vko->window) {
        printf("Failed to create GLFW window\n");
        exit(1);
    }

    glfwSetWindowUserPointer(vko->window, vko); // easy usage of god object within GLFW
    glfwSetFramebufferSizeCallback(vko->window, framebufferResizeCallback);
    glfwSetInputMode(vko->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

static void framebufferResizeCallback(GLFWwindow *window, int width, int height) {
    vk_context *vko = glfwGetWindowUserPointer(window);
    vko->framebufferResized = 1; // flag to update window in main loop
}

static void createInstance(vk_context *vko) {
    VkApplicationInfo appInfo = {0}; 
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan: Windows";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    
    uint32_t glfwExtensionCount;
    
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    uint32_t extensionCount = glfwExtensionCount;
    #ifdef __APPLE__
        extensionCount++;
    #endif
    const char **extensions = malloc(sizeof(char*) * extensionCount);

    for (uint32_t i = 0; i < glfwExtensionCount; i++) {
        extensions[i] = glfwExtensions[i];
    }

    #ifdef __APPLE__
        extensions[glfwExtensionCount] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
    #endif

    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = extensions;

    // validation layers

    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, NULL);

    VkLayerProperties* availableLayers =
        malloc(sizeof(VkLayerProperties) * layerCount);

    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

    int validationLayerFound = 0;

    for (uint32_t i = 0; i < layerCount; i++) {
        if (strcmp(availableLayers[i].layerName,
                "VK_LAYER_KHRONOS_validation") == 0) {
            validationLayerFound = 1;
            break;
        }
    }

    #ifdef DEBUG

    if (validationLayerFound) {
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = layers;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = NULL;
    }

    #else
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = NULL;

    #endif

    free(availableLayers);

    #ifdef __APPLE__
        createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    #endif

    VkResult res = vkCreateInstance(&createInfo, NULL, &vko->instance);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "vkCreateInstance failed: %d\n", res);
        exit(1);
    }    
}

static void createSurface(vk_context *vko) {
    if (glfwCreateWindowSurface(vko->instance, vko->window, NULL, &vko->surface) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create surface\n");
        exit(1);
    }
}

static void pickPhysicalDevice(vk_context *vko) {
    uint32_t physicalDeviceCount;
    vkEnumeratePhysicalDevices(vko->instance, &physicalDeviceCount, NULL);
    VkPhysicalDevice *physicalDevices = malloc(sizeof(VkPhysicalDevice) * physicalDeviceCount);
    vkEnumeratePhysicalDevices(vko->instance, &physicalDeviceCount, physicalDevices);

    vko->physicalDevice = physicalDevices[0]; // Pick first physical device found
    free(physicalDevices);
}

static void findQueueFamilies(vk_context *vko) {
    vkGetPhysicalDeviceQueueFamilyProperties(vko->physicalDevice, &vko->queueFamilyCount, NULL);
    VkQueueFamilyProperties *queueFamilyProperties = malloc(sizeof(VkQueueFamilyProperties) * vko->queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(vko->physicalDevice, &vko->queueFamilyCount, queueFamilyProperties);

    vko->graphicsFamilyIndex = -1;
    vko->presentFamilyIndex = -1;

    for (uint32_t i = 0; i < vko->queueFamilyCount; i++) {
        if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            vko->graphicsFamilyIndex = i;
        }

        VkBool32 presentSupport;
        vkGetPhysicalDeviceSurfaceSupportKHR(vko->physicalDevice, i, vko->surface, &presentSupport);
        if (presentSupport) {
            vko->presentFamilyIndex = i;
        }

        if (vko->graphicsFamilyIndex != -1 && vko->presentFamilyIndex != -1) break;
    }

    if (vko->graphicsFamilyIndex == -1 || vko->presentFamilyIndex == -1) {
        fprintf(stderr, "Failed to find required queue families\ngraphics family: %d\npresent family: %d", vko->graphicsFamilyIndex, vko->presentFamilyIndex);
        exit(1);
    }
}

static void createLogicalDevice(vk_context *vko) {
    uint32_t extensionCount = 1;

    #ifdef __APPLE__
        extensionCount = 2;
    #endif

    const char* deviceExtensions[2];
    deviceExtensions[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    #ifdef __APPLE__
        deviceExtensions[1] = VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME;
    #endif

    // create device queue
    float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo queueCreateInfos[2] = {0};
    queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfos[0].queueFamilyIndex = vko->graphicsFamilyIndex;
    queueCreateInfos[0].queueCount = 1;
    queueCreateInfos[0].pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures = {0};
    deviceFeatures.samplerAnisotropy = VK_TRUE; // must add device feature sampler anisotropy
    deviceFeatures.fillModeNonSolid = VK_TRUE;

    VkPhysicalDeviceVulkan12Features v12Features = {0};
    v12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    v12Features.separateDepthStencilLayouts = VK_TRUE;

    // create logical device
    VkDeviceCreateInfo deviceCreateInfo = {0};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &v12Features;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;
    deviceCreateInfo.enabledExtensionCount = extensionCount;
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    if (vkCreateDevice(vko->physicalDevice, &deviceCreateInfo, NULL, &vko->device) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create logical device\n");
        exit(1);
    }

    // must grab queue handle from logical device
    vkGetDeviceQueue(vko->device, vko->graphicsFamilyIndex, 0, &vko->graphicsQueue);
}

static void createRenderpass(vk_context *vko) {
    // (color/depth/any type of) attachment = describes how to use image view for a specific part of the render pass
    // image view = structured way to view or access the image (raw memory)
    VkAttachmentDescription colorAttachment = {0};
    colorAttachment.format = vko->surfaceFormat.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // clear previous data
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // image will be used for presentation

    // image layout = physical memory arrangement (what bytes go where)

    VkAttachmentReference colorAttachmentRef = {0}; // this is how a subpass gets access to the attachment
    colorAttachmentRef.attachment = 0; // pAttachments[0] - there is an array of attachments, our attachment index is 0 b/c we only have one attachment right now
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // depth buffer attachments
    VkAttachmentDescription depthAttachment = {0};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef = {0};
    depthAttachmentRef.attachment = 1; // links to around line 246 - VkAttachmentDescription attachments[2] = {colorAttachment, depthAttachment} - this is what render pass "references"
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

    // create the subpass. right now, we just have one subpass. this is the vertex -> fragment stuff
    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // running GRAPHICS pipeline - not a compute pipeline
    subpass.colorAttachmentCount = 1; // one SINGULAR color output, which is attachment #0. attachment = "logical" i
    subpass.pColorAttachments = &colorAttachmentRef; // array decay, for single length array just pointer to first
    
    // Uncomment this to add depth testing for 3D simulations
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency = {0};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // think negative 1 when srcSubpass is VK_SUBPASS_EXTERNAL, and max+1 when it is dstSubpass (dst = destination)
    dependency.dstSubpass = 0; // this is for the first implicit subpass from initial layout to render pass layout
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; // only when subpass 0 finished can pipeline move onto this stage
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // tells last implicit subpass to wait for subpass 0 t finish before writing to color attachment

    VkAttachmentDescription attachments[2] = {colorAttachment, depthAttachment};
    // VkAttachmentDescription *attachments = &colorAttachment;

    VkRenderPassCreateInfo renderPassInfo = {0};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 2; // only one, cuz no depth stencil
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(vko->device, &renderPassInfo, NULL, &vko->renderPass) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create render pass\n");
        exit(1);
    }
}

// No uniforms to put in. Populate these when adding uniforms to your program
// static void createDescriptorSetLayout(vk_context *vko)
// static void createCameraUniformBuffer(vk_context *vko) Rename this/create as many as necessary
// static void createDescriptorPool(vk_context *vko)
// static void createDescriptorSets(vk_context *vko)

static void createSyncObjects(vk_context *vko) {
    VkSemaphoreCreateInfo semaphoreInfo = {0};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {0};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // signal start so that main loop doesnt keep waiting for it to start

    // allocate space for the sync objects
    vko->imageAvailableSemaphores = malloc(sizeof(VkSemaphore) * vko->swapchainImageCount);
    vko->renderFinishedSemaphores = malloc(sizeof(VkSemaphore) * vko->swapchainImageCount);
    vko->inFlightFences = malloc(sizeof(VkFence) * vko->swapchainImageCount);

    for (uint32_t i = 0; i < 2; i++) {
        if (vkCreateSemaphore(vko->device, &semaphoreInfo, NULL, &vko->imageAvailableSemaphores[i]) != VK_SUCCESS || 
            vkCreateSemaphore(vko->device, &semaphoreInfo, NULL, &vko->renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(vko->device, &fenceInfo, NULL, &vko->inFlightFences[i]) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create sync objects.\n");
            exit(1);
        }
    }
}

double queryMS(uint64_t *ts, int id, float period) {
    return ((double)(ts[id*2+1] - ts[id*2])) * period / 1e6;
}

// public rendering functions
void drawFrame(vk_context *vko, uint32_t *currentFrame) {
    // wait for previous frame to finish
    vkWaitForFences(vko->device, 1, &vko->inFlightFences[*currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(vko->device, vko->swapchain, UINT64_MAX, vko->imageAvailableSemaphores[*currentFrame], VK_NULL_HANDLE, &imageIndex); // signal imageAvailableSemaphore when image is available.

    if (result == VK_ERROR_OUT_OF_DATE_KHR) { // usually happens after window resize
        recreateSwapchain(vko);
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        fprintf(stderr, "Failed to acquire swapchain image.\n");
        exit(1);
    }

    // only reset to use fence again if we are submitting work
    vkResetFences(vko->device, 1, &vko->inFlightFences[*currentFrame]);

    // if i were recording every frame, i would need to 1. vkResetCommandBuffer(commandBuffer, 0); to clear command buffer and 2. recordCommandBuffer(commandBuffer, imageIndex);
    // but since everything is pre recorded, i dont need to

    recordCommands(vko, *currentFrame, imageIndex);

    // submit commands to queue
    VkSubmitInfo submitInfo = {0};
    // explicitly tell gpu to wait until imageAvailableSemaphore signals ready before pipeline outputs to color attachment
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}; // first stage synced first semaphore.
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &vko->commandBuffers[imageIndex];
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &vko->imageAvailableSemaphores[*currentFrame]; // wait for imageAvailable semaphore to signal done before rendering
    submitInfo.pWaitDstStageMask = waitStages; // pipeline stages are synced with wait semaphores - doesnt continue onto stage until wait semaphore signals done
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &vko->renderFinishedSemaphores[*currentFrame]; // signal renderFinished semaphore when done

    if (vkQueueSubmit(vko->graphicsQueue, 1, &submitInfo, vko->inFlightFences[*currentFrame]) != VK_SUCCESS) {
        fprintf(stderr, "Failed to submit draw command buffer to graphics queue\n");
        exit(1);
    }

    // 3. Present the rendered image
    VkPresentInfoKHR presentInfo = {0};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &vko->renderFinishedSemaphores[*currentFrame]; // wait until rendering finished
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &vko->swapchain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = NULL;

    result = vkQueuePresentKHR(vko->graphicsQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || vko->framebufferResized) {
        vko->framebufferResized = 0;
        recreateSwapchain(vko);
        // rerecord command buffers
        resetCommands(vko);
        recordCommands(vko, *currentFrame, imageIndex);
    } else if (result != VK_SUCCESS) {
        printf("Failed to present swapchain image!\n");
        exit(1);
    }

    *currentFrame = (*currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    
    vkQueueWaitIdle(vko->graphicsQueue);

    if (vko->readTimeStamps && !vko->pause) {
        uint64_t timestamps[QUERY_COUNT * 2];

        vkGetQueryPoolResults(
            vko->device,
            vko->queryPool,
            0,
            QUERY_COUNT * 2,
            sizeof(timestamps),
            timestamps,
            sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT |
            VK_QUERY_RESULT_WAIT_BIT
        );

        double predictMS      = queryMS(timestamps, QUERY_PREDICT,                vko->timestampPeriod);
        double cellMS         = queryMS(timestamps, QUERY_UPDATE_CELL,            vko->timestampPeriod);
        double sortMS         = queryMS(timestamps, QUERY_SORT,                   vko->timestampPeriod);
        double scatterMS      = queryMS(timestamps, QUERY_SCATTER,                vko->timestampPeriod);
        double clearStartMS   = queryMS(timestamps, QUERY_CLEAR_START,            vko->timestampPeriod);
        double updateStartMS  = queryMS(timestamps, QUERY_UPDATE_START,           vko->timestampPeriod);
        double densityMS      = queryMS(timestamps, QUERY_DENSITY,                vko->timestampPeriod);
        double pressureMS     = queryMS(timestamps, QUERY_PRESSURE_AND_VISCOSITY, vko->timestampPeriod);
        double velocityMS     = queryMS(timestamps, QUERY_UPDATE_VELOCITY,        vko->timestampPeriod);
        double renderMS       = queryMS(timestamps, QUERY_RENDER,                 vko->timestampPeriod);

        double totalMS =
            predictMS +
            cellMS +
            sortMS +
            scatterMS +
            clearStartMS +
            updateStartMS +
            densityMS +
            pressureMS +
            velocityMS +
            renderMS;

        double fps = 1000.0 / totalMS;
        
        vko->predictMS += predictMS;
        vko->cellMS += cellMS;
        vko->sortMS += sortMS;
        vko->scatterMS += scatterMS;
        vko->clearStartMS += clearStartMS;
        vko->updateStartMS += updateStartMS;
        vko->densityMS += densityMS;
        vko->pressureMS += pressureMS;
        vko->velocityMS += velocityMS;
        vko->renderMS += renderMS;
        vko->totalFPS += fps;
        vko->totalMS += totalMS;
    }
}

float randf() {
    return (float)rand() / (RAND_MAX + 1.0f);
}

void initializeColorsBuffer(vk_context *vko) {
    vec4 *colors = malloc(vko->numParticles * sizeof(vec4));
    for (int i = 0; i < vko->numParticles; i++) {
        // use a bounding box
        vec4 color = { 1, 1, 1, 1 };
        // vec4 color = { randf(), randf(), randf(), 1 };

        glm_vec4_copy(color, colors[i]);
    }

    for (int ping = 0; ping < 2; ping++) {
        uploadDataToBufferViaStaging(
            vko,
            colors,
            vko->numParticles * sizeof(vec4),
            &vko->colorsBuffers[ping],
            &vko->colorsMemories[ping],
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            0
        );
    }

    free(colors);
}

void initializeHistogramsBuffer(vk_context *vko) {
    uint32_t *histogram = malloc(vko->numParticles * sizeof(uint32_t));
    for (int i = 0; i < vko->numParticles; i++) histogram[i] = 0;

    uploadDataToBufferViaStaging(
        vko,
        histogram,
        vko->numParticles * sizeof(uint32_t),
        &vko->histogramsBuffer,
        &vko->histogramsMemory,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        0
    );

    free(histogram);
}

void initializeEndIndicesBuffer(vk_context *vko) {
    int *endIndices = malloc(vko->numParticles * sizeof(int));
    for (int i = 0; i < vko->numParticles; i++) endIndices[i] = 0;

    uploadDataToBufferViaStaging(
        vko,
        endIndices,
        vko->numParticles * sizeof(int),
        &vko->endIndicesBuffer,
        &vko->endIndicesMemory,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        0
    );

    free(endIndices);
}

void initializePositionsBuffer(vk_context *vko) {
    vec4 *positions = malloc(vko->numParticles * sizeof(vec4));
    for (int i = 0; i < vko->numParticles; i++) {
        // use a bounding box
        float scale_x = 0.45;
        float scale_y = 0.45;
        float scale_z = 0.45;
        vec4 pos = {
            randf() * (scale_x * (vko->maxXYZ[0] - vko->minXYZ[0]) - 2 * vko->particleRadius) + vko->particleRadius + vko->minXYZ[0] + (1 - scale_x) * 0.5 * (vko->maxXYZ[0] - vko->minXYZ[0]),
            randf() * (scale_y * (vko->maxXYZ[1] - vko->minXYZ[1]) - 2 * vko->particleRadius) + vko->particleRadius + vko->minXYZ[1] + (1 - scale_y) * 0.5 * (vko->maxXYZ[1] - vko->minXYZ[1]),
            randf() * (scale_z * (vko->maxXYZ[2] - vko->minXYZ[2]) - 2 * vko->particleRadius) + vko->particleRadius + vko->minXYZ[2] + 1 * (1 - scale_z) * 0.5 * (vko->maxXYZ[2] - vko->minXYZ[2]),
            0
        };

        float s = 0;

        if (randf() < 0.5) {
            pos[0] -= s;
        }
        else {
            pos[0] += s;
        }

        glm_vec4_copy(pos, positions[i]);
    }

    for (int ping = 0; ping < 2; ping++) {
        uploadDataToBufferViaStaging(
            vko,
            positions,
            vko->numParticles * sizeof(vec4),
            &vko->positionsBuffers[ping],
            &vko->positionsMemories[ping],
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            0
        );
    }

    free(positions);
}

void initializePredictedPositionsBuffer(vk_context *vko) {
    vec4 *predictedPositions = malloc(vko->numParticles * sizeof(vec4));
    for (int i = 0; i < vko->numParticles; i++) {
        vec4 pos = { 0, 0, 0 };
        glm_vec4_copy(pos, predictedPositions[i]);
    }

    for (int ping = 0; ping < 2; ping++) {
        uploadDataToBufferViaStaging(
            vko,
            predictedPositions,
            vko->numParticles * sizeof(vec4),
            &vko->predictedPositionsBuffers[ping],
            &vko->predictedPositionsMemories[ping],
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            0
        );
    }

    free(predictedPositions);
}

void initializeVelocitiesBuffer(vk_context *vko) {
    vec4 *velocities = malloc(vko->numParticles * sizeof(vec4));
    for (int i = 0; i < vko->numParticles; i++) {
        // vec4 velocity = {
        //     10 * (2 * randf() - 1),
        //     10 * (2 * randf() - 1),
        //     10 * (2 * randf() - 1),
        //     0
        // };

        vec4 velocity = {0, 0, -60, 0};

        glm_vec4_copy(velocity, velocities[i]);
    }
    for (int ping = 0; ping < 2; ping++) {
        uploadDataToBufferViaStaging(
            vko,
            velocities,
            vko->numParticles * sizeof(vec4),
            &vko->velocitiesBuffers[ping],
            &vko->velocitiesMemories[ping],
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            0
        );
    }

    free(velocities);
}

void initializePredictedVelocitiesBuffer(vk_context *vko) {
    vec4 *predictedVelocities = malloc(vko->numParticles * sizeof(vec4));
    for (int i = 0; i < vko->numParticles; i++) {
        vec4 pos = { 0, 0, 0 };
        glm_vec4_copy(pos, predictedVelocities[i]);
    }
    for (int ping = 0; ping < 2; ping++) {
        uploadDataToBufferViaStaging(
            vko,
            predictedVelocities,
            vko->numParticles * sizeof(vec4),
            &vko->predictedVelocitiesBuffers[ping],
            &vko->predictedVelocitiesMemories[ping],
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            0
        );
    }

    free(predictedVelocities);
}

void initializeCellEntriesBuffer(vk_context *vko) {
    CellEntry *cellEntries = malloc(vko->numParticles * sizeof(CellEntry));
    for (int i = 0; i < vko->numParticles; i++) cellEntries[i] = (CellEntry) { .key = 0, .particleIndex = (uint32_t) i };
    for (int ping = 0; ping < 2; ping++) {
        uploadDataToBufferViaStaging(
            vko,
            cellEntries,
            vko->numParticles * sizeof(CellEntry),
            &vko->cellEntriesBuffers[ping],
            &vko->cellEntriesMemories[ping],
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            #ifdef TESTING
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            #else
                0
            #endif
        );
    }
    free(cellEntries);
}

void initializeNearDensitiesBuffer(vk_context *vko) {
    float *nearDensities = malloc(vko->numParticles * sizeof(float));
    for (int i = 0; i < vko->numParticles; i++) nearDensities[i] = 0;
    for (int ping = 0; ping < 2; ping++) {
        uploadDataToBufferViaStaging(
            vko,
            nearDensities,
            vko->numParticles * sizeof(float),
            &vko->nearDensitiesBuffers[ping],
            &vko->nearDensitiesMemories[ping],
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            0
        );
    }
    free(nearDensities);
}

void initializeDensitiesBuffer(vk_context *vko) {
    int *densities = malloc(vko->numParticles * sizeof(int));
    for (int i = 0; i < vko->numParticles; i++) densities[i] = 0;
    for (int ping = 0; ping < 2; ping++) {
        uploadDataToBufferViaStaging(
            vko,
            densities,
            vko->numParticles * sizeof(int),
            &vko->densitiesBuffers[ping],
            &vko->densitiesMemories[ping],
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            #ifdef TESTING
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            #else
                0
            #endif
        );
    }
    free(densities);
}

void initializeStartIndicesBuffer(vk_context *vko) {
    int *startIndices = malloc(vko->numParticles * sizeof(int));
    for (int i = 0; i < vko->numParticles; i++) startIndices[i] = 0;
    uploadDataToBufferViaStaging(
        vko,
        startIndices,
        vko->numParticles * sizeof(int),
        &vko->startIndicesBuffer,
        &vko->startIndicesMemory,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
    );
    free(startIndices);
}

void initializeStorageImage(vk_context *vko) {
    createImage( // create storage image for a fullscreen quad
        vko,
        vko->width,
        vko->height,
        1,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &vko->storageImage,
        &vko->storageImageMemory
    );
    createImageView(vko, vko->storageImage, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, &vko->storageImageView);
    createSampler(vko, &vko->storageSampler);
}

void initializeDensityMap(vk_context *vko) {
    vec3 cellSize;
    glm_vec3_sub(vko->maxXYZ, vko->minXYZ, cellSize);
    glm_vec3_scale(cellSize, 1/(float) vko->densityCellSize, cellSize);

    vko->raymarchCellSize[0] = (int) ceil(cellSize[0]);
    vko->raymarchCellSize[1] = (int) ceil(cellSize[1]);
    vko->raymarchCellSize[2] = (int) ceil(cellSize[2]);

    createImage(
        vko,
        (uint32_t) vko->raymarchCellSize[0],
        (uint32_t) vko->raymarchCellSize[1],
        (uint32_t) vko->raymarchCellSize[2],
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R32_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &vko->densityMap,
        &vko->densityMapMemory
    );
    createImageView(vko, vko->densityMap, VK_IMAGE_VIEW_TYPE_3D, VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, &vko->densityMapImageView);
    createSampler(vko, &vko->densityMapSampler);

    transitionImageLayout(
        vko,
        vko->densityMap,
        VK_FORMAT_R32_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        VK_ACCESS_SHADER_WRITE_BIT
    );
}

void initializeLightVolume(vk_context *vko) {
    vec3 volumeSize;
    glm_vec3_sub(vko->maxXYZ, vko->minXYZ, volumeSize);
    glm_vec3_scale(volumeSize, 1/(float) vko->lightMarchStepSize, volumeSize);

    vko->lightVolumeSize[0] = (int) ceil(volumeSize[0]);
    vko->lightVolumeSize[1] = (int) ceil(volumeSize[1]);
    vko->lightVolumeSize[2] = (int) ceil(volumeSize[2]);

    createImage(
        vko,
        (uint32_t) vko->lightVolumeSize[0],
        (uint32_t) vko->lightVolumeSize[1],
        (uint32_t) vko->lightVolumeSize[2],
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R32_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &vko->lightVolume,
        &vko->lightVolumeMemory
    );

    createImageView(vko, vko->lightVolume, VK_IMAGE_VIEW_TYPE_3D, VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, &vko->lightVolumeImageView);
    createSampler(vko, &vko->lightVolumeSampler);

    transitionImageLayout(
        vko,
        vko->lightVolume,
        VK_FORMAT_R32_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        VK_ACCESS_SHADER_WRITE_BIT
    );
}

static void initVulkan(vk_context *vko) {
    vko->framebufferResized = 0; // initialize framebuffer resizing flag to 0 (off)
    createInstance(vko);
    pickPhysicalDevice(vko);
    createSurface(vko);
    findQueueFamilies(vko);
    createLogicalDevice(vko);
    createSwapchain(vko);
    createImageViews(vko); // image view = vulkan interpretation of raw image source
    createQueryPool(vko);
    createCommandPool(vko);
    createUniformBuffer(vko);
    initializeStorageImage(vko);
    initializePositionsBuffer(vko);
    initializePredictedPositionsBuffer(vko);
    initializeVelocitiesBuffer(vko);
    initializePredictedVelocitiesBuffer(vko);
    initializeCellEntriesBuffer(vko);
    initializeDensitiesBuffer(vko);
    initializeStartIndicesBuffer(vko);
    initializeColorsBuffer(vko);
    initializeHistogramsBuffer(vko);
    initializeEndIndicesBuffer(vko);
    initializeNearDensitiesBuffer(vko);
    initializeDensityMap(vko);
    initializeLightVolume(vko);
    vko->initializePositionsBuffer = initializePositionsBuffer;
    vko->initializePredictedPositionsBuffer = initializePredictedPositionsBuffer;
    vko->initializeVelocitiesBuffer = initializeVelocitiesBuffer;
    vko->initializePredictedVelocitiesBuffer = initializePredictedVelocitiesBuffer;
    vko->initializeCellEntriesBuffer = initializeCellEntriesBuffer;
    vko->initializeDensitiesBuffer = initializeDensitiesBuffer;
    vko->initializeStartIndicesBuffer = initializeStartIndicesBuffer;
    vko->initializeColorsBuffer = initializeColorsBuffer;
    vko->initializeHistogramsBuffer = initializeHistogramsBuffer;
    vko->initializeEndIndicesBuffer = initializeEndIndicesBuffer;
    vko->initializeNearDensitiesBuffer = initializeNearDensitiesBuffer;
    vko->initializeDensityMap = initializeDensityMap;
    createDescriptorSetLayout(vko);
    createDescriptorPool(vko);
    createDescriptorSets(vko);
    createDepthResources(vko);
    createRenderpass(vko);
    setVertexAttributeDescriptions(vko);
    setVertexBindingDescription(vko);
    createGraphicsPipeline(vko);
    createComputePipeline(
        vko,
        &vko->updateCellEntriesComputePipelineLayout,
        &vko->updateCellEntriesComputePipeline,
        "./src/spvs/updateCellEntries.spv",
        sizeof(PushConstantsBitonicSort)
    );
    createComputePipeline(
        vko,
        &vko->bitonicSortComputePipelineLayout,
        &vko->bitonicSortComputePipeline,
        "./src/spvs/bitonicSort.spv",
        sizeof(PushConstantsBitonicSort)
    );
    createComputePipeline(
        vko,
        &vko->updateVelocitiesComputePipelineLayout,
        &vko->updateVelocitiesComputePipeline,
        "./src/spvs/updateVelocities.spv",
        sizeof(PushConstantsBitonicSort)
    );
    createComputePipeline(
        vko,
        &vko->predictPositionsComputePipelineLayout,
        &vko->predictPositionsComputePipeline,
        "./src/spvs/predictPositions.spv",
        sizeof(PushConstantsBitonicSort)
    );
    createComputePipeline(
        vko,
        &vko->calculateDensitiesComputePipelineLayout,
        &vko->calculateDensitiesComputePipeline,
        "./src/spvs/calculateDensities.spv",
        sizeof(PushConstantsBitonicSort)
    );
    createComputePipeline(
        vko,
        &vko->clearStartIndicesComputePipelineLayout,
        &vko->clearStartIndicesComputePipeline,
        "./src/spvs/clearStartIndices.spv",
        sizeof(PushConstantsBitonicSort)
    );
    createComputePipeline(
        vko,
        &vko->updateStartIndicesComputePipelineLayout,
        &vko->updateStartIndicesComputePipeline,
        "./src/spvs/updateStartIndices.spv",
        sizeof(PushConstantsBitonicSort)
    );
    createComputePipeline(
        vko,
        &vko->applyPressureAndViscosityForceComputePipelineLayout,
        &vko->applyPressureAndViscosityForceComputePipeline,
        "./src/spvs/applyPressureAndViscosityForce.spv",
        sizeof(PushConstantsBitonicSort)
    );
    createComputePipeline(
        vko,
        &vko->scatterPassComputePipelineLayout,
        &vko->scatterPassComputePipeline,
        "./src/spvs/scatterPass.spv",
        sizeof(PushConstantsBitonicSort)
    );
    createComputePipeline(
        vko,
        &vko->multiRadixSortComputePipelineLayout,
        &vko->multiRadixSortComputePipeline,
        "./src/spvs/multiRadixSort.spv",
        sizeof(PushConstantsMultiRadixSort)
    );
    createComputePipeline(
        vko,
        &vko->multiRadixSortHistogramsComputePipelineLayout,
        &vko->multiRadixSortHistogramsComputePipeline,
        "./src/spvs/multiRadixSortHistograms.spv",
        sizeof(PushConstantsMultiRadixSortHistograms)
    );
    createComputePipeline(
        vko,
        &vko->volumetricFluidComputePipelineLayout,
        &vko->volumetricFluidComputePipeline,
        "./src/spvs/volumetricFluid.spv",
        sizeof(PushConstantsBitonicSort)
    );
    createComputePipeline(
        vko,
        &vko->updateDensityMapComputePipelineLayout,
        &vko->updateDensityMapComputePipeline,
        "./src/spvs/updateDensityMap.spv",
        sizeof(PushConstantsBitonicSort)
    );
    createComputePipeline(
        vko,
        &vko->calculateLightVolumeComputePipelineLayout,
        &vko->calculateLightVolumeComputePipeline,
        "./src/spvs/calculateLightVolume.spv",
        sizeof(PushConstantsBitonicSort)
    );
    createComputePipeline(
        vko,
        &vko->properFluidComputePipelineLayout,
        &vko->properFluidComputePipeline,
        "./src/spvs/properFluid.spv",
        sizeof(PushConstantsBitonicSort)
    );
    createFramebuffers(vko);
    createCommandBuffers(vko);
    createSyncObjects(vko);
}

void cleanupRenderer(vk_context *vko) {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(vko->device, vko->imageAvailableSemaphores[i], NULL);
        vkDestroySemaphore(vko->device, vko->renderFinishedSemaphores[i], NULL);
        vkDestroyFence(vko->device, vko->inFlightFences[i], NULL);
    }
    vkDestroyCommandPool(vko->device, vko->commandPool, NULL);
    vkDestroyQueryPool(vko->device, vko->queryPool, NULL);
    vkDestroyPipeline(vko->device, vko->rasterizeGraphicsPipeline, NULL);
    vkDestroyPipeline(vko->device, vko->raymarchGraphicsPipeline, NULL);
    vkDestroyPipeline(vko->device, vko->updateCellEntriesComputePipeline, NULL);
    vkDestroyPipeline(vko->device, vko->bitonicSortComputePipeline, NULL);
    vkDestroyPipeline(vko->device, vko->updateVelocitiesComputePipeline, NULL);
    vkDestroyPipeline(vko->device, vko->predictPositionsComputePipeline, NULL);
    vkDestroyPipeline(vko->device, vko->calculateDensitiesComputePipeline, NULL);
    vkDestroyPipeline(vko->device, vko->clearStartIndicesComputePipeline, NULL);
    vkDestroyPipeline(vko->device, vko->updateStartIndicesComputePipeline, NULL);
    vkDestroyPipeline(vko->device, vko->applyPressureAndViscosityForceComputePipeline, NULL);
    vkDestroyPipeline(vko->device, vko->scatterPassComputePipeline, NULL);
    vkDestroyPipeline(vko->device, vko->multiRadixSortComputePipeline, NULL);
    vkDestroyPipeline(vko->device, vko->multiRadixSortHistogramsComputePipeline, NULL);
    vkDestroyPipeline(vko->device, vko->volumetricFluidComputePipeline, NULL);
    vkDestroyPipeline(vko->device, vko->updateDensityMapComputePipeline, NULL);
    vkDestroyPipeline(vko->device, vko->calculateLightVolumeComputePipeline, NULL);
    vkDestroyPipeline(vko->device, vko->properFluidComputePipeline, NULL);
    vkDestroyPipelineLayout(vko->device, vko->rasterizeGraphicsPipelineLayout, NULL);
    vkDestroyPipelineLayout(vko->device, vko->raymarchGraphicsPipelineLayout, NULL);
    vkDestroyPipelineLayout(vko->device, vko->updateCellEntriesComputePipelineLayout, NULL);
    vkDestroyPipelineLayout(vko->device, vko->bitonicSortComputePipelineLayout, NULL);
    vkDestroyPipelineLayout(vko->device, vko->updateVelocitiesComputePipelineLayout, NULL);
    vkDestroyPipelineLayout(vko->device, vko->predictPositionsComputePipelineLayout, NULL);
    vkDestroyPipelineLayout(vko->device, vko->calculateDensitiesComputePipelineLayout, NULL);
    vkDestroyPipelineLayout(vko->device, vko->clearStartIndicesComputePipelineLayout, NULL);
    vkDestroyPipelineLayout(vko->device, vko->updateStartIndicesComputePipelineLayout, NULL);
    vkDestroyPipelineLayout(vko->device, vko->applyPressureAndViscosityForceComputePipelineLayout, NULL);
    vkDestroyPipelineLayout(vko->device, vko->scatterPassComputePipelineLayout, NULL);
    vkDestroyPipelineLayout(vko->device, vko->multiRadixSortComputePipelineLayout, NULL);
    vkDestroyPipelineLayout(vko->device, vko->multiRadixSortHistogramsComputePipelineLayout, NULL);
    vkDestroyPipelineLayout(vko->device, vko->volumetricFluidComputePipelineLayout, NULL);
    vkDestroyPipelineLayout(vko->device, vko->updateDensityMapComputePipelineLayout, NULL);
    vkDestroyPipelineLayout(vko->device, vko->calculateLightVolumeComputePipelineLayout, NULL);
    vkDestroyPipelineLayout(vko->device, vko->properFluidComputePipelineLayout, NULL);
    vkDestroyRenderPass(vko->device, vko->renderPass, NULL);
    vkFreeMemory(vko->device, vko->fullScreenQuadVertexBufferMemory, NULL);
    vkDestroySampler(vko->device, vko->storageSampler, NULL);
    vkDestroyImage(vko->device, vko->depthImage, NULL);
    vkDestroyImageView(vko->device, vko->depthImageView, NULL);
    vkFreeMemory(vko->device, vko->depthImageMemory, NULL);
    vkDestroyImageView(vko->device, vko->storageImageView, NULL);
    vkDestroyImage(vko->device, vko->storageImage, NULL);
    vkDestroyImage(vko->device, vko->densityMap, NULL);
    vkDestroyImage(vko->device, vko->lightVolume, NULL);
    vkDestroyImageView(vko->device, vko->densityMapImageView, NULL);
    vkDestroyImageView(vko->device, vko->lightVolumeImageView, NULL);
    vkDestroySampler(vko->device, vko->densityMapSampler, NULL);
    vkDestroySampler(vko->device, vko->lightVolumeSampler, NULL);
    vkFreeMemory(vko->device, vko->storageImageMemory, NULL);
    vkFreeMemory(vko->device, vko->densityMapMemory, NULL);
    vkFreeMemory(vko->device, vko->lightVolumeMemory, NULL);
    vkDestroyBuffer(vko->device, vko->sphereMeshBuffer, NULL);
    vkFreeMemory(vko->device, vko->sphereMeshMemory, NULL);
    for (int ping = 0; ping < 2; ping++) {
        vkDestroyBuffer(vko->device, vko->positionsBuffers[ping], NULL);
        vkDestroyBuffer(vko->device, vko->velocitiesBuffers[ping], NULL);
        vkDestroyBuffer(vko->device, vko->predictedPositionsBuffers[ping], NULL);
        vkDestroyBuffer(vko->device, vko->predictedVelocitiesBuffers[ping], NULL);
        vkDestroyBuffer(vko->device, vko->densitiesBuffers[ping], NULL);
        vkDestroyBuffer(vko->device, vko->colorsBuffers[ping], NULL);
        vkDestroyBuffer(vko->device, vko->cellEntriesBuffers[ping], NULL);
        vkDestroyBuffer(vko->device, vko->nearDensitiesBuffers[ping], NULL);
        vkFreeMemory(vko->device, vko->predictedPositionsMemories[ping], NULL);
        vkFreeMemory(vko->device, vko->predictedVelocitiesMemories[ping], NULL);
        vkFreeMemory(vko->device, vko->positionsMemories[ping], NULL);
        vkFreeMemory(vko->device, vko->velocitiesMemories[ping], NULL);
        vkFreeMemory(vko->device, vko->densitiesMemories[ping], NULL);
        vkFreeMemory(vko->device, vko->colorsMemories[ping], NULL);
        vkFreeMemory(vko->device, vko->cellEntriesMemories[ping], NULL);
        vkFreeMemory(vko->device, vko->nearDensitiesMemories[ping], NULL);
    }
    vkDestroyBuffer(vko->device, vko->startIndicesBuffer, NULL);
    vkDestroyBuffer(vko->device, vko->histogramsBuffer, NULL);
    vkDestroyBuffer(vko->device, vko->endIndicesBuffer, NULL);
    vkFreeMemory(vko->device, vko->startIndicesMemory, NULL);
    vkFreeMemory(vko->device, vko->histogramsMemory, NULL);
    vkFreeMemory(vko->device, vko->endIndicesMemory, NULL);
    vkDestroyBuffer(vko->device, vko->uniformBuffer, NULL);
    vkFreeMemory(vko->device, vko->uniformBufferMemory, NULL);
    vkDestroyDescriptorPool(vko->device, vko->descriptorPool, NULL);
    vkDestroyDescriptorSetLayout(vko->device, vko->descriptorSetLayout, NULL);
    vkDestroyBuffer(vko->device, vko->fullScreenQuadVertexBuffer, NULL);
    cleanupSwapchain(vko);
    vkDestroySurfaceKHR(vko->instance, vko->surface, NULL);
    vkDestroyDevice(vko->device, NULL);
    vkDestroyInstance(vko->instance, NULL);

    glfwDestroyWindow(vko->window);
    glfwTerminate();
}

void initRenderer(vk_context *vko) {
    initWindow(vko);
    initVulkan(vko);
}
