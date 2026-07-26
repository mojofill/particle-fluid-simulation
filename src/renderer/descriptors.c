#include "descriptors.h"
#include "buffer.h"

void createDescriptorSetLayout(vk_context *vko) {
    // update these to account for ping pong

    VkDescriptorSetLayoutBinding bindings[19];

    VkDescriptorSetLayoutBinding uboLayoutBinding = {0};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[0] = uboLayoutBinding;

    VkDescriptorSetLayoutBinding imageLayoutBinding = {0};
    imageLayoutBinding.binding = 1;
    imageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    imageLayoutBinding.descriptorCount = 1;
    imageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[1] = imageLayoutBinding;

    // use struct of arrays for minimum overhead
    // essentially create a positions, velocities, and cellEntries array
    // create 2 in total: positions and velocities
    // then create a cellEntries buffer, a predictedPositions buffer, a predictedVelocities buffer, a densities buffer, and finally a startIndices buffer
    // then 6 more for outputs
    // add another color buffer
    // and one more cellEntriesOutput buffer for radix sort
    // and one more histogramsBuffer for radix sort
    // and one more endIndices buffer (for optimization)
    // so 18 in total
    for (int binding = 2; binding < 19; binding++) {
        VkDescriptorSetLayoutBinding SSBOLayoutBinding = {0};
        SSBOLayoutBinding.binding = binding;
        SSBOLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        SSBOLayoutBinding.descriptorCount = 1;
        SSBOLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[binding] = SSBOLayoutBinding;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pBindings = bindings;
    layoutInfo.bindingCount = (int) sizeof(bindings) / sizeof(VkDescriptorSetLayoutBinding);

    if (vkCreateDescriptorSetLayout(vko->device, &layoutInfo, NULL, &vko->descriptorSetLayout) != VK_SUCCESS) {
        printf("Failed to create descriptor set layout\n");
        exit(1);
    }
}

void createUniformBuffer(vk_context *vko) {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    
    createBuffer(vko, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vko->uniformBuffer, &vko->uniformBufferMemory);
    vkMapMemory(vko->device, vko->uniformBufferMemory, 0, bufferSize, 0, &vko->uniformBufferMapped);
}

void createDescriptorPool(vk_context *vko) {
    VkDescriptorPoolSize poolSizes[19]; // adjust for however many uniforms you want
    // only one singular uniform buffer object needs to be allocated
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = (uint32_t) MAX_FRAMES_IN_FLIGHT;

    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = (uint32_t) MAX_FRAMES_IN_FLIGHT;

    for (int i = 2; i < 19; i++) {
        poolSizes[i].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[i].descriptorCount = (uint32_t) MAX_FRAMES_IN_FLIGHT;
    }

    VkDescriptorPoolCreateInfo poolInfo = {0};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = (int) sizeof(poolSizes) / sizeof(VkDescriptorPoolSize);
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = (uint32_t) MAX_FRAMES_IN_FLIGHT;

    if (vkCreateDescriptorPool(vko->device, &poolInfo, NULL, &vko->descriptorPool) != VK_SUCCESS) {
        printf("failed to create descriptor pool\n");
        exit(1);
    }
}

void createDescriptorSets(vk_context *vko) {
    for (int ping = 0; ping < 2; ping++) {
        int ping_opp = ping == 0 ? 1 : 0;
        VkDescriptorSetAllocateInfo allocInfo = {0};

        // need identical layouts for each MAX_FRAMES_IN_FLIGHT descriptor set
        VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT] = {0};
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) layouts[i] = vko->descriptorSetLayout;

        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = vko->descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = layouts;

        if (vkAllocateDescriptorSets(vko->device, &allocInfo, &vko->descriptorSets[ping]) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create descriptor sets\n");
            exit(1);
        }

        // populate descriptors

        // add buffer to descriptor set
        VkDescriptorBufferInfo bufferInfo = {0};
        bufferInfo.buffer = vko->uniformBuffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo imageInfo = {0};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // doesnt matter what layout, so set it to be general layout
        imageInfo.imageView = vko->storageImageView;
        imageInfo.sampler = vko->storageSampler;

        // positionsBuffer
        VkDescriptorBufferInfo positionsBufferInfo = {0};
        positionsBufferInfo.buffer = vko->positionsBuffers[ping];
        positionsBufferInfo.offset = 0;
        positionsBufferInfo.range = VK_WHOLE_SIZE;

        // velocitiesBuffer
        VkDescriptorBufferInfo velocitiesBufferInfo = {0};
        velocitiesBufferInfo.buffer = vko->velocitiesBuffers[ping];
        velocitiesBufferInfo.offset = 0;
        velocitiesBufferInfo.range = VK_WHOLE_SIZE;

        // predictedPositionsBuffer
        VkDescriptorBufferInfo predictedPositionsBufferInfo = {0};
        predictedPositionsBufferInfo.buffer = vko->predictedPositionsBuffers[ping];
        predictedPositionsBufferInfo.offset = 0;
        predictedPositionsBufferInfo.range = VK_WHOLE_SIZE;

        // predictedVelocitiesBuffer
        VkDescriptorBufferInfo predictedVelocitiesBufferInfo = {0};
        predictedVelocitiesBufferInfo.buffer = vko->predictedVelocitiesBuffers[ping];
        predictedVelocitiesBufferInfo.offset = 0;
        predictedVelocitiesBufferInfo.range = VK_WHOLE_SIZE;

        // cellEntriesBuffer
        VkDescriptorBufferInfo cellEntriesBufferInfo = {0};
        cellEntriesBufferInfo.buffer = vko->cellEntriesBuffers[ping];
        cellEntriesBufferInfo.offset = 0;
        cellEntriesBufferInfo.range = VK_WHOLE_SIZE;
        
        // densitiesBuffer
        VkDescriptorBufferInfo densitiesBufferInfo = {0};
        densitiesBufferInfo.buffer = vko->densitiesBuffers[ping];
        densitiesBufferInfo.offset = 0;
        densitiesBufferInfo.range = VK_WHOLE_SIZE;

        // startIndicesBuffer
        VkDescriptorBufferInfo startIndicesBufferInfo = {0};
        startIndicesBufferInfo.buffer = vko->startIndicesBuffer;
        startIndicesBufferInfo.offset = 0;
        startIndicesBufferInfo.range = VK_WHOLE_SIZE;

        // colorsBuffer
        VkDescriptorBufferInfo colorsBufferInfo = {0};
        colorsBufferInfo.buffer = vko->colorsBuffers[ping];
        colorsBufferInfo.offset = 0;
        colorsBufferInfo.range = VK_WHOLE_SIZE;

        // output buffers
        // positionsOutput
        VkDescriptorBufferInfo positionsOutputBufferInfo = {0};
        positionsOutputBufferInfo.buffer = vko->positionsBuffers[ping_opp];
        positionsOutputBufferInfo.offset = 0;
        positionsOutputBufferInfo.range = VK_WHOLE_SIZE;

        // velocitiesOutput
        VkDescriptorBufferInfo velocitiesOutputBufferInfo = {0};
        velocitiesOutputBufferInfo.buffer = vko->velocitiesBuffers[ping_opp];
        velocitiesOutputBufferInfo.offset = 0;
        velocitiesOutputBufferInfo.range = VK_WHOLE_SIZE;

        // predictedPositionsOutput
        VkDescriptorBufferInfo predictedPositionsOutputBufferInfo = {0};
        predictedPositionsOutputBufferInfo.buffer = vko->predictedPositionsBuffers[ping_opp];
        predictedPositionsOutputBufferInfo.offset = 0;
        predictedPositionsOutputBufferInfo.range = VK_WHOLE_SIZE;

        // predictedVelocitiesOutput
        VkDescriptorBufferInfo predictedVelocitiesOutputBufferInfo = {0};
        predictedVelocitiesOutputBufferInfo.buffer = vko->predictedVelocitiesBuffers[ping_opp];
        predictedVelocitiesOutputBufferInfo.offset = 0;
        predictedVelocitiesOutputBufferInfo.range = VK_WHOLE_SIZE;

        // densitiesOutput
        VkDescriptorBufferInfo densitiesOutputBufferInfo = {0};
        densitiesOutputBufferInfo.buffer = vko->densitiesBuffers[ping_opp];
        densitiesOutputBufferInfo.offset = 0;
        densitiesOutputBufferInfo.range = VK_WHOLE_SIZE;

        // colorsOutput
        VkDescriptorBufferInfo colorsOutputBufferInfo = {0};
        colorsOutputBufferInfo.buffer = vko->colorsBuffers[ping_opp];
        colorsOutputBufferInfo.offset = 0;
        colorsOutputBufferInfo.range = VK_WHOLE_SIZE;

        // cellEntriesOutput
        VkDescriptorBufferInfo cellEntriesOutputBufferInfo = {0};
        cellEntriesOutputBufferInfo.buffer = vko->cellEntriesBuffers[ping_opp];
        cellEntriesOutputBufferInfo.offset = 0;
        cellEntriesOutputBufferInfo.range = VK_WHOLE_SIZE;

        // histogramsBuffer
        VkDescriptorBufferInfo histogramsBufferInfo = {0};
        histogramsBufferInfo.buffer = vko->histogramsBuffer;
        histogramsBufferInfo.offset = 0;
        histogramsBufferInfo.range = VK_WHOLE_SIZE;

        // endIndicesBuffer
        VkDescriptorBufferInfo endIndicesBufferInfo = {0};
        endIndicesBufferInfo.buffer = vko->endIndicesBuffer;
        endIndicesBufferInfo.offset = 0;
        endIndicesBufferInfo.range = VK_WHOLE_SIZE;

        // write to descriptor set
        VkWriteDescriptorSet descriptorWrites[19] = {0}; // can add more descriptor sets if necessary
        // such as samplers or storage buffers

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = vko->descriptorSets[ping]; // write to this descriptor set
        descriptorWrites[0].dstBinding = 0; // write to this binding (binding = 0)
        descriptorWrites[0].dstArrayElement = 0; // can have descriptor arrays, but we are not, so put first; ie index=0
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1; // only updating one descriptor
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = vko->descriptorSets[ping];
        descriptorWrites[1].dstBinding = 1; // binding = 1 in the shader
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = vko->descriptorSets[ping];
        descriptorWrites[2].dstBinding = 2; // binding = 2 in the shader
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &positionsBufferInfo;

        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = vko->descriptorSets[ping];
        descriptorWrites[3].dstBinding = 3; // binding = 3 in the shader
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pBufferInfo = &velocitiesBufferInfo;

        descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[4].dstSet = vko->descriptorSets[ping];
        descriptorWrites[4].dstBinding = 4; // binding = 4 in the shader
        descriptorWrites[4].dstArrayElement = 0;
        descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[4].descriptorCount = 1;
        descriptorWrites[4].pBufferInfo = &predictedPositionsBufferInfo;

        descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[5].dstSet = vko->descriptorSets[ping];
        descriptorWrites[5].dstBinding = 5; // binding = 5 in the shader
        descriptorWrites[5].dstArrayElement = 0;
        descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[5].descriptorCount = 1;
        descriptorWrites[5].pBufferInfo = &predictedVelocitiesBufferInfo;

        descriptorWrites[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[6].dstSet = vko->descriptorSets[ping];
        descriptorWrites[6].dstBinding = 6; // binding = 6 in the shader
        descriptorWrites[6].dstArrayElement = 0;
        descriptorWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[6].descriptorCount = 1;
        descriptorWrites[6].pBufferInfo = &cellEntriesBufferInfo;

        descriptorWrites[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[7].dstSet = vko->descriptorSets[ping];
        descriptorWrites[7].dstBinding = 7; // binding = 7 in the shader
        descriptorWrites[7].dstArrayElement = 0;
        descriptorWrites[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[7].descriptorCount = 1;
        descriptorWrites[7].pBufferInfo = &densitiesBufferInfo;

        descriptorWrites[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[8].dstSet = vko->descriptorSets[ping];
        descriptorWrites[8].dstBinding = 8; // binding = 8 in the shader
        descriptorWrites[8].dstArrayElement = 0;
        descriptorWrites[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[8].descriptorCount = 1;
        descriptorWrites[8].pBufferInfo = &startIndicesBufferInfo;

        descriptorWrites[9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[9].dstSet = vko->descriptorSets[ping];
        descriptorWrites[9].dstBinding = 9; // binding = 9 in the shader
        descriptorWrites[9].dstArrayElement = 0;
        descriptorWrites[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[9].descriptorCount = 1;
        descriptorWrites[9].pBufferInfo = &colorsBufferInfo;

        descriptorWrites[10].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[10].dstSet = vko->descriptorSets[ping];
        descriptorWrites[10].dstBinding = 10; // binding = 10 in the shader
        descriptorWrites[10].dstArrayElement = 0;
        descriptorWrites[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[10].descriptorCount = 1;
        descriptorWrites[10].pBufferInfo = &positionsOutputBufferInfo;

        descriptorWrites[11].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[11].dstSet = vko->descriptorSets[ping];
        descriptorWrites[11].dstBinding = 11; // binding = 11 in the shader
        descriptorWrites[11].dstArrayElement = 0;
        descriptorWrites[11].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[11].descriptorCount = 1;
        descriptorWrites[11].pBufferInfo = &velocitiesOutputBufferInfo;

        descriptorWrites[12].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[12].dstSet = vko->descriptorSets[ping];
        descriptorWrites[12].dstBinding = 12; // binding = 12 in the shader
        descriptorWrites[12].dstArrayElement = 0;
        descriptorWrites[12].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[12].descriptorCount = 1;
        descriptorWrites[12].pBufferInfo = &predictedPositionsOutputBufferInfo;

        descriptorWrites[13].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[13].dstSet = vko->descriptorSets[ping];
        descriptorWrites[13].dstBinding = 13; // binding = 13 in the shader
        descriptorWrites[13].dstArrayElement = 0;
        descriptorWrites[13].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[13].descriptorCount = 1;
        descriptorWrites[13].pBufferInfo = &predictedVelocitiesOutputBufferInfo;

        descriptorWrites[14].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[14].dstSet = vko->descriptorSets[ping];
        descriptorWrites[14].dstBinding = 14; // binding = 14 in the shader
        descriptorWrites[14].dstArrayElement = 0;
        descriptorWrites[14].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[14].descriptorCount = 1;
        descriptorWrites[14].pBufferInfo = &densitiesOutputBufferInfo;

        descriptorWrites[15].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[15].dstSet = vko->descriptorSets[ping];
        descriptorWrites[15].dstBinding = 15; // binding = 15 in the shader
        descriptorWrites[15].dstArrayElement = 0;
        descriptorWrites[15].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[15].descriptorCount = 1;
        descriptorWrites[15].pBufferInfo = &colorsOutputBufferInfo;

        descriptorWrites[16].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[16].dstSet = vko->descriptorSets[ping];
        descriptorWrites[16].dstBinding = 16; // binding = 16 in the shader
        descriptorWrites[16].dstArrayElement = 0;
        descriptorWrites[16].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[16].descriptorCount = 1;
        descriptorWrites[16].pBufferInfo = &cellEntriesOutputBufferInfo;

        descriptorWrites[17].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[17].dstSet = vko->descriptorSets[ping];
        descriptorWrites[17].dstBinding = 17; // binding = 17 in the shader
        descriptorWrites[17].dstArrayElement = 0;
        descriptorWrites[17].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[17].descriptorCount = 1;
        descriptorWrites[17].pBufferInfo = &histogramsBufferInfo;

        descriptorWrites[18].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[18].dstSet = vko->descriptorSets[ping];
        descriptorWrites[18].dstBinding = 18; // binding = 18 in the shader
        descriptorWrites[18].dstArrayElement = 0;
        descriptorWrites[18].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[18].descriptorCount = 1;
        descriptorWrites[18].pBufferInfo = &endIndicesBufferInfo;

        vkUpdateDescriptorSets(vko->device, (int) sizeof(descriptorWrites) / sizeof(VkWriteDescriptorSet), descriptorWrites, 0, NULL);
    }
}
