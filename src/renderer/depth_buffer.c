#include "depth_buffer.h"

void createDepthResources(vk_context *vko) {
    // use VK_FORMAT_D32_SFLOAT, where D stands for depth buffer specific data formats

    // going to use VK_FORMAT_D32_SFLOAT
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    createImage(vko, 
        vko->surfaceCapabilities.currentExtent.width, 
        vko->surfaceCapabilities.currentExtent.height,
        depthFormat, 
        VK_IMAGE_TILING_OPTIMAL, 
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
        &vko->depthImage, &vko->depthImageMemory
    );

    createImageView(vko, vko->depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, &vko->depthImageView);
}
