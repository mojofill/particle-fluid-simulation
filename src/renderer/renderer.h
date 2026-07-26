#ifndef RENDERER_H
#define RENDERER_H

#include "vk_types.h"
#include "buffer.h"
#include "swapchain.h"
#include "commands.h"
#include "compute_pipeline.h"
#include "depth_buffer.h"
#include "descriptors.h"
#include "graphics_pipeline.h"
#include "image.h"
#include <string.h>

#define MAX_FRAMES_IN_FLIGHT 2

static void initWindow(vk_context *vko);
static void framebufferResizeCallback(GLFWwindow *window, int width, int height);
static void createInstance(vk_context *vko);
static void createSurface(vk_context *vko);
static void pickPhysicalDevice(vk_context *vko);
static void findQueueFamilies(vk_context *vko);
static void createLogicalDevice(vk_context *vko);
static void createRenderpass(vk_context *vko);
static void createSyncObjects(vk_context *vko);

void drawFrame(vk_context *vko, uint32_t *currentFrame);
void initRenderer(vk_context *vko);
void createDescriptors(vk_context *vko);
void cleanupRenderer(vk_context *vko);

#endif