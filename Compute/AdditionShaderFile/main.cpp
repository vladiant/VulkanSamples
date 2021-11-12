// https://stackoverflow.com/questions/67831583/vanilla-vulkan-compute-shader-not-writing-to-output-buffer
// export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation

#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <assert.h>
#include <fstream>

// Some helper functions
typedef uint32_t            u32;
typedef uint64_t            u64;

// Vulkan two steps enumeration function
template<typename F, typename V, typename T>
void COUNT_AND_GET1(F func, V& vec, const T& arg1) {
    u32 size = 0;
    vec.clear();
    func(arg1, &size, nullptr);
    if(size > 0) {
    vec.resize(size);
    func(arg1, &size, vec.data());
    }
}

template<typename F, typename V, typename T1, typename T2>
void COUNT_AND_GET2(F func, V& vec, const T1& arg1, const T2& arg2) {
    u32 size = 0; 
    vec.clear(); 
    func(arg1, arg2, &size, nullptr); 
    if(size > 0) { 
    vec.resize(size); 
    func(arg1, arg2, &size, vec.data()); }
}

// Basic vec4 data
struct vec4
{
    u32 x; u32 y; u32 z; u32 w;
};

struct PhysicalDeviceProps
{
    VkPhysicalDeviceProperties              m_Properties;
    VkPhysicalDeviceFeatures                m_Features;
    VkPhysicalDeviceMemoryProperties        m_MemoryProperties;
    std::vector<VkQueueFamilyProperties>    m_QueueFamilyProperties;
    std::vector<VkLayerProperties>          m_LayerProperties;
    std::vector<VkExtensionProperties>      m_ExtensionProperties;
};

// Return device memory index that matches specified properties
u32 SelectMemoryHeapFrom(u32 memoryTypeBits, const VkPhysicalDeviceMemoryProperties& memoryProperties, VkMemoryPropertyFlags preferredProperties, VkMemoryPropertyFlags requiredProperties)
{
    assert((preferredProperties & requiredProperties) > 0);
    u32 selectedType = u32(-1);
    u32 memIndex = 0;
    while (memIndex < VK_MAX_MEMORY_TYPES && selectedType == u32(-1))
    {
        if (((memoryTypeBits & (1 << memIndex)) > 0)
            && ((memoryProperties.memoryTypes[memIndex].propertyFlags & preferredProperties) == preferredProperties))
        {
            // If it exactly matches my preferred properties, grab it.
            selectedType = memIndex;
        }
        ++memIndex;
    }

    if (selectedType == u32(-1))
    {
        memIndex = 0;
        while (memIndex < VK_MAX_MEMORY_TYPES && selectedType == u32(-1))
        {
            if (((memoryTypeBits & (1 << memIndex)) > 0)
                && ((memoryProperties.memoryTypes[memIndex].propertyFlags & requiredProperties) == requiredProperties))
            {
                // If it exactly matches my required properties, grab it.
                selectedType = memIndex;
            }
            ++memIndex;
        }
    }
    return selectedType;
}

// **** MAIN FUNCTION ****
void SampleCompute()
{
    // -------------------------------------
    // 1. Create Instance
    // -------------------------------------
    VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr, "SampleCompute", 0, "MyEngine", 0, VK_API_VERSION_1_2 };
    VkInstanceCreateInfo instCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr, 0, &appInfo, 0, nullptr, 0, nullptr };
    VkInstance instance = VK_NULL_HANDLE;
    if (VK_SUCCESS != vkCreateInstance(&instCreateInfo, nullptr, &instance))
        std::cout << "Instance creation failed!\n";


    // ---------------------------------------------------
    // 2. Enumerate physical devices and select 'best' one 
    // ---------------------------------------------------
    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    PhysicalDeviceProps bestDeviceProps;
    {
        std::vector<VkPhysicalDevice> physicalDevices;
        COUNT_AND_GET1(vkEnumeratePhysicalDevices, physicalDevices, instance);
        assert(!physicalDevices.empty());

        std::vector< PhysicalDeviceProps> physicalDeviceProps(physicalDevices.size());
        for (u64 i = 0; i < physicalDevices.size(); ++i)
        {
            vkGetPhysicalDeviceProperties(physicalDevices[i], &physicalDeviceProps[i].m_Properties);
            vkGetPhysicalDeviceMemoryProperties(physicalDevices[i], &physicalDeviceProps[i].m_MemoryProperties);
            COUNT_AND_GET1(vkGetPhysicalDeviceQueueFamilyProperties, physicalDeviceProps[i].m_QueueFamilyProperties, physicalDevices[i]);
            COUNT_AND_GET1(vkEnumerateDeviceLayerProperties, physicalDeviceProps[i].m_LayerProperties, physicalDevices[i]);
            COUNT_AND_GET2(vkEnumerateDeviceExtensionProperties, physicalDeviceProps[i].m_ExtensionProperties, physicalDevices[i], nullptr);
        }

        u64 bestDeviceIndex = 0;
        for (u64 i = 1; i < physicalDevices.size(); ++i)
        {
            const bool isDiscrete = physicalDeviceProps[bestDeviceIndex].m_Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
            const bool otherIsDiscrete = physicalDeviceProps[i].m_Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
            if (isDiscrete && !otherIsDiscrete)
                continue;
            else if ((!isDiscrete && otherIsDiscrete)
                || (physicalDeviceProps[bestDeviceIndex].m_Properties.limits.maxFramebufferWidth < physicalDeviceProps[i].m_Properties.limits.maxFramebufferWidth))
                bestDeviceIndex = i;
        }

        bestDevice = physicalDevices[bestDeviceIndex];
        bestDeviceProps = physicalDeviceProps[bestDeviceIndex];
    }


    // ---------------------------------------------------
    // 3. Find queue family which support compute pipeline
    // ---------------------------------------------------
    u32 computeQueue = 0;
    while (computeQueue < bestDeviceProps.m_QueueFamilyProperties.size()
        && ((bestDeviceProps.m_QueueFamilyProperties[computeQueue].queueFlags & VK_QUEUE_COMPUTE_BIT) != VK_QUEUE_COMPUTE_BIT))
    {
        ++computeQueue;
    }
    assert(computeQueue < bestDeviceProps.m_QueueFamilyProperties.size());


    // -------------------------------
    // 4. Create logical device
    // -------------------------------
    float queuePriorities[] = {1.0};
    VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, computeQueue, 1, queuePriorities};
    VkPhysicalDeviceFeatures features = {};
    VkDeviceCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, nullptr, 0,
        1, &queueInfo,
        0, nullptr,
        0, nullptr,
        &features
    };
    VkDevice device = VK_NULL_HANDLE;
    if (VK_SUCCESS != vkCreateDevice(bestDevice, &createInfo, nullptr, &device))
        std::cout << "Logical Device creation failed\n";


    // -------------------------------
    // 5. Create data buffers
    // -------------------------------
    constexpr u64 elemCount = 25;
    constexpr u64 bufferSize = elemCount * sizeof(vec4);
    VkBufferCreateInfo bufferCreateInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0,
            bufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_SHARING_MODE_EXCLUSIVE, 0, nullptr
    };

    VkBuffer inputBuffer = VK_NULL_HANDLE;
    if (VK_SUCCESS != vkCreateBuffer(device, &bufferCreateInfo, nullptr, &inputBuffer))
        std::cout << "Creating input buffer failed!\n";
    VkMemoryRequirements inputBufferMemory;
    vkGetBufferMemoryRequirements(device, inputBuffer, &inputBufferMemory);

    VkBuffer outputBuffer = VK_NULL_HANDLE;
    if (VK_SUCCESS != vkCreateBuffer(device, &bufferCreateInfo, nullptr, &outputBuffer))
        std::cout << "Creating output buffer failed!\n";
    VkMemoryRequirements outputBufferMemory;
    vkGetBufferMemoryRequirements(device, outputBuffer, &outputBufferMemory);


    // -------------------------------
    // 6. Allocate memory for buffers
    // -------------------------------
    u32 inputMemoryIndex = SelectMemoryHeapFrom(inputBufferMemory.memoryTypeBits, bestDeviceProps.m_MemoryProperties, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo inputAllocationInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, inputBufferMemory.size, inputMemoryIndex };
    VkDeviceMemory inputMemory = VK_NULL_HANDLE;
    if (VK_SUCCESS != vkAllocateMemory(device, &inputAllocationInfo, nullptr, &inputMemory))
        std::cout << "Memory allocation of " << inputBufferMemory.size << " failed!\n";

    u32 outputMemoryIndex = SelectMemoryHeapFrom(outputBufferMemory.memoryTypeBits, bestDeviceProps.m_MemoryProperties, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo outputAllocationInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, outputBufferMemory.size, outputMemoryIndex };
    VkDeviceMemory outputMemory = VK_NULL_HANDLE;
    if (VK_SUCCESS != vkAllocateMemory(device, &outputAllocationInfo, nullptr, &outputMemory))
        std::cout << "Memory allocation of " << outputBufferMemory.size << " failed!\n";


    // -------------------------------
    // 7. Bind buffers to memory
    // -------------------------------
    if (vkBindBufferMemory(device, inputBuffer, inputMemory, 0) != VK_SUCCESS)
        std::cout << "Input buffer binding failed!\n";

    if (vkBindBufferMemory(device, outputBuffer, outputMemory, 0) != VK_SUCCESS)
        std::cout << "Output buffer binding failed!\n";


    // ----------------------------------
    // 8. Map buffers and upload data
    // ----------------------------------
    vec4* inputData = nullptr;
    if (VK_SUCCESS != vkMapMemory(device, inputMemory, 0, VK_WHOLE_SIZE, 0, (void**)(&inputData)))
        std::cout << "Input memory mapping failed!\n";
    
    for (u32 i = 0; i < elemCount; ++i)
    {
        inputData[i].x = static_cast<u32>(rand() / (float)RAND_MAX * 100);
        inputData[i].y = static_cast<u32>(rand() / (float)RAND_MAX * 100);
        inputData[i].z = static_cast<u32>(rand() / (float)RAND_MAX * 100);
        inputData[i].w = static_cast<u32>(rand() / (float)RAND_MAX * 100);
        std::cout << inputData[i].x << ", " << inputData[i].y << ", " << inputData[i].z << ", " << inputData[i].w << ", ";
    }
    std::cout << "\n\n\n";
    vkUnmapMemory(device, inputMemory);

    vec4* initialOutputData = nullptr;
    if (VK_SUCCESS != vkMapMemory(device, outputMemory, 0, VK_WHOLE_SIZE, 0, (void**)(&initialOutputData)))
        std::cout << "Output memory mapping failed!\n";
    for (u32 i = 0; i < elemCount; ++i)
    {
        initialOutputData[i].x = 2; initialOutputData[i].z = 2; initialOutputData[i].y = 2; initialOutputData[i].w = 2;
    }
    vkUnmapMemory(device, outputMemory);


    // ----------------------------------
    // 9. Create shader/pipeline layout
    // ----------------------------------
    std::vector<VkDescriptorSetLayoutBinding> bindings = {
            { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
            { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr }
    };
    VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 2, bindings.data() };
    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    if (VK_SUCCESS != vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorLayout))
        std::cout << "Descriptor Layout creation failed!\n";

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, &descriptorLayout, 0, nullptr };
    VkPipelineLayout layout = VK_NULL_HANDLE;
    if (VK_SUCCESS != vkCreatePipelineLayout(device, &pipelineCreateInfo, nullptr, &layout))
        std::cout << "Pipeline Layout creation failed\n";


    // --------------------------------------------------
    // 10. Load shader source and create shader module
    // --------------------------------------------------
    std::ifstream file("./shaders/simple_shader.comp.spv", std::ifstream::binary);
    u64 size = 0;
    if (!file.is_open())
        std::cout << "Can't open shader!\n";
    
    file.seekg(0, file.end);
    size = file.tellg();
    file.seekg(0);
    char* shaderSrc = new char[size];
    file.read(shaderSrc, size);

    VkShaderModuleCreateInfo shaderCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0, size, reinterpret_cast<u32*>(shaderSrc) };
    VkShaderModule shader = VK_NULL_HANDLE;
    if (VK_SUCCESS != vkCreateShaderModule(device, &shaderCreateInfo, nullptr, &shader))
        std::cout << "Shader Module creation failed\n";
    delete[] shaderSrc;
    

    // ----------------------------------
    // 10.5. Create descriptor sets
    // ----------------------------------
    VkDescriptorPoolSize descriptorPoolSize = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 };
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
          VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0,
          1, 1, &descriptorPoolSize };
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, 0, &descriptorPool);

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
          VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, 0,
          descriptorPool, 1, &descriptorLayout
    };
    VkDescriptorSet descriptorSet;
    vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet);

    VkDescriptorBufferInfo inputBufferDescriptorInfo = { inputBuffer, 0, VK_WHOLE_SIZE };
    VkDescriptorBufferInfo outputBufferDescriptorInfo = { outputBuffer, 0, VK_WHOLE_SIZE };
    VkWriteDescriptorSet writeDescriptorSet[2] = {
          {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descriptorSet,
            0, 0, 1,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            0, &inputBufferDescriptorInfo, 0
          },
          {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, descriptorSet, 
            1, 0, 1,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            0, &outputBufferDescriptorInfo, 0
          }
    };

    vkUpdateDescriptorSets(device, 2, writeDescriptorSet, 0, nullptr);
    
    // -------------------------------
    // 11. Create compute pipeline
    // -------------------------------
    const char* entryPointName = "main";
    VkComputePipelineCreateInfo computeCreateInfo = {
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, nullptr, 0,
            {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                VK_SHADER_STAGE_COMPUTE_BIT, shader,
                entryPointName, nullptr
            },
            layout, VK_NULL_HANDLE, 0
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (VK_SUCCESS != vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computeCreateInfo, nullptr, &pipeline))
        std::cout << "Compute Pipeline creation failed!\n";


    // ------------------------------------------------
    // 12. Create Command Pool and Command Buffer
    // --------------------------------------------------
    VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, 0, computeQueue };
    VkCommandPool cmdPool = VK_NULL_HANDLE;
    if (VK_SUCCESS != vkCreateCommandPool(device, &poolInfo, nullptr, &cmdPool))
        std::cout << "Command Pool creation failed!\n";

    VkCommandBufferAllocateInfo cmdBufferInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr,
            cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1
    };
    VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
    if (VK_SUCCESS != vkAllocateCommandBuffers(device, &cmdBufferInfo, &cmdBuffer))
        std::cout << "Command buffer allocation failed!\n";

    // ---------------------------
    // 13. Run compute shader
    // ---------------------------
    VkCommandBufferUsageFlags flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr };
    vkBeginCommandBuffer(cmdBuffer, &beginInfo);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &descriptorSet, 0, 0);
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdDispatch(cmdBuffer, 8, 1, 1);

    // Optional
    VkBufferMemoryBarrier outbuffDependency = {};
    outbuffDependency.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    outbuffDependency.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    outbuffDependency.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    outbuffDependency.buffer = outputBuffer;
    outbuffDependency.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        cmdBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
        (VkDependencyFlags)0,
        0, nullptr,
        1, &outbuffDependency,
        0, nullptr
    );
    
    vkEndCommandBuffer(cmdBuffer);

    // -----------------------------------------
    // 14. Submit command buffer (with fence)
    // -----------------------------------------
    VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, (VkFenceCreateFlags)0 };
    VkFence fence = VK_NULL_HANDLE;
    if (VK_SUCCESS != vkCreateFence(device, &fenceCreateInfo, nullptr, &fence))
        std::cout << "Fence creation failed!\n";

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, computeQueue, 0, &queue);

    VkSubmitInfo submitInfo = { 
        VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, 0,
        1, &cmdBuffer, 0, nullptr
    };
    VkResult result = vkQueueSubmit(queue, 1, &submitInfo, fence);

    // Wait for everything finished
    if (result == VK_SUCCESS)
    {
        result = vkQueueWaitIdle(queue);
    }
    vkWaitForFences(device, 1, &fence, VK_TRUE, u64(-1));

    // ---------------------------------
    // 15. Grab and display results
    // ---------------------------------
    vec4* resultData = nullptr;
    if (VK_SUCCESS != vkMapMemory(device, outputMemory, 0, VK_WHOLE_SIZE, 0, (void**)(&resultData)))
        std::cout << "Output memory mapping failed!\n";
    for (u32 i = 0; i < elemCount; ++i)
    {
        std::cout << resultData[i].x << ", " << resultData[i].y << ", " << resultData[i].z << ", " << resultData[i].w << ", ";
    }
    std::cout << "\n\n\n";
    vkUnmapMemory(device, outputMemory);

    // ------------------------
    // 16. Resources Cleanup
    // ------------------------
    vkFreeCommandBuffers(device, cmdPool, 1, &cmdBuffer);
    vkDestroyCommandPool(device, cmdPool, nullptr);
    vkDestroyFence(device, fence, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, layout, nullptr);
    vkDestroyShaderModule(device, shader, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorLayout, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    vkDestroyBuffer(device, inputBuffer, nullptr);
    vkDestroyBuffer(device, outputBuffer, nullptr);
    vkFreeMemory(device, inputMemory, nullptr);
    vkFreeMemory(device, outputMemory, nullptr);

    if (VK_SUCCESS != vkDeviceWaitIdle(device))
        std::cout << "Can't wait for device to idle\n";
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
}

int main() {
  SampleCompute();
  return 0;
}