#include "image.h"

void createImage(
    vk_context *vko,
    uint32_t width,
    uint32_t height,
    uint32_t depth,
    VkImageType imageType,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkImage *image,
    VkDeviceMemory *imageMemory
) {
    if (*image != VK_NULL_HANDLE) {
        vkDestroyImage(vko->device, *image, NULL);
    }

    VkImageCreateInfo imageInfo = {0};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = imageType;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = depth;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling; // if u want access, then put VK_IMAGE_TILING_LINEAR - will be slower
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // since we are using staging buffer not staging image, can leave layout undefined
    imageInfo.usage = usage; // dst = destination during transfer, sampled bit = will be sampled by shader
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT; // only relevant for attachments (ie swapchain -> framebuffer -> render pass attachments, not relevant for texture images)

    if (vkCreateImage(vko->device, &imageInfo, NULL, image) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create image\n");
        exit(1);
    }

    // allocate space for image
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(vko->device, *image, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(vko, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(vko->device, &allocInfo, NULL, imageMemory) != VK_SUCCESS) {
        fprintf(stderr, "Failed to allocate memory for texture image\n");
        exit(1);
    }

    vkBindImageMemory(vko->device, *image, *imageMemory, 0);

    // now must transition image layout to general
    transitionImageLayout(
        vko,
        vko->storageImage,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // might need to go before, who knows
        0,
        VK_ACCESS_SHADER_READ_BIT
    );
}

void transitionImageLayout(
    vk_context *vko,
    VkImage image,
    VkFormat format,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkPipelineStageFlags sourceStage,
    VkPipelineStageFlags destinationStage,
    VkAccessFlagBits srcAccessMask,
    VkAccessFlagBits dstAccessMask
) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(vko);
    
    VkImageMemoryBarrier barrier = {0}; // must synchronize image resource sharing. make pipeline wait until image memory is fully transitioned via an image memory barrier
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // not transfering image between queues
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // aspect mask = what type/aspect of the image do you want. color bit = want rgba channels
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;

    vkCmdPipelineBarrier(
        commandBuffer, 
        sourceStage,
        destinationStage,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );

    endSingleTimeCommands(vko, commandBuffer);
}

void createImageView(vk_context *vko, VkImage image, VkImageViewType viewType, VkFormat format, VkImageAspectFlags aspectFlags, VkImageView *imageView) {
    if (*imageView != VK_NULL_HANDLE) vkDestroyImageView(vko->device, *imageView, NULL);

    VkImageViewCreateInfo viewInfo = {0};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = viewType;
    viewInfo.format = format;
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = viewType == VK_IMAGE_VIEW_TYPE_3D ? VK_REMAINING_ARRAY_LAYERS : 1;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseMipLevel = 0;

    if (vkCreateImageView(vko->device, &viewInfo, NULL, imageView) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create image view\n");
        exit(1);
    }
}

void createSampler(vk_context *vko, VkSampler *pSampler) {
    if (*pSampler != VK_NULL_HANDLE) vkDestroySampler(vko->device, *pSampler, NULL);

    VkSamplerCreateInfo samplerInfo = {0};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR; // no interpolation (no blurring)
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE; // don't need anisotropy for now (simple fullscreen quad)

    // uncomment below when anisotropy is enabled

    // VkPhysicalDeviceProperties properties = {0};
    // vkGetPhysicalDeviceProperties(vko->physicalDevice, &properties);

    // // higher anisotropy = "worse" performace, higher results
    // samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE; // want normalized [0,1) range
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    // need to change this code when enabling mip mapping. for now this is ok
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(vko->device, &samplerInfo, NULL, pSampler) != VK_SUCCESS) {
        printf("Failed to create texture image sampler\n");
        exit(1);
    }
}
