#ifndef COMPUTE_PIPELINE_H
#define COMPUTE_PIPELINE_H

#include "vk_types.h"
#include "graphics_pipeline.h"

void createComputePipeline(vk_context *vko, VkPipelineLayout *pComputePipelineLayout, VkPipeline *pComputePipeline, const char *compute_path, size_t pushConstantSize);
void createImageMemoryBarrier(vk_context *vko, VkImage image, VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);
void createBufferMemoryBarrier(vk_context *vko, VkBuffer buffer, VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);

#endif