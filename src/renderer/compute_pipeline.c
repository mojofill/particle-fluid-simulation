#include "compute_pipeline.h"

void createComputePipeline(vk_context *vko, VkPipelineLayout *pComputePipelineLayout, VkPipeline *pComputePipeline, const char *compute_path, size_t pushConstantSize) {
    VkPipelineLayoutCreateInfo layoutInfo = {0};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1; // how many descriptor set layouts there are
    layoutInfo.pSetLayouts = &vko->descriptorSetLayout; // be careful with this, in future if creating multiple descriptor set layouts, need to unbind this

    VkPushConstantRange pushConstantRange = {0};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = pushConstantSize;

    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(vko->device, &layoutInfo, NULL, pComputePipelineLayout) != VK_SUCCESS) {
        printf("failed to create compute pipeline layout\n");
        exit(1);
    }

    VkShaderModule module = load_shader(vko->device, compute_path);

    VkPipelineShaderStageCreateInfo shaderStageInfo = {0};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = module;
    shaderStageInfo.pName = "main";
    
    VkComputePipelineCreateInfo pipelineInfo = {0};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = *pComputePipelineLayout;
    pipelineInfo.stage  = shaderStageInfo;

    if (vkCreateComputePipelines(vko->device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, pComputePipeline) != VK_SUCCESS) {
        printf("failed to create compute pipeline\n");
        exit(1);
    }

    vkDestroyShaderModule(vko->device, module, NULL);
}

void createImageMemoryBarrier(vk_context *vko, VkImage image, VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask) {
    VkImageMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL; // more expensive to keep switching back & forth from general to optimal
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = vko->storageImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    // synchronization settings
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;

    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask,
        dstStageMask,
        0, 0, NULL, 0, NULL, 1, &barrier
    );
}

void createBufferMemoryBarrier(vk_context *vko, VkBuffer buffer, VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask) {
    VkBufferMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer;
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask, // Wait until compute completes
        dstStageMask,   // Before vertex inputs
        0,
        0, NULL,
        1, &barrier,
        0, NULL
    );
}
