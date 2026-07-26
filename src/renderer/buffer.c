#include "buffer.h"
#include <string.h>

uint32_t findMemoryType(vk_context *vko, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(vko->physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    fprintf(stderr, "Failed to find suitable memory\n");
    exit(1);
}

// create buffer + allocates memory, does not map/upload data
void createBuffer(vk_context *vko, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer *pBuffer, VkDeviceMemory *pBufferMemory) {
    VkBufferCreateInfo bufferInfo = {0};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size; // for now, 3 vertices
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (pBuffer == NULL) {
        printf("bad buffer pointer\n");
        exit(1);
    }

    if (vkCreateBuffer(vko->device, &bufferInfo, NULL, pBuffer) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create vertex buffer\n");
        exit(1);
    }

    // allocate memory for buffer
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(vko->device, *pBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(vko, memRequirements.memoryTypeBits, properties);
    
    if (vkAllocateMemory(vko->device, &allocInfo, NULL, pBufferMemory) != VK_SUCCESS) {
        fprintf(stderr, "Failed to allocate memory for vertex buffer\n");
        exit(1);
    }

    vkBindBufferMemory(vko->device, *pBuffer, *pBufferMemory, 0);
}

VkCommandBuffer beginSingleTimeCommands(vk_context *vko) {
    VkCommandBufferAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = vko->commandPool; // may want to create separate command pool for short lived commands
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(vko->device, &allocInfo, &commandBuffer);

    // begin recording copy command
    VkCommandBufferBeginInfo beginInfo = {0};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // only using buffer once, may help give vulkan optimizations

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void endSingleTimeCommands(vk_context *vko, VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer); // end recording

    // must submit command
    VkSubmitInfo submitInfo = {0};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(vko->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(vko->graphicsQueue); // BAD! do not do this.
    // printf("(queue wait single time cmd) elapsed ms: %f\n", elapsed_ms);

    // one time use, thus immediately free buffer

    vkFreeCommandBuffers(vko->device, vko->commandPool, 1, &commandBuffer);
}

void copyBuffer(vk_context *vko, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    // must create, record, and submit a copy buffer command
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(vko);

    VkBufferCopy copyRegion = {0};
    copyRegion.size = size;

    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    
    endSingleTimeCommands(vko, commandBuffer);
}

void uploadDataToBufferViaStaging(vk_context *vko, void *srcData, size_t size, VkBuffer *pBuffer, VkDeviceMemory *pMemory, VkBufferUsageFlagBits usageFlags, int additionalMemoryFlags) {
    // staging
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    // staging buffer must be host visible and host coherent so it can be copied
    createBuffer(vko, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingMemory);

    void *data;
    vkMapMemory(vko->device, stagingMemory, 0, size, 0, &data);
    memcpy(data, srcData, size);
    vkUnmapMemory(vko->device, stagingMemory);

    // actual buffer
    if (*pBuffer == VK_NULL_HANDLE) createBuffer(vko, size, usageFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | additionalMemoryFlags, pBuffer, pMemory);

    // copy from staging to test buffer
    copyBuffer(vko, stagingBuffer, *pBuffer, size);

    // destroy staging buffer
    vkDestroyBuffer(vko->device, stagingBuffer, NULL);
    vkFreeMemory(vko->device, stagingMemory, NULL);
}
