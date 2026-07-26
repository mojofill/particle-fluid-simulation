#ifndef COMMANDS_H
#define COMMANDS_H

#define WORKGROUP_SIZE_X 128 // absolute hardware limit
#define WORKGROUP_COUNT (vko->numParticles / (WORKGROUP_SIZE_X  * 2)) // this lowkey dangerous, just dont ever change the name vko or the field numParticles

#define LOCAL_BMS 0
#define BIG_MAIN_BLOCK 1
#define LOCAL_CASCADING 2
#define BIG_CASCADING 3

#define TS_BEGIN(id) \
if (vko->readTimeStamps) vkCmdWriteTimestamp( \
    vko->commandBuffers[i], \
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, \
    vko->queryPool, \
    (id)*2)

#define TS_END(id) \
if (vko->readTimeStamps) vkCmdWriteTimestamp( \
    vko->commandBuffers[i], \
    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, \
    vko->queryPool, \
    (id)*2+1)

#include "vk_types.h"
#include "compute_pipeline.h"
#include <stdint.h>

enum {
    QUERY_PREDICT = 0,
    QUERY_UPDATE_CELL,
    QUERY_SORT,
    QUERY_SCATTER,
    QUERY_CLEAR_START,
    QUERY_UPDATE_START,
    QUERY_DENSITY,
    QUERY_PRESSURE_AND_VISCOSITY,
    QUERY_UPDATE_VELOCITY,
    QUERY_RENDER,

    QUERY_COUNT
};

void createQueryPool(vk_context *vko);
void createCommandPool(vk_context *vko);
void createCommandBuffers(vk_context *vko);
void resetCommands(vk_context *vko);
void recordCommands(vk_context *vko, uint32_t currentFrame, int imageIndex);

// exposing sorting functions for main.c to test during debug mode
void dispatch(vk_context *vko, VkCommandBuffer commandBuffer, PushConstantsBitonicSort constants, int h);
void local_bms(vk_context *vko, VkCommandBuffer commandBuffer, int h);
void big_main_block(vk_context *vko, VkCommandBuffer commandBuffer, int h);
void local_cascading_block(vk_context *vko, VkCommandBuffer commandBuffer, int h);
void big_cascading_block(vk_context *vko, VkCommandBuffer commandBuffer, int h);
void multiRadixSort(vk_context  *vko, VkCommandBuffer commandBuffer);

#endif