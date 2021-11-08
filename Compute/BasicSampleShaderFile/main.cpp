// https://www.duskborn.com/posts/a-simple-vulkan-compute-example/
// https://gist.github.com/sheredom/523f02bbad2ae397d7ed255f3f3b5a7f
// export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation

// This is free and unencumbered software released into the public domain.
//
// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.
//
// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// For more information, please refer to <http://unlicense.org/>

#include "vulkan/vulkan.h"

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <fstream>

std::vector<char> readFile(const std::string& filepath) {
  std::ifstream file{filepath, std::ios::binary};

  if (!file) {
    throw std::runtime_error("failed to open file: " + filepath);
  }

  std::vector<char> result{std::istreambuf_iterator<char>(file),
                           std::istreambuf_iterator<char>()};
  return result;
}

#define BAIL_ON_BAD_RESULT(result) \
  if (VK_SUCCESS != (result)) { fprintf(stderr, "Failure at %u %s\n", __LINE__, __FILE__); exit(-1); }

VkResult vkGetBestTransferQueueNPH(VkPhysicalDevice physicalDevice, uint32_t* queueFamilyIndex) {
  uint32_t queueFamilyPropertiesCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, 0);

  VkQueueFamilyProperties* const queueFamilyProperties = (VkQueueFamilyProperties*)alloca(
    sizeof(VkQueueFamilyProperties) * queueFamilyPropertiesCount);

  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, queueFamilyProperties);

  // first try and find a queue that has just the transfer bit set
  for (uint32_t i = 0; i < queueFamilyPropertiesCount; i++) {
    // mask out the sparse binding bit that we aren't caring about (yet!)
    const VkQueueFlags maskedFlags = (~VK_QUEUE_SPARSE_BINDING_BIT & queueFamilyProperties[i].queueFlags);

    if (!((VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT) & maskedFlags) &&
      (VK_QUEUE_TRANSFER_BIT & maskedFlags)) {
      *queueFamilyIndex = i;
      return VK_SUCCESS;
    }
  }

  // otherwise we'll prefer using a compute-only queue,
  // remember that having compute on the queue implicitly enables transfer!
  for (uint32_t i = 0; i < queueFamilyPropertiesCount; i++) {
    // mask out the sparse binding bit that we aren't caring about (yet!)
    const VkQueueFlags maskedFlags = (~VK_QUEUE_SPARSE_BINDING_BIT & queueFamilyProperties[i].queueFlags);

    if (!(VK_QUEUE_GRAPHICS_BIT & maskedFlags) && (VK_QUEUE_COMPUTE_BIT & maskedFlags)) {
      *queueFamilyIndex = i;
      return VK_SUCCESS;
    }
  }

  // lastly get any queue that'll work for us (graphics, compute or transfer bit set)
  for (uint32_t i = 0; i < queueFamilyPropertiesCount; i++) {
    // mask out the sparse binding bit that we aren't caring about (yet!)
    const VkQueueFlags maskedFlags = (~VK_QUEUE_SPARSE_BINDING_BIT & queueFamilyProperties[i].queueFlags);

    if ((VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT) & maskedFlags) {
      *queueFamilyIndex = i;
      return VK_SUCCESS;
    }
  }

  return VK_ERROR_INITIALIZATION_FAILED;
}

VkResult vkGetBestComputeQueueNPH(VkPhysicalDevice physicalDevice, uint32_t* queueFamilyIndex) {
  uint32_t queueFamilyPropertiesCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, 0);

  VkQueueFamilyProperties* const queueFamilyProperties = (VkQueueFamilyProperties*)alloca(
    sizeof(VkQueueFamilyProperties) * queueFamilyPropertiesCount);

  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, queueFamilyProperties);

  // first try and find a queue that has just the compute bit set
  for (uint32_t i = 0; i < queueFamilyPropertiesCount; i++) {
    // mask out the sparse binding bit that we aren't caring about (yet!) and the transfer bit
    const VkQueueFlags maskedFlags = (~(VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT) &
      queueFamilyProperties[i].queueFlags);

    if (!(VK_QUEUE_GRAPHICS_BIT & maskedFlags) && (VK_QUEUE_COMPUTE_BIT & maskedFlags)) {
      *queueFamilyIndex = i;
      return VK_SUCCESS;
    }
  }

  // lastly get any queue that'll work for us
  for (uint32_t i = 0; i < queueFamilyPropertiesCount; i++) {
    // mask out the sparse binding bit that we aren't caring about (yet!) and the transfer bit
    const VkQueueFlags maskedFlags = (~(VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT) &
      queueFamilyProperties[i].queueFlags);

    if (VK_QUEUE_COMPUTE_BIT & maskedFlags) {
      *queueFamilyIndex = i;
      return VK_SUCCESS;
    }
  }

  return VK_ERROR_INITIALIZATION_FAILED;
}

int main(int argc, const char * const argv[]) {
  (void)argc;
  (void)argv;

  const VkApplicationInfo applicationInfo = {
    VK_STRUCTURE_TYPE_APPLICATION_INFO,
    0,
    "VKComputeSample",
    0,
    "",
    0,
    VK_MAKE_VERSION(1, 0, 9)
  };
  
  const VkInstanceCreateInfo instanceCreateInfo = {
    VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    0,
    0,
    &applicationInfo,
    0,
    0,
    0,
    0
  };
  
  VkInstance instance;
  BAIL_ON_BAD_RESULT(vkCreateInstance(&instanceCreateInfo, 0, &instance));
  
  uint32_t physicalDeviceCount = 0;
  BAIL_ON_BAD_RESULT(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, 0));

  VkPhysicalDevice* const physicalDevices = (VkPhysicalDevice*)malloc(
    sizeof(VkPhysicalDevice) * physicalDeviceCount);

  BAIL_ON_BAD_RESULT(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));

  for (uint32_t i = 0; i < physicalDeviceCount; i++) {
    // Just for information
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevices[i],&props);
    printf("device name: %s\n", props.deviceName);

    uint32_t queueFamilyIndex = 0;
    BAIL_ON_BAD_RESULT(vkGetBestComputeQueueNPH(physicalDevices[i], &queueFamilyIndex));

    const float queuePrioritory = 1.0f;
    const VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
      VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      0,
      0,
      queueFamilyIndex,
      1,
      &queuePrioritory
    };

    const VkDeviceCreateInfo deviceCreateInfo = {
      VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      0,
      0,
      1,
      &deviceQueueCreateInfo,
      0,
      0,
      0,
      0,
      0
    };

    VkDevice device;
    BAIL_ON_BAD_RESULT(vkCreateDevice(physicalDevices[i], &deviceCreateInfo, 0, &device));

    VkPhysicalDeviceMemoryProperties properties;

    vkGetPhysicalDeviceMemoryProperties(physicalDevices[i], &properties);

    const int32_t bufferLength = 16384;

    const uint32_t bufferSize = sizeof(int32_t) * bufferLength;

    // we are going to need two buffers from this one memory
    const VkDeviceSize memorySize = bufferSize * 2; 

    // set memoryTypeIndex to an invalid entry in the properties.memoryTypes array
    uint32_t memoryTypeIndex = VK_MAX_MEMORY_TYPES;

    for (uint32_t k = 0; k < properties.memoryTypeCount; k++) {
      if ((VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT & properties.memoryTypes[k].propertyFlags) &&
        (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT & properties.memoryTypes[k].propertyFlags) &&
        (memorySize < properties.memoryHeaps[properties.memoryTypes[k].heapIndex].size)) {
        memoryTypeIndex = k;
        break;
      }
    }

    BAIL_ON_BAD_RESULT(memoryTypeIndex == VK_MAX_MEMORY_TYPES ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_SUCCESS);

    const VkMemoryAllocateInfo memoryAllocateInfo = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      0,
      memorySize,
      memoryTypeIndex
    };

    VkDeviceMemory memory;
    BAIL_ON_BAD_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, 0, &memory));

    int32_t *payload;
    BAIL_ON_BAD_RESULT(vkMapMemory(device, memory, 0, memorySize, 0, (void**)&payload));

    for (uint32_t k = 1; k < memorySize / sizeof(int32_t); k++) {
      payload[k] = rand();
    }

    vkUnmapMemory(device, memory);

    const VkBufferCreateInfo bufferCreateInfo = {
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      0,
      0,
      bufferSize,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      VK_SHARING_MODE_EXCLUSIVE,
      1,
      &queueFamilyIndex
    };

    VkBuffer in_buffer;
    BAIL_ON_BAD_RESULT(vkCreateBuffer(device, &bufferCreateInfo, 0, &in_buffer));

    BAIL_ON_BAD_RESULT(vkBindBufferMemory(device, in_buffer, memory, 0));

    VkBuffer out_buffer;
    BAIL_ON_BAD_RESULT(vkCreateBuffer(device, &bufferCreateInfo, 0, &out_buffer));

    BAIL_ON_BAD_RESULT(vkBindBufferMemory(device, out_buffer, memory, bufferSize));

    const auto compCode = readFile("./shaders/simple_shader.comp.spv");

    VkShaderModuleCreateInfo shaderModuleCreateInfo = {
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      0,
      0,
      compCode.size(), //sizeof(shader),
      (const uint32_t*)compCode.data() //shader
    };

    VkShaderModule shader_module;

    BAIL_ON_BAD_RESULT(vkCreateShaderModule(device, &shaderModuleCreateInfo, 0, &shader_module));

    VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[2] = {
      {
        0,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        1,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0
      },
      {
        1,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        1,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0
      }
    };

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      0,
      0,
      2,
      descriptorSetLayoutBindings
    };

    VkDescriptorSetLayout descriptorSetLayout;
    BAIL_ON_BAD_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, 0, &descriptorSetLayout));

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      0,
      0,
      1,
      &descriptorSetLayout,
      0,
      0
    };

    VkPipelineLayout pipelineLayout;
    BAIL_ON_BAD_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, 0, &pipelineLayout));

    VkComputePipelineCreateInfo computePipelineCreateInfo = {
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      0,
      0,
      {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        0,
        0,
        VK_SHADER_STAGE_COMPUTE_BIT,
        shader_module,
        "main",
        0
      },
      pipelineLayout,
      0,
      0
    };

    VkPipeline pipeline;
    BAIL_ON_BAD_RESULT(vkCreateComputePipelines(device, 0, 1, &computePipelineCreateInfo, 0, &pipeline));

    VkCommandPoolCreateInfo commandPoolCreateInfo = {
      VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      0,
      0,
      queueFamilyIndex
    };

    VkDescriptorPoolSize descriptorPoolSize = {
      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      2
    };

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      0,
      0,
      1,
      1,
      &descriptorPoolSize
    };

    VkDescriptorPool descriptorPool;
    BAIL_ON_BAD_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, 0, &descriptorPool));

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      0,
      descriptorPool,
      1,
      &descriptorSetLayout
    };

    VkDescriptorSet descriptorSet;
    BAIL_ON_BAD_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

    VkDescriptorBufferInfo in_descriptorBufferInfo = {
      in_buffer,
      0,
      VK_WHOLE_SIZE
    };

    VkDescriptorBufferInfo out_descriptorBufferInfo = {
      out_buffer,
      0,
      VK_WHOLE_SIZE
    };

    VkWriteDescriptorSet writeDescriptorSet[2] = {
      {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        0,
        descriptorSet,
        0,
        0,
        1,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        0,
        &in_descriptorBufferInfo,
        0
      },
      {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        0,
        descriptorSet,
        1,
        0,
        1,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        0,
        &out_descriptorBufferInfo,
        0
      }
    };

    vkUpdateDescriptorSets(device, 2, writeDescriptorSet, 0, 0);

    VkCommandPool commandPool;
    BAIL_ON_BAD_RESULT(vkCreateCommandPool(device, &commandPoolCreateInfo, 0, &commandPool));

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      0,
      commandPool,
      VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      1
    };

    VkCommandBuffer commandBuffer;
    BAIL_ON_BAD_RESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer));

    VkCommandBufferBeginInfo commandBufferBeginInfo = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      0,
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      0
    };

    BAIL_ON_BAD_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
      pipelineLayout, 0, 1, &descriptorSet, 0, 0);

    vkCmdDispatch(commandBuffer, bufferSize / sizeof(int32_t), 1, 1);

    BAIL_ON_BAD_RESULT(vkEndCommandBuffer(commandBuffer));

    VkQueue queue;
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

    VkSubmitInfo submitInfo = {
      VK_STRUCTURE_TYPE_SUBMIT_INFO,
      0,
      0,
      0,
      0,
      1,
      &commandBuffer,
      0,
      0
    };

    BAIL_ON_BAD_RESULT(vkQueueSubmit(queue, 1, &submitInfo, 0));

    BAIL_ON_BAD_RESULT(vkQueueWaitIdle(queue));

    BAIL_ON_BAD_RESULT(vkMapMemory(device, memory, 0, memorySize, 0, (void **)&payload));

    for (uint32_t k = 0, e = bufferSize / sizeof(int32_t); k < e; k++) {
      BAIL_ON_BAD_RESULT(payload[k + e] == payload[k] ? VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY);
    }
  }

  printf("Done.\n");
}
