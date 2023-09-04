// https://www.duskborn.com/posts/a-simple-vulkan-compute-example/
// https://gist.github.com/sheredom/523f02bbad2ae397d7ed255f3f3b5a7f
// https://vulkan-tutorial.com/en/Texture_mapping/Combined_image_sampler
// export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation

#include <vulkan/vulkan.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <png++/png.hpp>
#include <string>
#include <vector>

struct Vec4 {
  float x;
  float y;
  float z;
  float w;
};

bool operator==(const Vec4& lhs, const Vec4& rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
}

struct ImageConstantData {
  uint32_t width;
  uint32_t height;
};

std::vector<char> readFile(const std::string& filepath) {
  std::ifstream file{filepath, std::ios::binary};

  if (!file) {
    throw std::runtime_error("failed to open file: " + filepath);
  }

  std::vector<char> result{std::istreambuf_iterator<char>(file),
                           std::istreambuf_iterator<char>()};
  return result;
}

#define BAIL_ON_BAD_RESULT(result)                             \
  if (VK_SUCCESS != (result)) {                                \
    fprintf(stderr, "Failure at %u %s\n", __LINE__, __FILE__); \
    exit(-1);                                                  \
  }

VkResult vkGetBestTransferQueueNPH(VkPhysicalDevice physicalDevice,
                                   uint32_t* queueFamilyIndex) {
  uint32_t queueFamilyPropertiesCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice,
                                           &queueFamilyPropertiesCount, nullptr);

  auto* const queueFamilyProperties =
      (VkQueueFamilyProperties*)alloca(sizeof(VkQueueFamilyProperties) *
                                       queueFamilyPropertiesCount);

  vkGetPhysicalDeviceQueueFamilyProperties(
      physicalDevice, &queueFamilyPropertiesCount, queueFamilyProperties);

  // first try and find a queue that has just the transfer bit set
  for (uint32_t i = 0; i < queueFamilyPropertiesCount; i++) {
    // mask out the sparse binding bit that we aren't caring about (yet!)
    const VkQueueFlags maskedFlags =
        (~VK_QUEUE_SPARSE_BINDING_BIT & queueFamilyProperties[i].queueFlags);

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
    const VkQueueFlags maskedFlags =
        (~VK_QUEUE_SPARSE_BINDING_BIT & queueFamilyProperties[i].queueFlags);

    if (!(VK_QUEUE_GRAPHICS_BIT & maskedFlags) &&
        (VK_QUEUE_COMPUTE_BIT & maskedFlags)) {
      *queueFamilyIndex = i;
      return VK_SUCCESS;
    }
  }

  // lastly get any queue that'll work for us (graphics, compute or transfer bit
  // set)
  for (uint32_t i = 0; i < queueFamilyPropertiesCount; i++) {
    // mask out the sparse binding bit that we aren't caring about (yet!)
    const VkQueueFlags maskedFlags =
        (~VK_QUEUE_SPARSE_BINDING_BIT & queueFamilyProperties[i].queueFlags);

    if ((VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT) &
        maskedFlags) {
      *queueFamilyIndex = i;
      return VK_SUCCESS;
    }
  }

  return VK_ERROR_INITIALIZATION_FAILED;
}

VkResult vkGetBestComputeQueueNPH(VkPhysicalDevice physicalDevice,
                                  uint32_t* queueFamilyIndex) {
  uint32_t queueFamilyPropertiesCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice,
                                           &queueFamilyPropertiesCount, nullptr);

  auto* const queueFamilyProperties =
      (VkQueueFamilyProperties*)alloca(sizeof(VkQueueFamilyProperties) *
                                       queueFamilyPropertiesCount);

  vkGetPhysicalDeviceQueueFamilyProperties(
      physicalDevice, &queueFamilyPropertiesCount, queueFamilyProperties);

  // first try and find a queue that has just the compute bit set
  for (uint32_t i = 0; i < queueFamilyPropertiesCount; i++) {
    // mask out the sparse binding bit that we aren't caring about (yet!) and
    // the transfer bit
    const VkQueueFlags maskedFlags =
        (~(VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT) &
         queueFamilyProperties[i].queueFlags);

    if (!(VK_QUEUE_GRAPHICS_BIT & maskedFlags) &&
        (VK_QUEUE_COMPUTE_BIT & maskedFlags)) {
      *queueFamilyIndex = i;
      return VK_SUCCESS;
    }
  }

  // lastly get any queue that'll work for us
  for (uint32_t i = 0; i < queueFamilyPropertiesCount; i++) {
    // mask out the sparse binding bit that we aren't caring about (yet!) and
    // the transfer bit
    const VkQueueFlags maskedFlags =
        (~(VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT) &
         queueFamilyProperties[i].queueFlags);

    if (VK_QUEUE_COMPUTE_BIT & maskedFlags) {
      *queueFamilyIndex = i;
      return VK_SUCCESS;
    }
  }

  return VK_ERROR_INITIALIZATION_FAILED;
}

int main(int argc, const char* const argv[]) {
  if (argc <= 1) {
    printf("Format to call: %s PNG_image\n", argv[0]);
    return EXIT_FAILURE;
  }

  const VkApplicationInfo applicationInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                             nullptr,
                                             "VKComputeSample",
                                             0,
                                             "",
                                             0,
                                             VK_MAKE_VERSION(1, 0, 9)};

  const VkInstanceCreateInfo instanceCreateInfo = {
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      nullptr,
      0,
      &applicationInfo,
      0,
      nullptr,
      0,
      nullptr};

  VkInstance instance = nullptr;
  BAIL_ON_BAD_RESULT(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));

  uint32_t physicalDeviceCount = 0;
  BAIL_ON_BAD_RESULT(
      vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr));

  auto* const physicalDevices =
      (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * physicalDeviceCount);

  BAIL_ON_BAD_RESULT(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount,
                                                physicalDevices));

  for (uint32_t i = 0; i < physicalDeviceCount; i++) {
    // Just for information
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevices[i], &props);
    printf("device name: %s\n", props.deviceName);

    uint32_t queueFamilyIndex = 0;
    BAIL_ON_BAD_RESULT(
        vkGetBestComputeQueueNPH(physicalDevices[i], &queueFamilyIndex));

    const float queuePrioritory = 1.0f;
    const VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        nullptr,
        0,
        queueFamilyIndex,
        1,
        &queuePrioritory};

    const VkDeviceCreateInfo deviceCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        nullptr,
        0,
        1,
        &deviceQueueCreateInfo,
        0,
        nullptr,
        0,
        nullptr,
        nullptr};

    VkDevice device = nullptr;
    BAIL_ON_BAD_RESULT(
        vkCreateDevice(physicalDevices[i], &deviceCreateInfo, nullptr, &device));

    VkPhysicalDeviceMemoryProperties properties;

    vkGetPhysicalDeviceMemoryProperties(physicalDevices[i], &properties);

    // Load an decode an image.
    png::image<png::rgb_pixel> image(argv[1]);
    const int32_t width = image.get_width();
    const int32_t height = image.get_height();
    const int32_t bufferLength = width * height;

    using BufferDataT = Vec4;

    const uint32_t bufferSize = sizeof(BufferDataT) * bufferLength;

    // set memoryTypeIndex to an invalid entry in the properties.memoryTypes
    // array
    uint32_t memoryTypeIndex = VK_MAX_MEMORY_TYPES;

    for (uint32_t k = 0; k < properties.memoryTypeCount; k++) {
      if ((VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT &
           properties.memoryTypes[k].propertyFlags) &&
          (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT &
           properties.memoryTypes[k].propertyFlags) &&
          (bufferSize <
           properties.memoryHeaps[properties.memoryTypes[k].heapIndex].size)) {
        memoryTypeIndex = k;
        break;
      }
    }

    BAIL_ON_BAD_RESULT(memoryTypeIndex == VK_MAX_MEMORY_TYPES
                           ? VK_ERROR_OUT_OF_HOST_MEMORY
                           : VK_SUCCESS);

    const VkMemoryAllocateInfo memoryAllocateInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, bufferSize, memoryTypeIndex};

    VkDeviceMemory memoryIn = nullptr;
    BAIL_ON_BAD_RESULT(
        vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &memoryIn));

    VkDeviceMemory memoryOut = nullptr;
    BAIL_ON_BAD_RESULT(
        vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &memoryOut));

    BufferDataT* payload = nullptr;
    BAIL_ON_BAD_RESULT(
        vkMapMemory(device, memoryIn, 0, bufferSize, 0, (void**)&payload));

    uint32_t k = 0;
    for (size_t y = 0; y < height; ++y) {
      for (size_t x = 0; x < width; ++x) {
        payload[k].x = image[y][x].red / 255.0;
        payload[k].y = image[y][x].green / 255.0;
        payload[k].z = image[y][x].blue / 255.0;
        payload[k].w = 1.0;
        k++;
      }
    }

    vkUnmapMemory(device, memoryIn);

    BAIL_ON_BAD_RESULT(
        vkMapMemory(device, memoryOut, 0, bufferSize, 0, (void**)&payload));

    for (; k < bufferSize / sizeof(BufferDataT); k++) {
      payload[k].x = rand() * 0.1;
      payload[k].y = rand() * 0.1;
      payload[k].z = rand() * 0.1;
      payload[k].w = rand() * 0.1;
    }

    vkUnmapMemory(device, memoryOut);

    const VkBufferCreateInfo bufferCreateInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        nullptr,
        0,
        bufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        1,
        &queueFamilyIndex};

    VkBuffer in_buffer = nullptr;
    BAIL_ON_BAD_RESULT(
        vkCreateBuffer(device, &bufferCreateInfo, nullptr, &in_buffer));

    BAIL_ON_BAD_RESULT(vkBindBufferMemory(device, in_buffer, memoryIn, 0));

    VkBuffer out_buffer = nullptr;
    BAIL_ON_BAD_RESULT(
        vkCreateBuffer(device, &bufferCreateInfo, nullptr, &out_buffer));

    BAIL_ON_BAD_RESULT(vkBindBufferMemory(device, out_buffer, memoryOut, 0));

    const auto compCode = readFile("./shaders/simple_shader.comp.spv");

    VkShaderModuleCreateInfo shaderModuleCreateInfo = {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0,
        compCode.size(),                  // sizeof(shader),
        (const uint32_t*)compCode.data()  // shader
    };

    VkShaderModule shader_module = nullptr;

    BAIL_ON_BAD_RESULT(vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr,
                                            &shader_module));

    VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[2] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
         nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
         nullptr}};

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 2,
        descriptorSetLayoutBindings};

    VkDescriptorSetLayout descriptorSetLayout = nullptr;
    BAIL_ON_BAD_RESULT(vkCreateDescriptorSetLayout(
        device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout));

    VkPushConstantRange pushConstantRange{
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = sizeof(ImageConstantData),
    };

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    };

    VkPipelineLayout pipelineLayout = nullptr;
    BAIL_ON_BAD_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo,
                                              nullptr, &pipelineLayout));

    VkComputePipelineCreateInfo computePipelineCreateInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        nullptr,
        0,
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
         VK_SHADER_STAGE_COMPUTE_BIT, shader_module, "main", nullptr},
        pipelineLayout,
        nullptr,
        0};

    VkPipeline pipeline = nullptr;
    BAIL_ON_BAD_RESULT(vkCreateComputePipelines(
        device, nullptr, 1, &computePipelineCreateInfo, nullptr, &pipeline));

    VkCommandPoolCreateInfo commandPoolCreateInfo = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, 0, queueFamilyIndex};

    VkDescriptorPoolSize descriptorPoolSize = {
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        nullptr,
        0,
        1,
        1,
        &descriptorPoolSize};

    VkDescriptorPool descriptorPool = nullptr;
    BAIL_ON_BAD_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo,
                                              nullptr, &descriptorPool));

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, descriptorPool, 1,
        &descriptorSetLayout};

    VkDescriptorSet descriptorSet = nullptr;
    BAIL_ON_BAD_RESULT(vkAllocateDescriptorSets(
        device, &descriptorSetAllocateInfo, &descriptorSet));

    VkDescriptorBufferInfo in_descriptorBufferInfo = {in_buffer, 0,
                                                      VK_WHOLE_SIZE};

    VkDescriptorBufferInfo out_descriptorBufferInfo = {out_buffer, 0,
                                                       VK_WHOLE_SIZE};

    VkWriteDescriptorSet writeDescriptorSet[2] = {
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 0, 0, 1,
         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &in_descriptorBufferInfo, nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 1, 0, 1,
         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &out_descriptorBufferInfo, nullptr}};

    vkUpdateDescriptorSets(device, 2, writeDescriptorSet, 0, nullptr);

    VkCommandPool commandPool = nullptr;
    BAIL_ON_BAD_RESULT(
        vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool));

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, commandPool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};

    VkCommandBuffer commandBuffer = nullptr;
    BAIL_ON_BAD_RESULT(vkAllocateCommandBuffers(
        device, &commandBufferAllocateInfo, &commandBuffer));

    VkCommandBufferBeginInfo commandBufferBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr};

    BAIL_ON_BAD_RESULT(
        vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // Push constants
    ImageConstantData push{.width = static_cast<uint32_t>(width),
                           .height = static_cast<uint32_t>(height)};

    vkCmdPushConstants(commandBuffer, pipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(ImageConstantData), &push);

    vkCmdDispatch(commandBuffer, width, height, 1);

    BAIL_ON_BAD_RESULT(vkEndCommandBuffer(commandBuffer));

    VkQueue queue = nullptr;
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

    VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &commandBuffer, 0, nullptr};

    BAIL_ON_BAD_RESULT(vkQueueSubmit(queue, 1, &submitInfo, nullptr));

    BAIL_ON_BAD_RESULT(vkQueueWaitIdle(queue));

    BufferDataT* payloadIn = nullptr;
    BAIL_ON_BAD_RESULT(
        vkMapMemory(device, memoryIn, 0, bufferSize, 0, (void**)&payloadIn));

    BufferDataT* payloadOut = nullptr;
    BAIL_ON_BAD_RESULT(
        vkMapMemory(device, memoryOut, 0, bufferSize, 0, (void**)&payloadOut));

    // Write output image
    const char* outputImageName = "output.png";
    png::image<png::rgb_pixel> outputImage(width, height);
    k = 0;
    for (size_t y = 0; y < height; ++y) {
      for (size_t x = 0; x < width; ++x) {
        outputImage[y][x].red = payloadOut[k].x * 255.0;
        outputImage[y][x].green = payloadOut[k].y * 255.0;
        outputImage[y][x].blue = payloadOut[k].z * 255.0;
        k++;
      }
    }

    outputImage.write(outputImageName);
  }

  printf("Done.\n");
}
