#include "commands.h"

void createQueryPool(vk_context *vko) {
    VkQueryPoolCreateInfo queryInfo = {0};
    queryInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryInfo.queryCount = 32; // might need more, might need less, who knows

    if (vkCreateQueryPool(vko->device, &queryInfo, NULL, &vko->queryPool) != VK_SUCCESS) {
        printf("Failed to create query pool\n");
        exit(1);
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(vko->physicalDevice, &props);

    vko->timestampPeriod = props.limits.timestampPeriod;
}

void createCommandPool(vk_context *vko) {
    VkCommandPoolCreateInfo poolInfo = {0};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = vko->graphicsFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(vko->device, &poolInfo, NULL, &vko->commandPool) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create command pool\n");
        exit(1);
    }
}

void createCommandBuffers(vk_context *vko) {
    VkCommandBufferAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = vko->commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    vko->commandBuffers = malloc(sizeof(VkCommandBuffer) * vko->swapchainImageCount);

    for (uint32_t i = 0; i < vko->swapchainImageCount; i++) {
        vkAllocateCommandBuffers(vko->device, &allocInfo, &vko->commandBuffers[i]);
    }
}

void resetCommands(vk_context *vko) {
    for (uint32_t i = 0; i < vko->swapchainImageCount; i++) {
        vkResetCommandBuffer(vko->commandBuffers[i], 0);
    }
}

void dispatch(vk_context *vko, VkCommandBuffer commandBuffer, PushConstantsBitonicSort constants, int h) {
    constants.h = h;
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vko->bitonicSortComputePipelineLayout, 0, 1, &vko->descriptorSets[vko->ping], 0, NULL);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vko->bitonicSortComputePipeline);
    vkCmdPushConstants(commandBuffer, vko->bitonicSortComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsBitonicSort), &constants);
    vkCmdDispatch(commandBuffer, WORKGROUP_COUNT, 1, 1);
    createBufferMemoryBarrier(vko, vko->cellEntriesBuffers[vko->ping], commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
}

void local_bms(vk_context *vko, VkCommandBuffer commandBuffer, int h) {
    PushConstantsBitonicSort constants = {0};
    constants.algorithm = LOCAL_BMS;
    dispatch(vko, commandBuffer, constants, h);
}

void big_main_block(vk_context *vko, VkCommandBuffer commandBuffer, int h) {
    PushConstantsBitonicSort constants = {0};
    constants.algorithm = BIG_MAIN_BLOCK;
    dispatch(vko, commandBuffer, constants, h);
}

void local_cascading_block(vk_context *vko, VkCommandBuffer commandBuffer, int h) {
    PushConstantsBitonicSort constants = {0};
    constants.algorithm = LOCAL_CASCADING;
    dispatch(vko, commandBuffer, constants, h);
}

void big_cascading_block(vk_context *vko, VkCommandBuffer commandBuffer, int h) {
    PushConstantsBitonicSort constants = {0};
    constants.algorithm = BIG_CASCADING;
    dispatch(vko, commandBuffer, constants, h);
}

void bitonicSort(vk_context *vko, VkCommandBuffer commandBuffer) {
    int h = WORKGROUP_SIZE_X * 2;
    
    local_bms(vko, commandBuffer, h);

    // we must now double h, as this happens before every flip
    h *= 2;

    for ( ; h <= vko->numParticles; h *= 2) {
        big_main_block(vko, commandBuffer, h);

        for (int hh = h / 2; hh > 1; hh /= 2 ) {
            if (hh <= WORKGROUP_SIZE_X * 2) {
                local_cascading_block(vko, commandBuffer, hh);
            } else {
                big_cascading_block(vko, commandBuffer, hh);
            }
        }
    }
}

void multiRadixSort(vk_context  *vko, VkCommandBuffer commandBuffer) {
    for (int i = 0; i < 4; i++) {
        uint32_t globalInvocationSize = vko->numParticles / vko->numBlocksPerWorkGroup;
        uint32_t remainder = vko->numParticles % vko->numBlocksPerWorkGroup;
        globalInvocationSize += remainder > 0 ? 1 : 0;

        uint32_t elementsPerWorkgroup = WORKGROUP_SIZE_X * vko->numBlocksPerWorkGroup;

        uint32_t numWorkgroups = (vko->numParticles + elementsPerWorkgroup - 1) / elementsPerWorkgroup;

        globalInvocationSize = numWorkgroups;

        PushConstantsMultiRadixSortHistograms hconstants = {0};
        hconstants.g_num_elements = vko->numParticles;
        hconstants.g_shift = i * 8;
        hconstants.g_num_workgroups = numWorkgroups;
        hconstants.g_num_blocks_per_workgroup = vko->numBlocksPerWorkGroup;
        vkCmdPushConstants(commandBuffer, vko->multiRadixSortHistogramsComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsMultiRadixSortHistograms), &hconstants);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vko->multiRadixSortHistogramsComputePipelineLayout, 0, 1, &vko->descriptorSets[vko->ping], 0, NULL);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vko->multiRadixSortHistogramsComputePipeline);
        vkCmdDispatch(commandBuffer, globalInvocationSize, 1, 1);

        createBufferMemoryBarrier(
            vko,
            vko->histogramsBuffer,
            commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT
        );

        PushConstantsMultiRadixSort rconstants = {0};
        rconstants.g_num_elements = vko->numParticles;
        rconstants.g_shift = i * 8;
        rconstants.g_num_workgroups = numWorkgroups;
        rconstants.g_num_blocks_per_workgroup = vko->numBlocksPerWorkGroup;
        vkCmdPushConstants(commandBuffer, vko->multiRadixSortComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsMultiRadixSort), &rconstants);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vko->multiRadixSortComputePipelineLayout, 0, 1, &vko->descriptorSets[vko->ping], 0, NULL);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vko->multiRadixSortComputePipeline);
        vkCmdDispatch(commandBuffer, globalInvocationSize, 1, 1);

        createBufferMemoryBarrier(
            vko,
            vko->cellEntriesBuffers[vko->ping == 0 ? 1 : 0], // writing to ping_opp as output, whereupon output becomes input, needs barrier between write and read
            commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT
        );

        // swap ping
        vko->ping = vko->ping == 0 ? 1 : 0;
    }
}

void dispatchComputeShader(VkCommandBuffer commandBuffer, VkPipeline computePipeline, VkPipelineLayout computePipelineLayout, VkDescriptorSet *pDescriptorSet, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, pDescriptorSet, 0, NULL);
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
}

void recordCommands(vk_context *vko, uint32_t currentFrame, int imageIndex) {
    int i = imageIndex;

    // record commands
    VkCommandBufferBeginInfo beginInfo = {0};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(vko->commandBuffers[i], &beginInfo);

    vkCmdResetQueryPool(vko->commandBuffers[i], vko->queryPool, 0, QUERY_COUNT * 2);

    VkRenderPassBeginInfo renderPassInfoCmd = {0}; // most basic command is starting a render pass
    renderPassInfoCmd.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfoCmd.renderPass = vko->renderPass;
    renderPassInfoCmd.framebuffer = vko->swapchainFramebuffers[i];
    renderPassInfoCmd.renderArea.offset = (VkOffset2D){0,0};
    renderPassInfoCmd.renderArea.extent = vko->surfaceCapabilities.currentExtent;

    VkClearValue clearColors[2] = {{0}, {0}};
    clearColors[0].color = (VkClearColorValue) {0};
    clearColors[1].depthStencil = (VkClearDepthStencilValue) {1.0f, 0};

    renderPassInfoCmd.clearValueCount = 2;
    renderPassInfoCmd.pClearValues = clearColors;

    for (int step = 0; step < vko->numSubsteps && !vko->pause; step++) {
        // first apply gravity and viscosity and predict next positions
        // this will be in the predictPositions buffer
        TS_BEGIN(QUERY_PREDICT);
        vkCmdBindPipeline(vko->commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, vko->predictPositionsComputePipeline);
        vkCmdBindDescriptorSets(vko->commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, vko->predictPositionsComputePipelineLayout, 0, 1, &vko->descriptorSets[vko->ping], 0, NULL);
        vkCmdDispatch(vko->commandBuffers[i], (vko->numParticles + 63) / 64, 1, 1);
        TS_END(QUERY_PREDICT);

        createBufferMemoryBarrier(
            vko,
            vko->predictedPositionsBuffers[vko->ping],
            vko->commandBuffers[i],
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT
        );
        createBufferMemoryBarrier(
            vko,
            vko->predictedVelocitiesBuffers[vko->ping],
            vko->commandBuffers[i],
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT
        );

        // next, update the cell entries
        TS_BEGIN(QUERY_UPDATE_CELL);
        vkCmdBindPipeline(vko->commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, vko->updateCellEntriesComputePipeline);
        vkCmdBindDescriptorSets(vko->commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, vko->updateCellEntriesComputePipelineLayout, 0, 1, &vko->descriptorSets[vko->ping], 0, NULL);
        vkCmdDispatch(vko->commandBuffers[i], (vko->numParticles + 63) / 64, 1, 1); // run for each number of particles
        TS_END(QUERY_UPDATE_CELL);

        createBufferMemoryBarrier(
            vko,
            vko->cellEntriesBuffers[vko->ping],
            vko->commandBuffers[i],
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
        );

        // ---------------------- HERE IS SORTING CODE --------------------------

        TS_BEGIN(QUERY_SORT);
        multiRadixSort(vko, vko->commandBuffers[i]);
        // bitonicSort(vko, vko->commandBuffers[i]);
        TS_END(QUERY_SORT);

        // ------------------- HERE IS END OF SORTING CODE ----------------------

        // now must need to scatter everything back into the positions buffer
        // next, update the cell entries
        TS_BEGIN(QUERY_SCATTER);
        vkCmdBindPipeline(vko->commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, vko->scatterPassComputePipeline);
        vkCmdBindDescriptorSets(vko->commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, vko->scatterPassComputePipelineLayout, 0, 1, &vko->descriptorSets[vko->ping], 0, NULL);
        vkCmdDispatch(vko->commandBuffers[i], (vko->numParticles + WORKGROUP_SIZE_X - 1) / WORKGROUP_SIZE_X, 1, 1);
        TS_END(QUERY_SCATTER);

        vko->ping = vko->ping == 0 ? 1 : 0;

        // need to make sure cellEntries gets fully sorted before working on start indices
        createBufferMemoryBarrier(
            vko, vko->cellEntriesBuffers[vko->ping],
            vko->commandBuffers[i],
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT
        );

        // clear start indices
        TS_BEGIN(QUERY_CLEAR_START);
        vkCmdBindPipeline(vko->commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, vko->clearStartIndicesComputePipeline);
        vkCmdBindDescriptorSets(vko->commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, vko->clearStartIndicesComputePipelineLayout, 0, 1, &vko->descriptorSets[vko->ping], 0, NULL);
        vkCmdDispatch(vko->commandBuffers[i], (vko->numParticles + WORKGROUP_SIZE_X - 1) / WORKGROUP_SIZE_X, 1, 1);
        TS_END(QUERY_CLEAR_START);

        createBufferMemoryBarrier(
            vko,
            vko->startIndicesBuffer,
            vko->commandBuffers[i],
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT // update start indices compute shader needs to write to this after (does not read)
        );

        createBufferMemoryBarrier(
            vko,
            vko->endIndicesBuffer,
            vko->commandBuffers[i],
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT // update start indices compute shader needs to write to this after (does not read)
        );

        // then update start indices
        TS_BEGIN(QUERY_UPDATE_START);
        vkCmdBindPipeline(vko->commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, vko->updateStartIndicesComputePipeline);
        vkCmdBindDescriptorSets(vko->commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, vko->updateStartIndicesComputePipelineLayout, 0, 1, &vko->descriptorSets[vko->ping], 0, NULL);
        vkCmdDispatch(vko->commandBuffers[i], (vko->numParticles + WORKGROUP_SIZE_X - 1) / WORKGROUP_SIZE_X, 1, 1);
        TS_END(QUERY_UPDATE_START);

        createBufferMemoryBarrier(
            vko,
            vko->startIndicesBuffer,
            vko->commandBuffers[i],
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT // all compute shaders after this will not touch the startIndices buffer, only read
        );

        createBufferMemoryBarrier(
            vko,
            vko->endIndicesBuffer,
            vko->commandBuffers[i],
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT // update start indices compute shader needs to write to this after (does not read)
        );

        // calculate particle densities
        TS_BEGIN(QUERY_DENSITY);
        vkCmdBindPipeline(vko->commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, vko->calculateDensitiesComputePipeline);
        vkCmdBindDescriptorSets(vko->commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, vko->calculateDensitiesComputePipelineLayout, 0, 1, &vko->descriptorSets[vko->ping], 0, NULL);
        vkCmdDispatch(vko->commandBuffers[i], (vko->numParticles + WORKGROUP_SIZE_X - 1) / WORKGROUP_SIZE_X, 1, 1);
        TS_END(QUERY_DENSITY);

        createBufferMemoryBarrier(
            vko,
            vko->densitiesBuffers[vko->ping],
            vko->commandBuffers[i],
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT // all compute shaders after this will not touch the densities buffer, only read
        );

        // calculate and apply pressure forces
        TS_BEGIN(QUERY_PRESSURE_AND_VISCOSITY);
        vkCmdBindPipeline(vko->commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, vko->applyPressureAndViscosityForceComputePipeline);
        vkCmdBindDescriptorSets(vko->commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, vko->applyPressureAndViscosityForceComputePipelineLayout, 0, 1, &vko->descriptorSets[vko->ping], 0, NULL);
        vkCmdDispatch(vko->commandBuffers[i], (vko->numParticles + WORKGROUP_SIZE_X - 1) / WORKGROUP_SIZE_X, 1, 1);
        TS_END(QUERY_PRESSURE_AND_VISCOSITY);

        createBufferMemoryBarrier(
            vko,
            vko->velocitiesBuffers[vko->ping],
            vko->commandBuffers[i],
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
        );

        createBufferMemoryBarrier(
            vko,
            vko->velocitiesBuffers[vko->ping],
            vko->commandBuffers[i],
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
        );

        // lastly, must update velocities
        // this will all be in the updateVelocities compute shader
        TS_BEGIN(QUERY_UPDATE_VELOCITY);
        vkCmdBindPipeline(vko->commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, vko->updateVelocitiesComputePipeline);
        vkCmdBindDescriptorSets(vko->commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, vko->updateVelocitiesComputePipelineLayout, 0, 1, &vko->descriptorSets[vko->ping], 0, NULL);
        vkCmdDispatch(vko->commandBuffers[i], (vko->numParticles + WORKGROUP_SIZE_X - 1) / WORKGROUP_SIZE_X, 1, 1);
        TS_END(QUERY_UPDATE_VELOCITY);

        createBufferMemoryBarrier(
            vko, 
            vko->positionsBuffers[vko->ping],
            vko->commandBuffers[i], 
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT
        );

        createBufferMemoryBarrier(
            vko, 
            vko->velocitiesBuffers[vko->ping],
            vko->commandBuffers[i], 
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
            VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_SHADER_READ_BIT
        );

        createBufferMemoryBarrier(
            vko, 
            vko->velocitiesBuffers[vko->ping],
            vko->commandBuffers[i], 
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
            VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_SHADER_READ_BIT
        );
    }

    createBufferMemoryBarrier(
        vko, 
        vko->positionsBuffers[vko->ping],
        vko->commandBuffers[i], 
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
        VK_ACCESS_SHADER_WRITE_BIT, 
        VK_ACCESS_SHADER_READ_BIT
    );

    TS_BEGIN(QUERY_RENDER);

    if (vko->renderType == RAYMARCH_VOLUMETRIC || vko->renderType == RAYMARCH_FLUID) {
        // update density map
        dispatchComputeShader(
            vko->commandBuffers[i],
            vko->updateDensityMapComputePipeline,
            vko->updateDensityMapComputePipelineLayout,
            &vko->descriptorSets[vko->ping],
            (vko->raymarchCellSize[0] + 7) / 8, (vko->raymarchCellSize[1] + 7) / 8, (vko->raymarchCellSize[2] + 7) / 8
        );

        createImageMemoryBarrier(
            vko,
            vko->densityMap,
            vko->commandBuffers[i],
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT
        );

        // volumetric rendering
        if (vko->renderType == RAYMARCH_VOLUMETRIC) {
            // calculate light volume
            dispatchComputeShader(
                vko->commandBuffers[i],
                vko->calculateLightVolumeComputePipeline,
                vko->calculateLightVolumeComputePipelineLayout,
                &vko->descriptorSets[vko->ping],
                (vko->lightVolumeSize[0] + 7) / 8, (vko->lightVolumeSize[1] + 7) / 8, (vko->lightVolumeSize[2] + 7) / 8
            );

            createImageMemoryBarrier(
                vko,
                vko->lightVolume,
                vko->commandBuffers[i],
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT
            );

            // raymarch particles as a volume (like an atmosphere or cloud)
            dispatchComputeShader(
                vko->commandBuffers[i],
                vko->volumetricFluidComputePipeline,
                vko->volumetricFluidComputePipelineLayout,
                &vko->descriptorSets[vko->ping],
                (vko->width + 15) / 16, (vko->height + 15) / 16, 1
            );
        }
        // proper fluid rendering (reflection and refractions)
        else if (vko->renderType == RAYMARCH_FLUID) {
            dispatchComputeShader(
                vko->commandBuffers[i],
                vko->properFluidComputePipeline,
                vko->properFluidComputePipelineLayout,
                &vko->descriptorSets[vko->ping],
                (vko->width + 15) / 16, (vko->height + 15) / 16, 1
            );
        }

        createImageMemoryBarrier(
            vko,
            vko->storageImage,
            vko->commandBuffers[i],
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT
        );
    }

    // render pass = the "plan" that says what it will do after it gets the image data. its currently working with a color attachment as a placeholder. needs pipeline to actually supply the data to the color attachment
    vkCmdBeginRenderPass(vko->commandBuffers[i], &renderPassInfoCmd, VK_SUBPASS_CONTENTS_INLINE);

    // here is graphipcs pipeline work
    VkPipeline graphicsPipeline;
    VkPipelineLayout graphicsPipelineLayout;
    if (vko->renderType == RASTERIZATION) {
        graphicsPipeline = vko->rasterizeGraphicsPipeline;
        graphicsPipelineLayout = vko->rasterizeGraphicsPipelineLayout;
    }
    else if (vko->renderType == RAYMARCH_VOLUMETRIC || vko->renderType == RAYMARCH_FLUID) {
        graphicsPipeline = vko->raymarchGraphicsPipeline;
        graphicsPipelineLayout = vko->raymarchGraphicsPipelineLayout;
    }
    vkCmdBindPipeline(vko->commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    vkCmdBindDescriptorSets(vko->commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout, 0, 1, &vko->descriptorSets[vko->ping], 0, NULL);

    // specified viewport + scissor to be dynamic => must explicity set them here
    vko->viewport = (VkViewport) {0};
    vko->viewport.x = 0.0f;
    vko->viewport.y = 0.0f;
    vko->viewport.width = (float) vko->surfaceCapabilities.currentExtent.width;
    vko->viewport.height = (float) vko->surfaceCapabilities.currentExtent.height;
    vko->viewport.minDepth = 0.0f;
    vko->viewport.maxDepth = 1.0f;
    vkCmdSetViewport(vko->commandBuffers[i], 0, 1, &vko->viewport);

    // must specify scissor as well b/c we've set pipeline to be in dynamic state
    vko->scissor = (VkRect2D) {0};
    vko->scissor.offset = (VkOffset2D){0,0};
    vko->scissor.extent = vko->surfaceCapabilities.currentExtent;
    vkCmdSetScissor(vko->commandBuffers[i], 0, 1, &vko->scissor);

    if (vko->renderType == RASTERIZATION) {
        VkBuffer buffers[2] = { vko->sphereMeshBuffer, vko->positionsBuffers[vko->ping] };

        vkCmdBindVertexBuffers(vko->commandBuffers[i], 0, 2, buffers, (VkDeviceSize[2]) {0, 0});
        vkCmdDraw(vko->commandBuffers[i], vko->sphereVertexCount, vko->numParticles, 0, 0);
    }
    else if (vko->renderType == RAYMARCH_VOLUMETRIC || vko->renderType == RAYMARCH_FLUID) {
        vkCmdBindVertexBuffers(vko->commandBuffers[i], 0, 1, &vko->fullScreenQuadVertexBuffer, (VkDeviceSize[]) {0});
        vkCmdDraw(vko->commandBuffers[i], 6, 1, 0, 0);
    }
    
    vkCmdEndRenderPass(vko->commandBuffers[i]);
    TS_END(QUERY_RENDER);
    vkEndCommandBuffer(vko->commandBuffers[i]);
}
