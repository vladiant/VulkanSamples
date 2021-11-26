// https://www.duskborn.com/posts/a-simple-vulkan-compute-example/
// https://gist.github.com/sheredom/523f02bbad2ae397d7ed255f3f3b5a7f

#include <vulkan/vulkan.hpp>

#include <cstdio>
#include <iostream>

uint32_t getBestComputeQueue(const vk::PhysicalDevice& physicalDevice) {
  const std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

  // first try and find a queue that has just the compute bit set
  for (uint32_t i = 0; i < queueFamilyProperties.size(); i++) {
    // mask out the sparse binding bit that we aren't caring about (yet!) and the transfer bit
    const vk::QueueFlags maskedFlags = (~(vk::QueueFlagBits::eTransfer | vk::QueueFlagBits::eSparseBinding) &
      queueFamilyProperties[i].queueFlags);

    if (!(vk::QueueFlagBits::eGraphics & maskedFlags) && (vk::QueueFlagBits::eCompute & maskedFlags)) {
      return i;
    }
  }

  // lastly get any queue that'll work for us
  for (uint32_t i = 0; i < queueFamilyProperties.size(); i++) {
    // mask out the sparse binding bit that we aren't caring about (yet!) and the transfer bit
    const vk::QueueFlags maskedFlags = (~(vk::QueueFlagBits::eTransfer | vk::QueueFlagBits::eSparseBinding) &
      queueFamilyProperties[i].queueFlags);

    if (vk::QueueFlagBits::eCompute & maskedFlags) {
      return i;
    }
  }

  throw std::runtime_error("No suitable queue for computation found");
}

uint32_t getMemoryTypeIndex(const vk::PhysicalDeviceMemoryProperties& properties, vk::DeviceSize memorySize) {
  for (uint32_t k = 0; k < properties.memoryTypeCount; k++) {
    if ((vk::MemoryPropertyFlagBits::eHostVisible & properties.memoryTypes[k].propertyFlags) &&
      (vk::MemoryPropertyFlagBits::eHostCoherent & properties.memoryTypes[k].propertyFlags) &&
      (memorySize < properties.memoryHeaps[properties.memoryTypes[k].heapIndex].size)) {
      return k;
      break;
    }
  }

  throw std::runtime_error("No suitable memory index found");
}

int main(int argc, const char * const argv[]) {
  (void)argc;
  (void)argv;

  const vk::ApplicationInfo applicationInfo("VKComputeSample", 0, "", 0, VK_MAKE_VERSION(1, 0, 9));

  const vk::InstanceCreateInfo instanceCreateInfo({}, &applicationInfo);

  const vk::Instance instance = vk::createInstance(instanceCreateInfo);

  const std::vector<vk::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();

  
  for (const auto& physicalDevice : physicalDevices) {
    // Just for information
    const vk::PhysicalDeviceProperties props = physicalDevice.getProperties();
    std::cout << "device name: " <<  props.deviceName << '\n';

    const uint32_t queueFamilyIndex = getBestComputeQueue(physicalDevice);

    const float queuePrioritory = 1.0f;
    const vk::DeviceQueueCreateInfo deviceQueueCreateInfo({}, queueFamilyIndex, 1, &queuePrioritory);

    const vk::DeviceCreateInfo deviceCreateInfo({}, 1, &deviceQueueCreateInfo);

    const vk::Device device = physicalDevice.createDevice(deviceCreateInfo);

    const vk::PhysicalDeviceMemoryProperties properties = physicalDevice.getMemoryProperties();

    const int32_t bufferLength = 16384;

    const uint32_t bufferSize = sizeof(int32_t) * bufferLength;

    // we are going to need two buffers from this one memory
    const vk::DeviceSize memorySize = bufferSize * 2; 

    const uint32_t memoryTypeIndex = getMemoryTypeIndex(properties, memorySize);

    const vk::MemoryAllocateInfo memoryAllocateInfo(memorySize, memoryTypeIndex);

    const vk::DeviceMemory memory = device.allocateMemory(memoryAllocateInfo);

    int32_t *payload = static_cast<int32_t*>(device.mapMemory(memory, 0, memorySize));

    for (uint32_t k = 0; k < memorySize / sizeof(int32_t); k++) {
      payload[k] = rand();
    }

    device.unmapMemory(memory);

    const vk::BufferCreateInfo bufferCreateInfo(
      {}, bufferSize, vk::BufferUsageFlagBits::eStorageBuffer, vk::SharingMode::eExclusive, 1, &queueFamilyIndex);

    const vk::Buffer in_buffer = device.createBuffer(bufferCreateInfo);
    device.bindBufferMemory(in_buffer, memory, 0);

    const vk::Buffer out_buffer = device.createBuffer(bufferCreateInfo);
    device.bindBufferMemory(out_buffer, memory, bufferSize);

    enum {
      RESERVED_ID = 0,
      FUNC_ID,
      IN_ID,
      OUT_ID,
      GLOBAL_INVOCATION_ID,
      VOID_TYPE_ID,
      FUNC_TYPE_ID,
      INT_TYPE_ID,
      INT_ARRAY_TYPE_ID,
      STRUCT_ID,
      POINTER_TYPE_ID,
      ELEMENT_POINTER_TYPE_ID,
      INT_VECTOR_TYPE_ID,
      INT_VECTOR_POINTER_TYPE_ID,
      INT_POINTER_TYPE_ID,
      CONSTANT_ZERO_ID,
      CONSTANT_ARRAY_LENGTH_ID,
      LABEL_ID,
      IN_ELEMENT_ID,
      OUT_ELEMENT_ID,
      GLOBAL_INVOCATION_X_ID,
      GLOBAL_INVOCATION_X_PTR_ID,
      TEMP_LOADED_ID,
      BOUND
    };

    enum {
      INPUT = 1,
      UNIFORM = 2,
      BUFFER_BLOCK = 3,
      ARRAY_STRIDE = 6,
      BUILTIN = 11,
      BINDING = 33,
      OFFSET = 35,
      DESCRIPTOR_SET = 34,
      GLOBAL_INVOCATION = 28,
      OP_TYPE_VOID = 19,
      OP_TYPE_FUNCTION = 33,
      OP_TYPE_INT = 21,
      OP_TYPE_VECTOR = 23,
      OP_TYPE_ARRAY = 28,
      OP_TYPE_STRUCT = 30,
      OP_TYPE_POINTER = 32,
      OP_VARIABLE = 59,
      OP_DECORATE = 71,
      OP_MEMBER_DECORATE = 72,
      OP_FUNCTION = 54,
      OP_LABEL = 248,
      OP_ACCESS_CHAIN = 65,
      OP_CONSTANT = 43,
      OP_LOAD = 61,
      OP_STORE = 62,
      OP_RETURN = 253,
      OP_FUNCTION_END = 56,
      OP_CAPABILITY = 17,
      OP_MEMORY_MODEL = 14,
      OP_ENTRY_POINT = 15,
      OP_EXECUTION_MODE = 16,
      OP_COMPOSITE_EXTRACT = 81,
    };

    const uint32_t shader[] = {
      // first is the SPIR-V header
      0x07230203, // magic header ID
      0x00010000, // version 1.0.0
      0,          // generator (optional)
      BOUND,      // bound
      0,          // schema

      // OpCapability Shader
      (2 << 16) | OP_CAPABILITY, 1, 

      // OpMemoryModel Logical Simple
      (3 << 16) | OP_MEMORY_MODEL, 0, 0, 

      // OpEntryPoint GLCompute %FUNC_ID "f" %IN_ID %OUT_ID
      // was: (4 << 16) | OP_ENTRY_POINT, 5, FUNC_ID, 0x00000066,
      (5 << 16) | OP_ENTRY_POINT, 5, FUNC_ID, 0x66, GLOBAL_INVOCATION_ID,

      // OpExecutionMode %FUNC_ID LocalSize 1 1 1
      (6 << 16) | OP_EXECUTION_MODE, FUNC_ID, 17, 1, 1, 1,

      // next declare decorations

      (3 << 16) | OP_DECORATE, STRUCT_ID, BUFFER_BLOCK, 

      (4 << 16) | OP_DECORATE, GLOBAL_INVOCATION_ID, BUILTIN, GLOBAL_INVOCATION,

      (4 << 16) | OP_DECORATE, IN_ID, DESCRIPTOR_SET, 0,

      (4 << 16) | OP_DECORATE, IN_ID, BINDING, 0,

      (4 << 16) | OP_DECORATE, OUT_ID, DESCRIPTOR_SET, 0,

      (4 << 16) | OP_DECORATE, OUT_ID, BINDING, 1,

      (4 << 16) | OP_DECORATE, INT_ARRAY_TYPE_ID, ARRAY_STRIDE, 4,

      (5 << 16) | OP_MEMBER_DECORATE, STRUCT_ID, 0, OFFSET, 0,

      // next declare types
      (2 << 16) | OP_TYPE_VOID, VOID_TYPE_ID,

      (3 << 16) | OP_TYPE_FUNCTION, FUNC_TYPE_ID, VOID_TYPE_ID,

      (4 << 16) | OP_TYPE_INT, INT_TYPE_ID, 32, 1,

      (4 << 16) | OP_CONSTANT, INT_TYPE_ID, CONSTANT_ARRAY_LENGTH_ID, bufferLength,

      (4 << 16) | OP_TYPE_ARRAY, INT_ARRAY_TYPE_ID, INT_TYPE_ID, CONSTANT_ARRAY_LENGTH_ID,

      (3 << 16) | OP_TYPE_STRUCT, STRUCT_ID, INT_ARRAY_TYPE_ID,

      (4 << 16) | OP_TYPE_POINTER, POINTER_TYPE_ID, UNIFORM, STRUCT_ID,

      (4 << 16) | OP_TYPE_POINTER, ELEMENT_POINTER_TYPE_ID, UNIFORM, INT_TYPE_ID,

      (4 << 16) | OP_TYPE_VECTOR, INT_VECTOR_TYPE_ID, INT_TYPE_ID, 3,

      (4 << 16) | OP_TYPE_POINTER, INT_VECTOR_POINTER_TYPE_ID, INPUT, INT_VECTOR_TYPE_ID,

      (4 << 16) | OP_TYPE_POINTER, INT_POINTER_TYPE_ID, INPUT, INT_TYPE_ID,

      // then declare constants
      (4 << 16) | OP_CONSTANT, INT_TYPE_ID, CONSTANT_ZERO_ID, 0,

      // then declare variables
      (4 << 16) | OP_VARIABLE, POINTER_TYPE_ID, IN_ID, UNIFORM,

      (4 << 16) | OP_VARIABLE, POINTER_TYPE_ID, OUT_ID, UNIFORM,

      (4 << 16) | OP_VARIABLE, INT_VECTOR_POINTER_TYPE_ID, GLOBAL_INVOCATION_ID, INPUT,

      // then declare function
      (5 << 16) | OP_FUNCTION, VOID_TYPE_ID, FUNC_ID, 0, FUNC_TYPE_ID,

      (2 << 16) | OP_LABEL, LABEL_ID,

      (5 << 16) | OP_ACCESS_CHAIN, INT_POINTER_TYPE_ID, GLOBAL_INVOCATION_X_PTR_ID, GLOBAL_INVOCATION_ID, CONSTANT_ZERO_ID,

      (4 << 16) | OP_LOAD, INT_TYPE_ID, GLOBAL_INVOCATION_X_ID, GLOBAL_INVOCATION_X_PTR_ID,

      (6 << 16) | OP_ACCESS_CHAIN, ELEMENT_POINTER_TYPE_ID, IN_ELEMENT_ID, IN_ID, CONSTANT_ZERO_ID, GLOBAL_INVOCATION_X_ID,

      (4 << 16) | OP_LOAD, INT_TYPE_ID, TEMP_LOADED_ID, IN_ELEMENT_ID,

      (6 << 16) | OP_ACCESS_CHAIN, ELEMENT_POINTER_TYPE_ID, OUT_ELEMENT_ID, OUT_ID, CONSTANT_ZERO_ID, GLOBAL_INVOCATION_X_ID,

      (3 << 16) | OP_STORE, OUT_ELEMENT_ID, TEMP_LOADED_ID,

      (1 << 16) | OP_RETURN,

      (1 << 16) | OP_FUNCTION_END,
    };

    const vk::ShaderModuleCreateInfo shaderModuleCreateInfo({}, sizeof(shader), shader);

    const vk::ShaderModule shader_module = device.createShaderModule(shaderModuleCreateInfo);

    const vk::DescriptorSetLayoutBinding descriptorSetLayoutBindings[2] {
      {
        0,
        vk::DescriptorType::eStorageBuffer,
        1,
        vk::ShaderStageFlagBits::eCompute
      },
      {
        1,
        vk::DescriptorType::eStorageBuffer,
        1,
        vk::ShaderStageFlagBits::eCompute
      }    
    };

    const vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo({}, 2, descriptorSetLayoutBindings);

    const vk::DescriptorSetLayout descriptorSetLayout = device.createDescriptorSetLayout(descriptorSetLayoutCreateInfo);

    const vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo({}, 1, &descriptorSetLayout);

    const vk::PipelineLayout pipelineLayout = device.createPipelineLayout(pipelineLayoutCreateInfo);

    const vk::PipelineShaderStageCreateInfo pipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eCompute, shader_module, "f");
  
    const vk::ComputePipelineCreateInfo computePipelineCreateInfo({}, pipelineShaderStageCreateInfo, pipelineLayout);

    const vk::Pipeline pipeline = device.createComputePipeline({}, computePipelineCreateInfo);

    const vk::CommandPoolCreateInfo commandPoolCreateInfo({}, queueFamilyIndex);

    const vk::DescriptorPoolSize descriptorPoolSize(vk::DescriptorType::eStorageBuffer, 2);

    const vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo({}, 1, 1, &descriptorPoolSize);

    const vk::DescriptorPool descriptorPool = device.createDescriptorPool(descriptorPoolCreateInfo);

    const vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo(descriptorPool, 1, &descriptorSetLayout);

    const std::vector<vk::DescriptorSet> descriptorSets = device.allocateDescriptorSets(descriptorSetAllocateInfo);

    const vk::DescriptorBufferInfo in_descriptorBufferInfo(in_buffer, 0, VK_WHOLE_SIZE);

    const vk::DescriptorBufferInfo out_descriptorBufferInfo(out_buffer, 0, VK_WHOLE_SIZE);

    const vk::WriteDescriptorSet writeDescriptorSet[2] =
    {
      {
        descriptorSets[0],
        0,
        0,
        1,
        vk::DescriptorType::eStorageBuffer,
        nullptr,
        &in_descriptorBufferInfo
      },
      {
        descriptorSets[0],
        1,
        0,
        1,
        vk::DescriptorType::eStorageBuffer,
        nullptr,
        &out_descriptorBufferInfo
      }
    };

    device.updateDescriptorSets(2, writeDescriptorSet, 0, nullptr);

    const vk::CommandPool commandPool = device.createCommandPool(commandPoolCreateInfo);

    const vk::CommandBufferAllocateInfo commandBufferAllocateInfo(commandPool, vk::CommandBufferLevel::ePrimary, 1);

    const std::vector<vk::CommandBuffer> commandBuffers = device.allocateCommandBuffers(commandBufferAllocateInfo);

    const vk::CommandBufferBeginInfo commandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    commandBuffers[0].begin(commandBufferBeginInfo);

    commandBuffers[0].bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
    commandBuffers[0].bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 0, descriptorSets, {0});
    commandBuffers[0].dispatch(bufferSize / sizeof(int32_t), 1, 1);

    commandBuffers[0].end();

    const vk::Queue queue = device.getQueue(queueFamilyIndex, 0);

    const vk::SubmitInfo submitInfo(0, nullptr, nullptr, 1, commandBuffers.data());

    queue.submit({submitInfo}, vk::Fence{});
    queue.waitIdle();

    payload = static_cast<int32_t*>(device.mapMemory(memory, 0, memorySize));

    for (uint32_t k = 0, e = bufferSize / sizeof(int32_t); k < e; k++) {
      if(payload[k + e] != payload[k]) {
        std::cout << "Computation shader execution failed.\n";
        return EXIT_FAILURE;
      }
    }
  }

  std::cout << "Done.\n";

  return EXIT_SUCCESS;
}

