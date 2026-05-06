// ─────────────────────────────────────────────────────────────────────────────
//  Nexus GFX — VulkanDevice implementation
// ─────────────────────────────────────────────────────────────────────────────
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "VulkanAllocator.h"
#include "VulkanCommandBuffer.h"   // defines VulkanResourcePool
#include "VulkanBuffer.h"
#include "VulkanTexture.h"
#include "VulkanRayTracing.h"
#include "VulkanUtils.h"
#include <nexus/gfx/CommandBuffer.h>
#include <nexus/gfx/vulkan/ShaderCompiler.h>

// Include VMA implementation header (without VMA_IMPLEMENTATION — only VulkanAllocator.cpp defines that)
#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

#include <stdexcept>
#include <cstring>
#include <vector>
#include <array>
#include <string>
#include <algorithm>
#include <cstdio>

namespace nexus::gfx {
using namespace vkutil;

// ── PSO Caching helpers ───────────────────────────────────────────────────────
namespace {
    struct DescriptorLayoutKeyItem {
        uint32_t binding = 0;
        DescriptorType type = DescriptorType::SampledTexture;
    };

    // Compute FNV-1a 64-bit hash for memory regions
    inline uint64_t fnv1a_hash(const void* data, size_t size) {
        constexpr uint64_t FNV_offset_basis = 14695981039346656037ULL;
        constexpr uint64_t FNV_prime = 1099511628211ULL;
        uint64_t hash = FNV_offset_basis;
        const auto* bytes = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i) {
            hash ^= bytes[i];
            hash *= FNV_prime;
        }
        return hash;
    }

    uint64_t hashGraphicsPipelineDesc(const GraphicsPipelineDesc& desc) {
        uint64_t hash = 0;
        hash ^= fnv1a_hash(&desc.vertexShader,        sizeof(desc.vertexShader));
        hash ^= fnv1a_hash(&desc.fragmentShader,      sizeof(desc.fragmentShader));
        hash ^= fnv1a_hash(&desc.topology,            sizeof(desc.topology));
        hash ^= fnv1a_hash(&desc.polygonMode,         sizeof(desc.polygonMode));
        hash ^= fnv1a_hash(&desc.cullMode,            sizeof(desc.cullMode));
        hash ^= fnv1a_hash(&desc.frontFace,           sizeof(desc.frontFace));
        hash ^= fnv1a_hash(&desc.depthTest,           sizeof(desc.depthTest));
        hash ^= fnv1a_hash(&desc.depthWrite,          sizeof(desc.depthWrite));
        hash ^= fnv1a_hash(&desc.depthCompare,        sizeof(desc.depthCompare));
        hash ^= fnv1a_hash(&desc.stencilEnable,       sizeof(desc.stencilEnable));
        hash ^= fnv1a_hash(&desc.blendEnable,         sizeof(desc.blendEnable));
        hash ^= fnv1a_hash(&desc.colorWriteMask,      sizeof(desc.colorWriteMask));
        hash ^= fnv1a_hash(&desc.srcColorBlend,       sizeof(desc.srcColorBlend));
        hash ^= fnv1a_hash(&desc.dstColorBlend,       sizeof(desc.dstColorBlend));
        hash ^= fnv1a_hash(&desc.colorBlendOp,        sizeof(desc.colorBlendOp));
        hash ^= fnv1a_hash(&desc.srcAlphaBlend,       sizeof(desc.srcAlphaBlend));
        hash ^= fnv1a_hash(&desc.dstAlphaBlend,       sizeof(desc.dstAlphaBlend));
        hash ^= fnv1a_hash(&desc.alphaBlendOp,        sizeof(desc.alphaBlendOp));
        hash ^= fnv1a_hash(&desc.lineWidth,           sizeof(desc.lineWidth));
        hash ^= fnv1a_hash(&desc.colorAttachmentFormat, sizeof(desc.colorAttachmentFormat));
        hash ^= fnv1a_hash(&desc.depthAttachmentFormat, sizeof(desc.depthAttachmentFormat));
        return hash;
    }

    uint64_t hashMeshShaderPipelineDesc(const MeshShaderPipelineDesc& desc) {
        uint64_t hash = 0;
        hash ^= fnv1a_hash(&desc.taskShader,           sizeof(desc.taskShader));
        hash ^= fnv1a_hash(&desc.meshShader,           sizeof(desc.meshShader));
        hash ^= fnv1a_hash(&desc.fragmentShader,       sizeof(desc.fragmentShader));
        hash ^= fnv1a_hash(&desc.depthTest,            sizeof(desc.depthTest));
        hash ^= fnv1a_hash(&desc.depthWrite,           sizeof(desc.depthWrite));
        return hash;
    }

    uint64_t hashComputePipelineDesc(const ComputePipelineDesc& desc) {
        uint64_t hash = 0;
        hash ^= fnv1a_hash(&desc.computeShader, sizeof(desc.computeShader));
        return hash;
    }

    uint64_t hashRayTracingPipelineDesc(const RayTracingPipelineDesc& desc) {
        uint64_t hash = 0;
        hash ^= fnv1a_hash(&desc.rayGenShader,         sizeof(desc.rayGenShader));
        if (desc.missShaders.data()) 
            hash ^= fnv1a_hash(desc.missShaders.data(), desc.missShaders.size_bytes());
        if (desc.closestHitShaders.data())
            hash ^= fnv1a_hash(desc.closestHitShaders.data(), desc.closestHitShaders.size_bytes());
        if (desc.anyHitShaders.data())
            hash ^= fnv1a_hash(desc.anyHitShaders.data(), desc.anyHitShaders.size_bytes());
        hash ^= fnv1a_hash(&desc.maxRecursionDepth,    sizeof(desc.maxRecursionDepth));
        return hash;
    }
}


// ── Handle pool ───────────────────────────────────────────────────────────────
// VulkanResourcePool is defined in VulkanCommandBuffer.h.
// We alias it here for VulkanDevice::Impl.
struct VulkanDevice::Impl : VulkanResourcePool {
    struct DescriptorLayoutEntry {
        std::vector<DescriptorLayoutKeyItem> key;
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    };

    struct DescriptorPoolBucket {
        std::vector<VkDescriptorPoolSize> sizes;
        std::vector<VkDescriptorPool> pools;
    };

    VmaAllocator  vma   = nullptr;
    RTPfnTable    rt;
    VkCommandPool transferPool = VK_NULL_HANDLE;

    // Descriptor set allocator state
    std::unordered_map<uint64_t, std::vector<DescriptorLayoutEntry>> descriptorLayoutCache;
    std::unordered_map<uint64_t, std::vector<DescriptorPoolBucket>> descriptorPoolBuckets;

    // Per-CmdBufHandle ICommandBuffer wrappers (created on first getCommandBuffer)
    std::vector<std::unique_ptr<VulkanCommandBuffer>> cmdBufWrappers;
};

// ── Constructor / Destructor ──────────────────────────────────────────────────
VulkanDevice::VulkanDevice()
    : m_pool(std::make_unique<Impl>())
{}

VulkanDevice::~VulkanDevice()
{
    waitIdle();
    // Destroy RT accel structs
    for (auto& as : m_pool->accelStructs)
        nexus::gfx::destroyAccelStruct(m_pool->vma, m_device, m_pool->rt, as);
    // Destroy samplers
    for (auto s : m_pool->samplers) if (s) vkDestroySampler(m_device, s, nullptr);
    // Destroy descriptor set layouts and pool buckets
    for (auto& [_, entries] : m_pool->descriptorLayoutCache) {
        for (auto& entry : entries) {
            if (entry.layout) vkDestroyDescriptorSetLayout(m_device, entry.layout, nullptr);
        }
    }
    for (auto& [_, buckets] : m_pool->descriptorPoolBuckets) {
        for (auto& bucket : buckets) {
            for (auto pool : bucket.pools) {
                if (pool) vkDestroyDescriptorPool(m_device, pool, nullptr);
            }
        }
    }
    // Destroy query pools
    for (auto qp : m_pool->queryPools) if (qp) vkDestroyQueryPool(m_device, qp, nullptr);
    // Destroy pipelines
    for (auto& pe : m_pool->pipelines) {
        if (pe.pipeline) vkDestroyPipeline(m_device, pe.pipeline, nullptr);
        if (pe.layout)   vkDestroyPipelineLayout(m_device, pe.layout, nullptr);
    }
    // Destroy shaders
    for (auto s : m_pool->shaders)
        if (s) vkDestroyShaderModule(m_device, s, nullptr);
    // Destroy fences / semaphores
    for (auto f : m_pool->fences)     if (f) vkDestroyFence    (m_device, f, nullptr);
    for (auto s : m_pool->semaphores) if (s) vkDestroySemaphore(m_device, s, nullptr);
    // Destroy cmd pools (each cmdBuf has its own pool for independent reset)
    m_pool->cmdBufWrappers.clear();
    for (auto p : m_pool->cmdPools)   if (p) vkDestroyCommandPool(m_device, p, nullptr);
    if (m_pool->transferPool) vkDestroyCommandPool(m_device, m_pool->transferPool, nullptr);
    // Destroy buffers / textures via VMA
    for (auto& b : m_pool->buffers)
        if (b.handle) nexus::gfx::vkDestroyBuffer(m_pool->vma, b);
    for (auto& t : m_pool->textures) {
        if (t.isExternalImage) continue;  // swapchain images owned by ISwapchain
        if (t.defaultView) vkDestroyImageView(m_device, t.defaultView, nullptr);
        if (!t.isSparse && t.image && t.allocation)
            nexus::gfx::vkDestroyTexture(m_pool->vma, m_device, t);
        else if (t.isSparse && t.image)
            vkDestroyImage(m_device, t.image, nullptr);
    }
    if (m_pool->vma) vmaDestroyAllocator(m_pool->vma);

    if (m_debugMessenger != VK_NULL_HANDLE) {
        auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (fn) fn(m_instance, m_debugMessenger, nullptr);
    }
    if (m_device   != VK_NULL_HANDLE) vkDestroyDevice  (m_device,   nullptr);
    if (m_instance != VK_NULL_HANDLE) vkDestroyInstance(m_instance, nullptr);
}

// ── init ──────────────────────────────────────────────────────────────────────
void VulkanDevice::init(const RenderContextDesc& desc)
{
    createInstance(desc);
    setupDebugMessenger(desc.validation);
    selectPhysicalDevice();
    createLogicalDevice(desc.enableMeshShaders, desc.enableRayTracing);
    queryCapabilities();
    m_tier = classifyTier();

    // Initialize VMA
    {
        VmaVulkanFunctions vkFns{};
        vkFns.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vkFns.vkGetDeviceProcAddr   = vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo aci{};
        aci.instance         = m_instance;
        aci.physicalDevice   = m_physDevice;
        aci.device           = m_device;
        aci.vulkanApiVersion = VK_API_VERSION_1_3;
        aci.pVulkanFunctions = &vkFns;
        aci.flags            = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT
                             | VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
        if (vmaCreateAllocator(&aci, &m_pool->vma) != VK_SUCCESS)
            throw std::runtime_error("VulkanDevice: vmaCreateAllocator failed");
    }

    // Load RT function pointers if enabled
    if (m_caps.rayTracingTier >= 1)
        m_pool->rt.load(m_device);

    // Create a persistent transfer command pool for buffer/texture uploads
    {
        VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pci.queueFamilyIndex = m_queues[static_cast<size_t>(QueueType::Transfer)].family;
        pci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkCreateCommandPool(m_device, &pci, nullptr, &m_pool->transferPool);
    }
}

// ── Instance creation ─────────────────────────────────────────────────────────
void VulkanDevice::createInstance(const RenderContextDesc& desc)
{
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = desc.appName.data();
    appInfo.applicationVersion = desc.appVersion;
    appInfo.pEngineName        = "NexusKernel";
    appInfo.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    std::vector<const char*> layers;
    if (desc.validation != ValidationLevel::Off) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(NEXUS_PLATFORM_LINUX)
        "VK_KHR_xcb_surface",
        "VK_KHR_xlib_surface",
        "VK_KHR_wayland_surface",
#elif defined(NEXUS_PLATFORM_WINDOWS)
        "VK_KHR_win32_surface",
#elif defined(NEXUS_PLATFORM_MACOS)
        "VK_EXT_metal_surface",
#endif
    };
    if (desc.validation != ValidationLevel::Off) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &appInfo;
    ci.enabledLayerCount       = static_cast<uint32_t>(layers.size());
    ci.ppEnabledLayerNames     = layers.data();
    ci.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();

    VkResult res = vkCreateInstance(&ci, nullptr, &m_instance);
    if (res != VK_SUCCESS)
        throw std::runtime_error("vkCreateInstance failed: " + std::to_string(res));
}

// ── Physical device selection ─────────────────────────────────────────────────
void VulkanDevice::selectPhysicalDevice()
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (count == 0) throw std::runtime_error("No Vulkan-capable GPU found");

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

    VkPhysicalDevice best = VK_NULL_HANDLE;
    uint32_t bestScore = 0;

    for (auto pd : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(pd, &props);

        uint32_t score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)   score += 10000;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 1000;

        VkPhysicalDeviceMemoryProperties mem{};
        vkGetPhysicalDeviceMemoryProperties(pd, &mem);
        for (uint32_t i = 0; i < mem.memoryHeapCount; ++i)
            if (mem.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                score += static_cast<uint32_t>(mem.memoryHeaps[i].size >> 20);

        if (score > bestScore) { bestScore = score; best = pd; }
    }

    m_physDevice = best;

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(m_physDevice, &props);
    m_deviceName = props.deviceName;
}

// ── Logical device ────────────────────────────────────────────────────────────
void VulkanDevice::createLogicalDevice(bool enableMesh, bool enableRT)
{
    uint32_t qfCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice, &qfCount, nullptr);
    std::vector<VkQueueFamilyProperties> qfProps(qfCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice, &qfCount, qfProps.data());

    m_queues[0].family = m_queues[1].family = m_queues[2].family = UINT32_MAX;
    for (uint32_t i = 0; i < qfCount; ++i) {
        auto flags = qfProps[i].queueFlags;
        if ((flags & VK_QUEUE_GRAPHICS_BIT) && m_queues[0].family == UINT32_MAX)
            m_queues[0].family = i;
        if ((flags & VK_QUEUE_COMPUTE_BIT) && !(flags & VK_QUEUE_GRAPHICS_BIT) && m_queues[1].family == UINT32_MAX)
            m_queues[1].family = i;
        if ((flags & VK_QUEUE_TRANSFER_BIT) && !(flags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) && m_queues[2].family == UINT32_MAX)
            m_queues[2].family = i;
    }
    if (m_queues[1].family == UINT32_MAX) m_queues[1].family = m_queues[0].family;
    if (m_queues[2].family == UINT32_MAX) m_queues[2].family = m_queues[0].family;

    float prio = 1.f;
    std::vector<VkDeviceQueueCreateInfo> qcis;
    for (uint32_t f : {m_queues[0].family, m_queues[1].family, m_queues[2].family}) {
        bool dup = false;
        for (auto& q : qcis) if (q.queueFamilyIndex == f) { dup = true; break; }
        if (!dup) {
            VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            qi.queueFamilyIndex = f;
            qi.queueCount       = 1;
            qi.pQueuePriorities = &prio;
            qcis.push_back(qi);
        }
    }

    std::vector<const char*> exts = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    };
    if (enableMesh) exts.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
    if (enableRT) {
        exts.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        exts.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        exts.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
        exts.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    }

    VkPhysicalDeviceFeatures supportedFeatures{};
    vkGetPhysicalDeviceFeatures(m_physDevice, &supportedFeatures);

    VkPhysicalDeviceFeatures baseFeatures{};
    if (supportedFeatures.sparseBinding == VK_TRUE)
        baseFeatures.sparseBinding = VK_TRUE;
    if (supportedFeatures.sparseResidencyImage2D == VK_TRUE)
        baseFeatures.sparseResidencyImage2D = VK_TRUE;

    VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    features12.bufferDeviceAddress                       = VK_TRUE;
    features12.descriptorIndexing                        = VK_TRUE;
    features12.runtimeDescriptorArray                    = VK_TRUE;
    features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    features12.timelineSemaphore                         = VK_TRUE;

    VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;
    features13.pNext            = &features12;

    VkPhysicalDeviceMeshShaderFeaturesEXT meshFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    meshFeatures.meshShader = VK_TRUE;
    meshFeatures.taskShader = VK_TRUE;
    meshFeatures.pNext      = &features13;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    asFeatures.accelerationStructure = VK_TRUE;
    asFeatures.pNext = enableMesh ? static_cast<void*>(&meshFeatures) : static_cast<void*>(&features13);

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
    rtFeatures.rayTracingPipeline = VK_TRUE;
    rtFeatures.pNext              = &asFeatures;

    VkPhysicalDeviceRayQueryFeaturesKHR rqFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
    rqFeatures.rayQuery = VK_TRUE;
    rqFeatures.pNext    = &rtFeatures;

    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount    = static_cast<uint32_t>(qcis.size());
    dci.pQueueCreateInfos       = qcis.data();
    dci.enabledExtensionCount   = static_cast<uint32_t>(exts.size());
    dci.ppEnabledExtensionNames = exts.data();
    dci.pEnabledFeatures        = &baseFeatures;
    dci.pNext = enableRT   ? static_cast<void*>(&rqFeatures)
              : enableMesh ? static_cast<void*>(&meshFeatures)
              : static_cast<void*>(&features13);

    if (vkCreateDevice(m_physDevice, &dci, nullptr, &m_device) != VK_SUCCESS)
        throw std::runtime_error("vkCreateDevice failed");

    for (auto& q : m_queues)
        if (q.family != UINT32_MAX)
            vkGetDeviceQueue(m_device, q.family, 0, &q.handle);
}

// ── Capability query ──────────────────────────────────────────────────────────
void VulkanDevice::queryCapabilities()
{
    VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    VkPhysicalDeviceMeshShaderPropertiesEXT meshProps{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT};
    VkPhysicalDeviceMeshShaderFeaturesEXT   meshFeat {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeat{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeat{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
    VkPhysicalDeviceRayQueryFeaturesKHR rqFeat{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};

    meshFeat.pNext = &meshProps;
    asFeat.pNext   = &meshFeat;
    rtFeat.pNext   = &asFeat;
    rqFeat.pNext   = &rtFeat;
    f2.pNext       = &rqFeat;
    vkGetPhysicalDeviceFeatures2(m_physDevice, &f2);

    m_caps.meshShaders        = meshFeat.meshShader == VK_TRUE;
    m_caps.rayTracingPipeline = rtFeat.rayTracingPipeline == VK_TRUE;
    m_caps.rayQuery           = rqFeat.rayQuery == VK_TRUE;
    m_caps.maxMeshletVerts    = meshProps.maxMeshOutputVertices;
    m_caps.maxMeshletPrims    = meshProps.maxMeshOutputPrimitives;
    m_caps.tiledResources     = (f2.features.sparseBinding == VK_TRUE)
                             && (f2.features.sparseResidencyImage2D == VK_TRUE);
    m_caps.asyncCompute       = (m_queues[1].family != m_queues[0].family);
    if (m_caps.rayTracingPipeline) m_caps.rayTracingTier = 2;
    else if (m_caps.rayQuery)      m_caps.rayTracingTier = 1;

    VkPhysicalDeviceSubgroupProperties sg{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    p2.pNext = &sg;
    vkGetPhysicalDeviceProperties2(m_physDevice, &p2);
    m_caps.minSubgroupSize = sg.subgroupSize;
    m_caps.maxSubgroupSize = sg.subgroupSize;
}

// ── Tier classification ───────────────────────────────────────────────────────
HardwareTier VulkanDevice::classifyTier() const noexcept
{
    VkPhysicalDeviceMemoryProperties mem{};
    vkGetPhysicalDeviceMemoryProperties(m_physDevice, &mem);
    uint64_t vram = 0;
    for (uint32_t i = 0; i < mem.memoryHeapCount; ++i)
        if (mem.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            vram = std::max(vram, mem.memoryHeaps[i].size);
    constexpr uint64_t GB = 1024ULL * 1024 * 1024;
    if (vram >= 16*GB && m_caps.meshShaders && m_caps.rayTracingTier == 2) return HardwareTier::Ultra;
    if (vram >= 8*GB  && m_caps.rayTracingTier >= 1)                       return HardwareTier::High;
    if (vram >= 4*GB)                                                       return HardwareTier::Mid;
    return HardwareTier::Low;
}

// ── Debug messenger ────────────────────────────────────────────────────────────
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        fprintf(stderr, "[Vulkan] %s\n", data->pMessage);
    return VK_FALSE;
}

void VulkanDevice::setupDebugMessenger(ValidationLevel level)
{
    if (level == ValidationLevel::Off) return;
    auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
    if (!fn) return;

    VkDebugUtilsMessengerCreateInfoEXT ci{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                       | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    if (level == ValidationLevel::Full)
        ci.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                           |  VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                   | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                   | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback = debugCallback;
    fn(m_instance, &ci, nullptr, &m_debugMessenger);
}

// ── waitIdle + queue accessors ────────────────────────────────────────────────
void VulkanDevice::waitIdle() {
    if (m_device != VK_NULL_HANDLE) vkDeviceWaitIdle(m_device);
}
VkQueue  VulkanDevice::queue(QueueType t)       const noexcept { return m_queues[static_cast<size_t>(t)].handle; }
uint32_t VulkanDevice::queueFamily(QueueType t) const noexcept { return m_queues[static_cast<size_t>(t)].family; }
VulkanResourcePool* VulkanDevice::resourcePool() noexcept { return m_pool.get(); }

// ── Buffer ────────────────────────────────────────────────────────────────────
BufferHandle VulkanDevice::createBuffer(const BufferDesc& desc)
{
    VulkanBuffer buf = vkCreateBuffer(m_pool->vma, m_device, desc);
    BufferHandle h{}; h.id = m_pool->nextBufferId;
    if (m_pool->nextBufferId >= m_pool->buffers.size())
        m_pool->buffers.resize(m_pool->nextBufferId + 1);
    m_pool->buffers[m_pool->nextBufferId++] = buf;
    return h;
}

void VulkanDevice::destroyBuffer(BufferHandle h) {
    if (!h.valid() || h.id >= m_pool->buffers.size()) return;
    nexus::gfx::vkDestroyBuffer(m_pool->vma, m_pool->buffers[h.id]);
    m_pool->buffers[h.id] = {};
}

// ── Texture ───────────────────────────────────────────────────────────────────
TextureHandle VulkanDevice::createTexture(const TextureDesc& desc)
{
    VulkanTexture tex = vkCreateTexture(m_pool->vma, m_device, desc);
    TextureHandle h{}; h.id = m_pool->nextTextureId;
    if (m_pool->nextTextureId >= m_pool->textures.size())
        m_pool->textures.resize(m_pool->nextTextureId + 1);
    m_pool->textures[m_pool->nextTextureId++] = tex;
    return h;
}

void VulkanDevice::destroyTexture(TextureHandle h) {
    if (!h.valid() || h.id >= m_pool->textures.size()) return;
    vkDestroyTexture(m_pool->vma, m_device, m_pool->textures[h.id]);
    m_pool->textures[h.id] = {};
}

// ── Shader ────────────────────────────────────────────────────────────────────
ShaderHandle VulkanDevice::createShader(const ShaderDesc& desc)
{
    std::vector<uint32_t> compiledSpirv;
    std::span<const uint32_t> shaderCode = desc.spirvCode;

    if (shaderCode.empty()) {
        vkshader::ShaderCompileOutput out;
        if (!desc.sourcePath.empty()) {
            if (!vkshader::loadShaderFileToSpirv(desc.sourcePath, desc.stage, out, desc.entryPoint)) {
                std::fprintf(stderr, "createShader: failed to load '%.*s': %s\n",
                             static_cast<int>(desc.sourcePath.size()), desc.sourcePath.data(),
                             out.errorLog.c_str());
                return {};
            }
            compiledSpirv = std::move(out.spirv);
        } else if (!desc.glslSource.empty()) {
            if (!vkshader::compileGlslToSpirv(desc.glslSource, desc.stage, out,
                                              desc.entryPoint ? desc.entryPoint : "main",
                                              desc.debugName ? desc.debugName : "inline-glsl")) {
                std::fprintf(stderr, "createShader: GLSL compile failed (%s): %s\n",
                             desc.debugName ? desc.debugName : "unnamed",
                             out.errorLog.c_str());
                return {};
            }
            compiledSpirv = std::move(out.spirv);
        } else {
            std::fprintf(stderr, "createShader: no shader input provided (spirvCode/sourcePath/glslSource)\n");
            return {};
        }
        shaderCode = compiledSpirv;
    }

    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = shaderCode.size_bytes();
    ci.pCode    = shaderCode.data();
    VkShaderModule mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(m_device, &ci, nullptr, &mod) != VK_SUCCESS) return {};
    ShaderHandle h{}; h.id = m_pool->nextShaderId;
    if (m_pool->nextShaderId >= m_pool->shaders.size())
        m_pool->shaders.resize(m_pool->nextShaderId + 1, VK_NULL_HANDLE);
    m_pool->shaders[m_pool->nextShaderId++] = mod;
    return h;
}
void VulkanDevice::destroyShader(ShaderHandle h) {
    if (h.valid() && h.id < m_pool->shaders.size())
        vkDestroyShaderModule(m_device, m_pool->shaders[h.id], nullptr);
}

// ── RenderPass (metadata only — runtime uses dynamic rendering) ───────────────
RenderPassHandle VulkanDevice::createRenderPass(const RenderPassDesc&) {
    RenderPassHandle h{}; h.id = m_pool->nextPassId++; return h;
}
void VulkanDevice::destroyRenderPass(RenderPassHandle) {}

// ── Pipeline stubs (full pipeline creation goes in VulkanPipeline.cpp) ────────
PipelineHandle VulkanDevice::createGraphicsPipeline(const GraphicsPipelineDesc& desc)
{
    if (!desc.vertexShader.valid() || !desc.fragmentShader.valid()) {
        std::fprintf(stderr, "createGraphicsPipeline: vertex/fragment shader handles are required\n");
        return {};
    }
    if (desc.vertexShader.id >= m_pool->shaders.size() ||
        desc.fragmentShader.id >= m_pool->shaders.size()) {
        std::fprintf(stderr, "createGraphicsPipeline: shader handle out of range\n");
        return {};
    }

    // Check PSO cache first
    {
        const uint64_t descHash = hashGraphicsPipelineDesc(desc);
        auto it = m_pool->psoCacheGraphics.find(descHash);
        if (it != m_pool->psoCacheGraphics.end()) {
            PipelineHandle h{};
            h.id = it->second;
            return h;
        }
    }

    const VkShaderModule vs = m_pool->shaders[desc.vertexShader.id];
    const VkShaderModule fs = m_pool->shaders[desc.fragmentShader.id];
    if (vs == VK_NULL_HANDLE || fs == VK_NULL_HANDLE) {
        std::fprintf(stderr, "createGraphicsPipeline: shader module is null\n");
        return {};
    }

    // 128-byte push constant block visible to all shader stages
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_ALL;
    pcRange.offset     = 0;
    pcRange.size       = 128;

    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.setLayoutCount         = 0;
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges    = &pcRange;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(m_device, &lci, nullptr, &layout) != VK_SUCCESS) {
        std::fprintf(stderr, "createGraphicsPipeline: vkCreatePipelineLayout failed\n");
        return {};
    }

    auto toVkTopology = [](Topology t) -> VkPrimitiveTopology {
        switch (t) {
        case Topology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case Topology::LineList:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case Topology::LineStrip:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case Topology::PointList:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case Topology::TriangleList:
        default:                      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        }
    };

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs;
    stages[0].pName  = "main";

    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs;
    stages[1].pName  = "main";

    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = toVkTopology(desc.topology);
    ia.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.depthClampEnable        = VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.polygonMode             = vkutil::toVkPolygonMode(desc.polygonMode);
    rs.cullMode                = vkutil::toVkCullMode(desc.cullMode);
    rs.frontFace               = vkutil::toVkFrontFace(desc.frontFace);
    rs.depthBiasEnable         = VK_FALSE;
    rs.lineWidth               = desc.lineWidth;

    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable       = desc.depthTest   ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable      = desc.depthWrite  ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp        = vkutil::toVkCompareOp(desc.depthCompare);
    ds.depthBoundsTestEnable = VK_FALSE;
    ds.stencilTestEnable     = desc.stencilEnable ? VK_TRUE : VK_FALSE;

    VkPipelineColorBlendAttachmentState cbAttach{};
    cbAttach.blendEnable         = desc.blendEnable ? VK_TRUE : VK_FALSE;
    cbAttach.colorWriteMask      = static_cast<VkColorComponentFlags>(desc.colorWriteMask & 0xF);
    if (desc.blendEnable) {
        cbAttach.srcColorBlendFactor = vkutil::toVkBlendFactor(desc.srcColorBlend);
        cbAttach.dstColorBlendFactor = vkutil::toVkBlendFactor(desc.dstColorBlend);
        cbAttach.colorBlendOp        = vkutil::toVkBlendOp(desc.colorBlendOp);
        cbAttach.srcAlphaBlendFactor = vkutil::toVkBlendFactor(desc.srcAlphaBlend);
        cbAttach.dstAlphaBlendFactor = vkutil::toVkBlendFactor(desc.dstAlphaBlend);
        cbAttach.alphaBlendOp        = vkutil::toVkBlendOp(desc.alphaBlendOp);
    }

    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.logicOpEnable   = VK_FALSE;
    cb.attachmentCount = 1;
    cb.pAttachments    = &cbAttach;

    const VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = static_cast<uint32_t>(std::size(dynStates));
    dyn.pDynamicStates    = dynStates;

    // Dynamic rendering keeps us render-pass agnostic at runtime.
    // Use explicit format overrides when provided, else fall back to sensible defaults
    VkFormat colorFormat = desc.colorAttachmentFormat != Format::Undefined
                         ? vkutil::toVkFormat(desc.colorAttachmentFormat)
                         : VK_FORMAT_B8G8R8A8_SRGB;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    if (desc.depthTest || desc.depthWrite) {
        depthFormat = desc.depthAttachmentFormat != Format::Undefined
                    ? vkutil::toVkFormat(desc.depthAttachmentFormat)
                    : VK_FORMAT_D32_SFLOAT;
    }
    VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rendering.colorAttachmentCount    = 1;
    rendering.pColorAttachmentFormats = &colorFormat;
    rendering.depthAttachmentFormat   = depthFormat;
    rendering.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkGraphicsPipelineCreateInfo pci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pci.pNext               = &rendering;
    pci.stageCount          = 2;
    pci.pStages             = stages;
    pci.pVertexInputState   = &vi;
    pci.pInputAssemblyState = &ia;
    pci.pViewportState      = &vp;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState   = &ms;
    pci.pDepthStencilState  = &ds;
    pci.pColorBlendState    = &cb;
    pci.pDynamicState       = &dyn;
    pci.layout              = layout;
    pci.renderPass          = VK_NULL_HANDLE;
    pci.subpass             = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    const VkResult vr = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline);
    if (vr != VK_SUCCESS) {
        std::fprintf(stderr, "createGraphicsPipeline: vkCreateGraphicsPipelines failed (VkResult=%d)\n",
                     static_cast<int>(vr));
        vkDestroyPipelineLayout(m_device, layout, nullptr);
        return {};
    }

    VulkanResourcePool::PipelineEntry e{};
    e.pipeline = pipeline;
    e.layout   = layout;

    PipelineHandle h{};
    h.id = m_pool->nextPipeId;
    if (m_pool->nextPipeId >= m_pool->pipelines.size())
        m_pool->pipelines.resize(m_pool->nextPipeId + 1);
    m_pool->pipelines[m_pool->nextPipeId++] = e;
    
    // Cache the created pipeline by its descriptor hash
    m_pool->psoCacheGraphics[hashGraphicsPipelineDesc(desc)] = h.id;
    
    return h;
}
PipelineHandle VulkanDevice::createMeshShaderPipeline(const MeshShaderPipelineDesc& desc)
{
    if (!m_caps.meshShaders) {
        std::fprintf(stderr, "createMeshShaderPipeline: mesh shaders are not supported on this device\n");
        return {};
    }
    if (!desc.meshShader.valid() || !desc.fragmentShader.valid()) {
        std::fprintf(stderr, "createMeshShaderPipeline: mesh and fragment shaders are required\n");
        return {};
    }
    if (desc.meshShader.id >= m_pool->shaders.size() ||
        desc.fragmentShader.id >= m_pool->shaders.size() ||
        (desc.taskShader.valid() && desc.taskShader.id >= m_pool->shaders.size())) {
        std::fprintf(stderr, "createMeshShaderPipeline: shader handle out of range\n");
        return {};
    }

    // Check PSO cache first
    {
        const uint64_t descHash = hashMeshShaderPipelineDesc(desc);
        auto it = m_pool->psoCacheMeshShader.find(descHash);
        if (it != m_pool->psoCacheMeshShader.end()) {
            PipelineHandle h{};
            h.id = it->second;
            return h;
        }
    }

    const VkShaderModule task = desc.taskShader.valid() ? m_pool->shaders[desc.taskShader.id] : VK_NULL_HANDLE;
    const VkShaderModule mesh = m_pool->shaders[desc.meshShader.id];
    const VkShaderModule frag = m_pool->shaders[desc.fragmentShader.id];
    if ((desc.taskShader.valid() && task == VK_NULL_HANDLE) || mesh == VK_NULL_HANDLE || frag == VK_NULL_HANDLE) {
        std::fprintf(stderr, "createMeshShaderPipeline: shader module is null\n");
        return {};
    }

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_ALL;
    pcRange.offset     = 0;
    pcRange.size       = 128;

    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.setLayoutCount         = 0;
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges    = &pcRange;

    VkPipelineLayout layout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(m_device, &lci, nullptr, &layout) != VK_SUCCESS) {
        std::fprintf(stderr, "createMeshShaderPipeline: vkCreatePipelineLayout failed\n");
        return {};
    }

    std::array<VkPipelineShaderStageCreateInfo, 3> stages{};
    uint32_t stageCount = 0;
    if (desc.taskShader.valid()) {
        stages[stageCount].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[stageCount].stage  = VK_SHADER_STAGE_TASK_BIT_EXT;
        stages[stageCount].module = task;
        stages[stageCount].pName  = "main";
        ++stageCount;
    }
    stages[stageCount].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[stageCount].stage  = VK_SHADER_STAGE_MESH_BIT_EXT;
    stages[stageCount].module = mesh;
    stages[stageCount].pName  = "main";
    ++stageCount;

    stages[stageCount].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[stageCount].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[stageCount].module = frag;
    stages[stageCount].pName  = "main";
    ++stageCount;

    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.depthClampEnable        = VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.polygonMode             = VK_POLYGON_MODE_FILL;
    rs.cullMode                = VK_CULL_MODE_BACK_BIT;
    rs.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.depthBiasEnable         = VK_FALSE;
    rs.lineWidth               = 1.f;

    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable       = desc.depthTest ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable      = desc.depthWrite ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp        = VK_COMPARE_OP_GREATER_OR_EQUAL;
    ds.depthBoundsTestEnable = VK_FALSE;
    ds.stencilTestEnable     = VK_FALSE;

    VkPipelineColorBlendAttachmentState cbAttach{};
    cbAttach.blendEnable = VK_FALSE;
    cbAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.logicOpEnable   = VK_FALSE;
    cb.attachmentCount = 1;
    cb.pAttachments    = &cbAttach;

    const VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = static_cast<uint32_t>(std::size(dynStates));
    dyn.pDynamicStates    = dynStates;

    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkFormat depthFormat = (desc.depthTest || desc.depthWrite) ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_UNDEFINED;
    VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rendering.colorAttachmentCount    = 1;
    rendering.pColorAttachmentFormats = &colorFormat;
    rendering.depthAttachmentFormat   = depthFormat;
    rendering.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkGraphicsPipelineCreateInfo pci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pci.pNext               = &rendering;
    pci.stageCount          = stageCount;
    pci.pStages             = stages.data();
    pci.pViewportState      = &vp;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState   = &ms;
    pci.pDepthStencilState  = &ds;
    pci.pColorBlendState    = &cb;
    pci.pDynamicState       = &dyn;
    pci.layout              = layout;
    pci.renderPass          = VK_NULL_HANDLE;
    pci.subpass             = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    const VkResult vr = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline);
    if (vr != VK_SUCCESS) {
        std::fprintf(stderr, "createMeshShaderPipeline: vkCreateGraphicsPipelines failed (VkResult=%d)\n",
                     static_cast<int>(vr));
        vkDestroyPipelineLayout(m_device, layout, nullptr);
        return {};
    }

    VulkanResourcePool::PipelineEntry e{};
    e.pipeline = pipeline;
    e.layout   = layout;
    e.bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    PipelineHandle h{};
    h.id = m_pool->nextPipeId;
    if (m_pool->nextPipeId >= m_pool->pipelines.size())
        m_pool->pipelines.resize(m_pool->nextPipeId + 1);
    m_pool->pipelines[m_pool->nextPipeId++] = e;
    
    // Cache the created pipeline by its descriptor hash
    m_pool->psoCacheMeshShader[hashMeshShaderPipelineDesc(desc)] = h.id;
    
    return h;
}
PipelineHandle VulkanDevice::createComputePipeline(const ComputePipelineDesc& desc)
{
    if (!desc.computeShader.valid() || desc.computeShader.id >= m_pool->shaders.size()) {
        std::fprintf(stderr, "createComputePipeline: invalid compute shader handle\n");
        return {};
    }
    
    // Check PSO cache first
    {
        const uint64_t descHash = hashComputePipelineDesc(desc);
        auto it = m_pool->psoCacheCompute.find(descHash);
        if (it != m_pool->psoCacheCompute.end()) {
            PipelineHandle h{};
            h.id = it->second;
            return h;
        }
    }
    
    VkShaderModule cs = m_pool->shaders[desc.computeShader.id];
    if (!cs) { std::fprintf(stderr, "createComputePipeline: shader module is null\n"); return {}; }

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcRange.offset     = 0;
    pcRange.size       = 128;

    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges    = &pcRange;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(m_device, &lci, nullptr, &layout) != VK_SUCCESS) return {};

    VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = cs;
    stage.pName  = "main";

    VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    ci.stage  = stage;
    ci.layout = layout;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline) != VK_SUCCESS) {
        std::fprintf(stderr, "createComputePipeline: vkCreateComputePipelines failed\n");
        vkDestroyPipelineLayout(m_device, layout, nullptr);
        return {};
    }

    VulkanResourcePool::PipelineEntry e{};
    e.pipeline  = pipeline;
    e.layout    = layout;
    e.bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    PipelineHandle h{}; h.id = m_pool->nextPipeId;
    if (m_pool->nextPipeId >= m_pool->pipelines.size())
        m_pool->pipelines.resize(m_pool->nextPipeId + 1);
    m_pool->pipelines[m_pool->nextPipeId++] = e;
    
    // Cache the created pipeline by its descriptor hash
    m_pool->psoCacheCompute[hashComputePipelineDesc(desc)] = h.id;
    
    return h;
}
PipelineHandle VulkanDevice::createRayTracingPipeline(const RayTracingPipelineDesc& desc)
{
    if (!m_caps.rayTracingPipeline) {
        std::fprintf(stderr, "createRayTracingPipeline: ray tracing pipeline is not supported on this device\n");
        return {};
    }
    if (!desc.rayGenShader.valid() || desc.missShaders.empty() || desc.closestHitShaders.empty()) {
        std::fprintf(stderr, "createRayTracingPipeline: raygen, miss, and closest-hit shaders are required\n");
        return {};
    }

    // Check PSO cache first
    {
        const uint64_t descHash = hashRayTracingPipelineDesc(desc);
        auto it = m_pool->psoCacheRayTracing.find(descHash);
        if (it != m_pool->psoCacheRayTracing.end()) {
            PipelineHandle h{};
            h.id = it->second;
            return h;
        }
    }

    auto shaderModuleFromHandle = [&](ShaderHandle h) -> VkShaderModule {
        if (!h.valid() || h.id >= m_pool->shaders.size()) return VK_NULL_HANDLE;
        return m_pool->shaders[h.id];
    };

    VkShaderModule raygen = shaderModuleFromHandle(desc.rayGenShader);
    if (raygen == VK_NULL_HANDLE) {
        std::fprintf(stderr, "createRayTracingPipeline: invalid raygen shader module\n");
        return {};
    }

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_ALL;
    pcRange.offset     = 0;
    pcRange.size       = 128;

    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges    = &pcRange;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(m_device, &lci, nullptr, &layout) != VK_SUCCESS) {
        std::fprintf(stderr, "createRayTracingPipeline: vkCreatePipelineLayout failed\n");
        return {};
    }

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    stages.reserve(1 + desc.missShaders.size() + desc.closestHitShaders.size() + desc.anyHitShaders.size());
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    groups.reserve(1 + desc.missShaders.size() + desc.closestHitShaders.size());

    auto pushStage = [&](VkShaderStageFlagBits stageFlag, VkShaderModule module) -> uint32_t {
        VkPipelineShaderStageCreateInfo s{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        s.stage  = stageFlag;
        s.module = module;
        s.pName  = "main";
        stages.push_back(s);
        return static_cast<uint32_t>(stages.size() - 1);
    };

    {
        const uint32_t rgIndex = pushStage(VK_SHADER_STAGE_RAYGEN_BIT_KHR, raygen);
        VkRayTracingShaderGroupCreateInfoKHR g{VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
        g.type              = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        g.generalShader     = rgIndex;
        g.closestHitShader  = VK_SHADER_UNUSED_KHR;
        g.anyHitShader      = VK_SHADER_UNUSED_KHR;
        g.intersectionShader= VK_SHADER_UNUSED_KHR;
        groups.push_back(g);
    }

    for (const ShaderHandle h : desc.missShaders) {
        VkShaderModule miss = shaderModuleFromHandle(h);
        if (miss == VK_NULL_HANDLE) {
            std::fprintf(stderr, "createRayTracingPipeline: invalid miss shader module\n");
            vkDestroyPipelineLayout(m_device, layout, nullptr);
            return {};
        }
        const uint32_t missIndex = pushStage(VK_SHADER_STAGE_MISS_BIT_KHR, miss);
        VkRayTracingShaderGroupCreateInfoKHR g{VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
        g.type              = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        g.generalShader     = missIndex;
        g.closestHitShader  = VK_SHADER_UNUSED_KHR;
        g.anyHitShader      = VK_SHADER_UNUSED_KHR;
        g.intersectionShader= VK_SHADER_UNUSED_KHR;
        groups.push_back(g);
    }

    for (size_t i = 0; i < desc.closestHitShaders.size(); ++i) {
        const ShaderHandle chHandle = desc.closestHitShaders[i];
        VkShaderModule closestHit = shaderModuleFromHandle(chHandle);
        if (closestHit == VK_NULL_HANDLE) {
            std::fprintf(stderr, "createRayTracingPipeline: invalid closest-hit shader module\n");
            vkDestroyPipelineLayout(m_device, layout, nullptr);
            return {};
        }

        const uint32_t chIndex = pushStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, closestHit);
        uint32_t ahIndex = VK_SHADER_UNUSED_KHR;

        if (i < desc.anyHitShaders.size() && desc.anyHitShaders[i].valid()) {
            VkShaderModule anyHit = shaderModuleFromHandle(desc.anyHitShaders[i]);
            if (anyHit == VK_NULL_HANDLE) {
                std::fprintf(stderr, "createRayTracingPipeline: invalid any-hit shader module\n");
                vkDestroyPipelineLayout(m_device, layout, nullptr);
                return {};
            }
            ahIndex = pushStage(VK_SHADER_STAGE_ANY_HIT_BIT_KHR, anyHit);
        }

        VkRayTracingShaderGroupCreateInfoKHR g{VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
        g.type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        g.generalShader      = VK_SHADER_UNUSED_KHR;
        g.closestHitShader   = chIndex;
        g.anyHitShader       = ahIndex;
        g.intersectionShader = VK_SHADER_UNUSED_KHR;
        groups.push_back(g);
    }

    auto pfnCreateRTPipeline = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
        vkGetDeviceProcAddr(m_device, "vkCreateRayTracingPipelinesKHR"));
    if (!pfnCreateRTPipeline) {
        std::fprintf(stderr, "createRayTracingPipeline: vkCreateRayTracingPipelinesKHR unavailable\n");
        vkDestroyPipelineLayout(m_device, layout, nullptr);
        return {};
    }

    VkRayTracingPipelineCreateInfoKHR ci{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
    ci.stageCount                   = static_cast<uint32_t>(stages.size());
    ci.pStages                      = stages.data();
    ci.groupCount                   = static_cast<uint32_t>(groups.size());
    ci.pGroups                      = groups.data();
    ci.maxPipelineRayRecursionDepth = desc.maxRecursionDepth;
    ci.layout                       = layout;

    VkPipeline pipeline = VK_NULL_HANDLE;
    const VkResult vr = pfnCreateRTPipeline(m_device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline);
    if (vr != VK_SUCCESS) {
        std::fprintf(stderr, "createRayTracingPipeline: vkCreateRayTracingPipelinesKHR failed (VkResult=%d)\n",
                     static_cast<int>(vr));
        vkDestroyPipelineLayout(m_device, layout, nullptr);
        return {};
    }

    VulkanResourcePool::PipelineEntry e{};
    e.pipeline = pipeline;
    e.layout   = layout;
    e.bindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;

    PipelineHandle h{};
    h.id = m_pool->nextPipeId;
    if (m_pool->nextPipeId >= m_pool->pipelines.size())
        m_pool->pipelines.resize(m_pool->nextPipeId + 1);
    m_pool->pipelines[m_pool->nextPipeId++] = e;
    
    // Cache the created pipeline by its descriptor hash
    m_pool->psoCacheRayTracing[hashRayTracingPipelineDesc(desc)] = h.id;
    
    return h;
}
void VulkanDevice::destroyPipeline(PipelineHandle h) {
    if (!h.valid() || h.id >= m_pool->pipelines.size()) return;
    if (m_pool->pipelines[h.id].pipeline)
        vkDestroyPipeline(m_device, m_pool->pipelines[h.id].pipeline, nullptr);
    if (m_pool->pipelines[h.id].layout)
        vkDestroyPipelineLayout(m_device, m_pool->pipelines[h.id].layout, nullptr);
    m_pool->pipelines[h.id] = {};
}

// ── Command buffers ───────────────────────────────────────────────────────────
CmdBufHandle VulkanDevice::allocateCommandBuffer(QueueType qtype)
{
    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.queueFamilyIndex = m_queues[static_cast<size_t>(qtype)].family;
    pci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool pool = VK_NULL_HANDLE;
    vkCreateCommandPool(m_device, &pci, nullptr, &pool);

    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool        = pool;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(m_device, &ai, &cb);

    CmdBufHandle h{}; h.id = m_pool->nextCmdId;
    if (m_pool->nextCmdId >= m_pool->cmdBufs.size()) {
        m_pool->cmdBufs.resize(m_pool->nextCmdId + 1, VK_NULL_HANDLE);
        m_pool->cmdPools.resize(m_pool->nextCmdId + 1, VK_NULL_HANDLE);
        m_pool->cmdBufWrappers.resize(m_pool->nextCmdId + 1);
    }
    m_pool->cmdBufs [m_pool->nextCmdId] = cb;
    m_pool->cmdPools[m_pool->nextCmdId] = pool;
    ++m_pool->nextCmdId;
    return h;
}

void VulkanDevice::freeCommandBuffer(CmdBufHandle h) {
    if (!h.valid() || h.id >= m_pool->cmdBufs.size()) return;
    m_pool->cmdBufWrappers[h.id].reset();
    vkDestroyCommandPool(m_device, m_pool->cmdPools[h.id], nullptr);
    m_pool->cmdBufs [h.id] = VK_NULL_HANDLE;
    m_pool->cmdPools[h.id] = VK_NULL_HANDLE;
}

ICommandBuffer* VulkanDevice::getCommandBuffer(CmdBufHandle h)
{
    if (!h.valid() || h.id >= m_pool->cmdBufs.size()) return nullptr;
    auto& wrapper = m_pool->cmdBufWrappers[h.id];
    if (!wrapper)
        wrapper = std::make_unique<VulkanCommandBuffer>(m_pool->cmdBufs[h.id],
                                                        m_device, m_pool.get());
    return wrapper.get();
}

// ── Fence / Semaphore ─────────────────────────────────────────────────────────
FenceHandle VulkanDevice::createFence(bool signaled) {
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fi.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
    VkFence f = VK_NULL_HANDLE; vkCreateFence(m_device, &fi, nullptr, &f);
    FenceHandle h{}; h.id = m_pool->nextFenceId;
    if (m_pool->nextFenceId >= m_pool->fences.size()) m_pool->fences.resize(m_pool->nextFenceId+1);
    m_pool->fences[m_pool->nextFenceId++] = f;
    return h;
}
void VulkanDevice::destroyFence(FenceHandle h) {
    if (h.valid() && h.id < m_pool->fences.size()) vkDestroyFence(m_device, m_pool->fences[h.id], nullptr);
}
void VulkanDevice::waitForFence(FenceHandle h, uint64_t ns) {
    if (h.valid() && h.id < m_pool->fences.size()) vkWaitForFences(m_device, 1, &m_pool->fences[h.id], VK_TRUE, ns);
}
void VulkanDevice::resetFence(FenceHandle h) {
    if (h.valid() && h.id < m_pool->fences.size()) vkResetFences(m_device, 1, &m_pool->fences[h.id]);
}

SemaphoreHandle VulkanDevice::createSemaphore() {
    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore s = VK_NULL_HANDLE; vkCreateSemaphore(m_device, &si, nullptr, &s);
    SemaphoreHandle h{}; h.id = m_pool->nextSemId;
    if (m_pool->nextSemId >= m_pool->semaphores.size()) {
        m_pool->semaphores.resize(m_pool->nextSemId + 1, VK_NULL_HANDLE);
        m_pool->semaphoreKinds.resize(m_pool->nextSemId + 1, 0);
        m_pool->semaphoreValues.resize(m_pool->nextSemId + 1, 0);
    }
    m_pool->semaphores[m_pool->nextSemId++] = s;
    return h;
}

SemaphoreHandle VulkanDevice::createTimelineSemaphore(uint64_t initialValue)
{
    VkSemaphoreTypeCreateInfo typeInfo{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    typeInfo.initialValue  = initialValue;

    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    si.pNext = &typeInfo;

    VkSemaphore s = VK_NULL_HANDLE;
    if (vkCreateSemaphore(m_device, &si, nullptr, &s) != VK_SUCCESS)
        return {};

    SemaphoreHandle h{};
    h.id = m_pool->nextSemId;
    if (m_pool->nextSemId >= m_pool->semaphores.size()) {
        m_pool->semaphores.resize(m_pool->nextSemId + 1, VK_NULL_HANDLE);
        m_pool->semaphoreKinds.resize(m_pool->nextSemId + 1, 0);
        m_pool->semaphoreValues.resize(m_pool->nextSemId + 1, 0);
    }
    m_pool->semaphores[m_pool->nextSemId] = s;
    m_pool->semaphoreKinds[m_pool->nextSemId] = 1;
    m_pool->semaphoreValues[m_pool->nextSemId] = initialValue;
    ++m_pool->nextSemId;
    return h;
}

uint64_t VulkanDevice::querySemaphoreValue(SemaphoreHandle semaphore)
{
    if (!semaphore.valid() || semaphore.id >= m_pool->semaphores.size()) return 0;
    if (semaphore.id >= m_pool->semaphoreKinds.size() || m_pool->semaphoreKinds[semaphore.id] != 1) return 0;
    if (m_pool->semaphores[semaphore.id] == VK_NULL_HANDLE) return 0;

    uint64_t value = 0;
    const VkResult vr = vkGetSemaphoreCounterValue(m_device, m_pool->semaphores[semaphore.id], &value);
    if (vr != VK_SUCCESS) return 0;

    if (semaphore.id < m_pool->semaphoreValues.size())
        m_pool->semaphoreValues[semaphore.id] = value;
    return value;
}

void VulkanDevice::signalSemaphore(SemaphoreHandle semaphore, uint64_t value)
{
    if (!semaphore.valid() || semaphore.id >= m_pool->semaphores.size()) return;
    if (semaphore.id >= m_pool->semaphoreKinds.size() || m_pool->semaphoreKinds[semaphore.id] != 1) return;
    if (m_pool->semaphores[semaphore.id] == VK_NULL_HANDLE) return;

    VkSemaphoreSignalInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO};
    info.semaphore = m_pool->semaphores[semaphore.id];
    info.value = value;
    if (vkSignalSemaphore(m_device, &info) == VK_SUCCESS && semaphore.id < m_pool->semaphoreValues.size())
        m_pool->semaphoreValues[semaphore.id] = value;
}

void VulkanDevice::waitSemaphore(SemaphoreHandle semaphore, uint64_t value, uint64_t timeoutNs)
{
    if (!semaphore.valid() || semaphore.id >= m_pool->semaphores.size()) return;
    if (semaphore.id >= m_pool->semaphoreKinds.size() || m_pool->semaphoreKinds[semaphore.id] != 1) return;
    if (m_pool->semaphores[semaphore.id] == VK_NULL_HANDLE) return;

    VkSemaphoreWaitInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
    info.flags = 0;
    info.semaphoreCount = 1;
    VkSemaphore sem = m_pool->semaphores[semaphore.id];
    info.pSemaphores = &sem;
    info.pValues = &value;
    if (vkWaitSemaphores(m_device, &info, timeoutNs) == VK_SUCCESS && semaphore.id < m_pool->semaphoreValues.size())
        m_pool->semaphoreValues[semaphore.id] = value;
}

void VulkanDevice::destroySemaphore(SemaphoreHandle h) {
    if (!h.valid() || h.id >= m_pool->semaphores.size()) return;
    if (m_pool->semaphores[h.id]) vkDestroySemaphore(m_device, m_pool->semaphores[h.id], nullptr);
    m_pool->semaphores[h.id] = VK_NULL_HANDLE;
    if (h.id < m_pool->semaphoreKinds.size()) m_pool->semaphoreKinds[h.id] = 0;
    if (h.id < m_pool->semaphoreValues.size()) m_pool->semaphoreValues[h.id] = 0;
}

// ── Sampler ──────────────────────────────────────────────────────────────────────────────────
SamplerHandle VulkanDevice::createSampler(const SamplerDesc& desc)
{
    VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter               = vkutil::toVkFilter(desc.magFilter);
    sci.minFilter               = vkutil::toVkFilter(desc.minFilter);
    sci.mipmapMode              = vkutil::toVkMipmapMode(desc.mipmapMode);
    sci.addressModeU            = vkutil::toVkAddressMode(desc.addressU);
    sci.addressModeV            = vkutil::toVkAddressMode(desc.addressV);
    sci.addressModeW            = vkutil::toVkAddressMode(desc.addressW);
    sci.mipLodBias              = desc.mipLodBias;
    sci.anisotropyEnable        = desc.anisotropyEnable ? VK_TRUE : VK_FALSE;
    sci.maxAnisotropy           = desc.maxAnisotropy;
    sci.compareEnable           = desc.compareEnable ? VK_TRUE : VK_FALSE;
    sci.compareOp               = vkutil::toVkCompareOp(desc.compareOp);
    sci.minLod                  = desc.minLod;
    sci.maxLod                  = desc.maxLod;
    sci.borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    VkSampler sampler = VK_NULL_HANDLE;
    if (vkCreateSampler(m_device, &sci, nullptr, &sampler) != VK_SUCCESS) return {};
    SamplerHandle h{}; h.id = m_pool->nextSamplerId;
    if (m_pool->nextSamplerId >= m_pool->samplers.size())
        m_pool->samplers.resize(m_pool->nextSamplerId + 1, VK_NULL_HANDLE);
    m_pool->samplers[m_pool->nextSamplerId++] = sampler;
    return h;
}
void VulkanDevice::destroySampler(SamplerHandle h) {
    if (h.valid() && h.id < m_pool->samplers.size() && m_pool->samplers[h.id]) {
        vkDestroySampler(m_device, m_pool->samplers[h.id], nullptr);
        m_pool->samplers[h.id] = VK_NULL_HANDLE;
    }
}

// ── Query pool (timestamps) ────────────────────────────────────────────────────────────────
QueryPoolHandle VulkanDevice::createQueryPool(const QueryPoolDesc& desc)
{
    VkQueryPoolCreateInfo qpci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    qpci.queryType  = VK_QUERY_TYPE_TIMESTAMP;
    qpci.queryCount = desc.count;
    VkQueryPool qp = VK_NULL_HANDLE;
    if (vkCreateQueryPool(m_device, &qpci, nullptr, &qp) != VK_SUCCESS) return {};
    QueryPoolHandle h{}; h.id = m_pool->nextQPoolId;
    if (m_pool->nextQPoolId >= m_pool->queryPools.size())
        m_pool->queryPools.resize(m_pool->nextQPoolId + 1, VK_NULL_HANDLE);
    m_pool->queryPools[m_pool->nextQPoolId++] = qp;
    return h;
}
void VulkanDevice::destroyQueryPool(QueryPoolHandle h) {
    if (h.valid() && h.id < m_pool->queryPools.size() && m_pool->queryPools[h.id]) {
        vkDestroyQueryPool(m_device, m_pool->queryPools[h.id], nullptr);
        m_pool->queryPools[h.id] = VK_NULL_HANDLE;
    }
}
void VulkanDevice::readbackTimestamps(QueryPoolHandle h, uint32_t first, uint32_t count,
                                        uint64_t* outNanos)
{
    if (!h.valid() || h.id >= m_pool->queryPools.size() || !m_pool->queryPools[h.id]) return;

    // Query GPU timestamp period (nanoseconds per timestamp tick)
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(m_physDevice, &props);
    const float nsPerTick = props.limits.timestampPeriod;

    std::vector<uint64_t> ticks(count, 0);
    vkGetQueryPoolResults(m_device, m_pool->queryPools[h.id], first, count,
                          count * sizeof(uint64_t), ticks.data(), sizeof(uint64_t),
                          VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    for (uint32_t i = 0; i < count; ++i)
        outNanos[i] = static_cast<uint64_t>(ticks[i] * nsPerTick);
}

// ── Data upload ───────────────────────────────────────────────────────────────
void VulkanDevice::uploadBuffer(BufferHandle dst, const void* data,
                                 uint64_t sizeBytes, uint64_t dstOffset)
{
    if (!dst.valid() || dst.id >= m_pool->buffers.size()) return;
    auto& buf = m_pool->buffers[dst.id];

    // If the buffer is host-visible (mappedPtr set), write directly
    if (buf.mappedPtr) {
        std::memcpy(static_cast<uint8_t*>(buf.mappedPtr) + dstOffset, data, sizeBytes);
        vmaFlushAllocation(m_pool->vma, buf.allocation, dstOffset, sizeBytes);
        return;
    }

    // Otherwise use a staging buffer
    StagingUpload stage = vkPrepareBufferUpload(m_pool->vma, m_device,
                                                 data, sizeBytes);
    VkCommandBuffer cmd = beginSingleUse(m_device, m_pool->transferPool);
    VkBufferCopy region{ 0, dstOffset, sizeBytes };
    vkCmdCopyBuffer(cmd, stage.staging.handle, buf.handle, 1, &region);
    endAndSubmitSingleUse(m_device, m_pool->transferPool,
                          m_queues[static_cast<size_t>(QueueType::Transfer)].handle, cmd);
    nexus::gfx::vkDestroyBuffer(m_pool->vma, stage.staging);
}

void VulkanDevice::uploadTexture(TextureHandle dst, const void* data, uint64_t sizeBytes)
{
    if (!dst.valid() || dst.id >= m_pool->textures.size()) return;
    auto& tex = m_pool->textures[dst.id];
    if (!tex.image || !data || !sizeBytes) return;

    // Create transient staging buffer
    StagingUpload stage = vkPrepareBufferUpload(m_pool->vma, m_device, data, sizeBytes);

    VkCommandBuffer cmd = beginSingleUse(m_device, m_pool->transferPool);

    // Transition UNDEFINED → TRANSFER_DST_OPTIMAL
    VkImageMemoryBarrier2 toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    toTransfer.srcStageMask          = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    toTransfer.srcAccessMask         = 0;
    toTransfer.dstStageMask          = VK_PIPELINE_STAGE_2_COPY_BIT;
    toTransfer.dstAccessMask         = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    toTransfer.oldLayout             = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout             = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image                 = tex.image;
    toTransfer.subresourceRange      = { VK_IMAGE_ASPECT_COLOR_BIT, 0,
                                         VK_REMAINING_MIP_LEVELS, 0,
                                         VK_REMAINING_ARRAY_LAYERS };
    VkDependencyInfo di1{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    di1.imageMemoryBarrierCount = 1; di1.pImageMemoryBarriers = &toTransfer;
    vkCmdPipelineBarrier2(cmd, &di1);

    // Copy staging buffer → image (mip 0, layer 0)
    VkBufferImageCopy region{};
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;   // tightly packed
    region.bufferImageHeight = 0;
    region.imageSubresource  = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageOffset       = { 0, 0, 0 };
    region.imageExtent       = tex.extent;
    vkCmdCopyBufferToImage(cmd, stage.staging.handle, tex.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition TRANSFER_DST_OPTIMAL → SHADER_READ_ONLY_OPTIMAL
    VkImageMemoryBarrier2 toShader{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    toShader.srcStageMask          = VK_PIPELINE_STAGE_2_COPY_BIT;
    toShader.srcAccessMask         = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    toShader.dstStageMask          = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
                                   | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    toShader.dstAccessMask         = VK_ACCESS_2_SHADER_READ_BIT;
    toShader.oldLayout             = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShader.newLayout             = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toShader.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    toShader.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    toShader.image                 = tex.image;
    toShader.subresourceRange      = { VK_IMAGE_ASPECT_COLOR_BIT, 0,
                                       VK_REMAINING_MIP_LEVELS, 0,
                                       VK_REMAINING_ARRAY_LAYERS };
    VkDependencyInfo di2{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    di2.imageMemoryBarrierCount = 1; di2.pImageMemoryBarriers = &toShader;
    vkCmdPipelineBarrier2(cmd, &di2);

    endAndSubmitSingleUse(m_device, m_pool->transferPool,
                          m_queues[static_cast<size_t>(QueueType::Transfer)].handle, cmd);
    nexus::gfx::vkDestroyBuffer(m_pool->vma, stage.staging);
}

// ── Submit ────────────────────────────────────────────────────────────────────
void VulkanDevice::submit(QueueType qtype,
                           std::span<const CmdBufHandle> cmds,
                           std::span<const SemaphoreHandle> waitSems,
                           std::span<const SemaphoreHandle> signalSems,
                           FenceHandle signalFence)
{
    submitWithTimeline(qtype, cmds, waitSems, {}, signalSems, {}, signalFence);
}

void VulkanDevice::submitWithTimeline(QueueType qtype,
                                      std::span<const CmdBufHandle> cmds,
                                      std::span<const SemaphoreHandle> waitSems,
                                      std::span<const uint64_t> waitValues,
                                      std::span<const SemaphoreHandle> signalSems,
                                      std::span<const uint64_t> signalValues,
                                      FenceHandle signalFence)
{
    std::vector<VkCommandBuffer> vkCmds;
    vkCmds.reserve(cmds.size());
    for (auto h : cmds)
        if (h.valid() && h.id < m_pool->cmdBufs.size())
            vkCmds.push_back(m_pool->cmdBufs[h.id]);

    std::vector<VkSemaphore> vkWait, vkSignal;
    std::vector<uint64_t> timelineWaitValues, timelineSignalValues;

    vkWait.reserve(waitSems.size());
    timelineWaitValues.reserve(waitSems.size());
    for (size_t i = 0; i < waitSems.size(); ++i) {
        const auto h = waitSems[i];
        if (!h.valid() || h.id >= m_pool->semaphores.size()) continue;
        const auto sem = m_pool->semaphores[h.id];
        if (sem == VK_NULL_HANDLE) continue;

        vkWait.push_back(sem);
        const bool isTimeline = (h.id < m_pool->semaphoreKinds.size() && m_pool->semaphoreKinds[h.id] == 1);
        const uint64_t value = (isTimeline && i < waitValues.size()) ? waitValues[i] : 0;
        timelineWaitValues.push_back(value);
    }

    vkSignal.reserve(signalSems.size());
    timelineSignalValues.reserve(signalSems.size());
    for (size_t i = 0; i < signalSems.size(); ++i) {
        const auto h = signalSems[i];
        if (!h.valid() || h.id >= m_pool->semaphores.size()) continue;
        const auto sem = m_pool->semaphores[h.id];
        if (sem == VK_NULL_HANDLE) continue;

        vkSignal.push_back(sem);
        const bool isTimeline = (h.id < m_pool->semaphoreKinds.size() && m_pool->semaphoreKinds[h.id] == 1);
        const uint64_t value = (isTimeline && i < signalValues.size()) ? signalValues[i] : 0;
        timelineSignalValues.push_back(value);
    }

    std::vector<VkPipelineStageFlags> waitStages(vkWait.size(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    VkTimelineSemaphoreSubmitInfo tsi{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    tsi.waitSemaphoreValueCount   = static_cast<uint32_t>(timelineWaitValues.size());
    tsi.pWaitSemaphoreValues      = timelineWaitValues.empty() ? nullptr : timelineWaitValues.data();
    tsi.signalSemaphoreValueCount = static_cast<uint32_t>(timelineSignalValues.size());
    tsi.pSignalSemaphoreValues    = timelineSignalValues.empty() ? nullptr : timelineSignalValues.data();

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.pNext                = &tsi;
    si.waitSemaphoreCount   = static_cast<uint32_t>(vkWait.size());
    si.pWaitSemaphores      = vkWait.empty() ? nullptr : vkWait.data();
    si.pWaitDstStageMask    = waitStages.empty() ? nullptr : waitStages.data();
    si.commandBufferCount   = static_cast<uint32_t>(vkCmds.size());
    si.pCommandBuffers      = vkCmds.empty() ? nullptr : vkCmds.data();
    si.signalSemaphoreCount = static_cast<uint32_t>(vkSignal.size());
    si.pSignalSemaphores    = vkSignal.empty() ? nullptr : vkSignal.data();

    VkFence fence = VK_NULL_HANDLE;
    if (signalFence.valid() && signalFence.id < m_pool->fences.size())
        fence = m_pool->fences[signalFence.id];

    const VkResult vr = vkQueueSubmit(m_queues[static_cast<size_t>(qtype)].handle, 1, &si, fence);
    if (vr == VK_SUCCESS) {
        for (size_t i = 0; i < signalSems.size(); ++i) {
            const auto h = signalSems[i];
            if (!h.valid() || h.id >= m_pool->semaphoreKinds.size()) continue;
            if (m_pool->semaphoreKinds[h.id] != 1) continue;
            if (h.id >= m_pool->semaphoreValues.size()) continue;
            if (i >= signalValues.size()) continue;
            m_pool->semaphoreValues[h.id] = std::max(m_pool->semaphoreValues[h.id], signalValues[i]);
        }
    }
}

// ── Acceleration structures ───────────────────────────────────────────────────
AccelStructHandle VulkanDevice::buildBLAS(BufferHandle vtxBuf, BufferHandle idxBuf,
                                           uint32_t vtxCount, uint32_t idxCount)
{
    if (!m_pool->rt.ready() || !vtxBuf.valid() || !idxBuf.valid()) return {};

    VulkanAccelStruct as = nexus::gfx::buildBLAS(
        m_pool->vma, m_device,
        m_queues[static_cast<size_t>(QueueType::Compute)].handle,
        m_pool->transferPool,
        m_pool->rt,
        m_pool->buffers[vtxBuf.id].handle,
        sizeof(float) * 3,  // stride: vec3 position
        vtxCount,
        m_pool->buffers[idxBuf.id].handle,
        idxCount
    );

    AccelStructHandle h{}; h.id = m_pool->nextASId;
    if (m_pool->nextASId >= m_pool->accelStructs.size())
        m_pool->accelStructs.resize(m_pool->nextASId + 1);
    m_pool->accelStructs[m_pool->nextASId++] = as;
    return h;
}

AccelStructHandle VulkanDevice::buildTLAS(std::span<const AccelStructHandle> blases)
{
    if (!m_pool->rt.ready() || blases.empty()) return {};

    std::vector<TLASInstance> instances;
    instances.reserve(blases.size());
    for (auto h : blases) {
        if (!h.valid() || h.id >= m_pool->accelStructs.size()) continue;
        TLASInstance inst{};
        inst.blasDeviceAddress = m_pool->accelStructs[h.id].deviceAddress;
        // Identity transform
        inst.transform.matrix[0][0] = inst.transform.matrix[1][1] = inst.transform.matrix[2][2] = 1.f;
        instances.push_back(inst);
    }
    if (instances.empty()) return {};

    VulkanAccelStruct as = nexus::gfx::buildTLAS(
        m_pool->vma, m_device,
        m_queues[static_cast<size_t>(QueueType::Compute)].handle,
        m_pool->transferPool,
        m_pool->rt, instances
    );

    AccelStructHandle h{}; h.id = m_pool->nextASId;
    if (m_pool->nextASId >= m_pool->accelStructs.size())
        m_pool->accelStructs.resize(m_pool->nextASId + 1);
    m_pool->accelStructs[m_pool->nextASId++] = as;
    return h;
}

void VulkanDevice::destroyAccelStruct(AccelStructHandle h) {
    if (!h.valid() || h.id >= m_pool->accelStructs.size()) return;
    nexus::gfx::destroyAccelStruct(m_pool->vma, m_device, m_pool->rt, m_pool->accelStructs[h.id]);
}

// ── Factories ────────────────────────────────────────────────────────────────────────
std::unique_ptr<IGPUAllocator> VulkanDevice::createAllocator() {
    return std::make_unique<VulkanAllocator>(
        m_instance,
        m_physDevice,
        m_device,
        m_pool.get(),
        m_queues[static_cast<size_t>(QueueType::Graphics)].handle);
}
std::unique_ptr<ISwapchain> VulkanDevice::createSwapchain(const SwapchainDesc& desc) {
    auto sc = std::make_unique<VulkanSwapchain>(m_instance, m_physDevice, m_device, desc, m_queues[0].family);
    sc->setPresentQueue(m_queues[0].handle, m_queues[0].family);
    return sc;
}

// ── Internal single-use cmd helpers (also used in VulkanRayTracing) ───────────
VkCommandBuffer VulkanDevice::beginSingleUse(VkDevice device, VkCommandPool pool)
{
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = pool; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device, &ai, &cmd);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    return cmd;
}
void VulkanDevice::endAndSubmitSingleUse(VkDevice device, VkCommandPool pool,
                                          VkQueue queue, VkCommandBuffer cmd)
{
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO}; si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE; vkCreateFence(device, &fi, nullptr, &fence);
    vkQueueSubmit(queue, 1, &si, fence);
    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, pool, 1, &cmd);
}

// ── Descriptor set management ─────────────────────────────────────────────────

static VkDescriptorType toVkDescriptorType(DescriptorType t)
{
    switch (t) {
        case DescriptorType::UniformBuffer:        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorType::StorageBuffer:        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case DescriptorType::SampledTexture:       return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case DescriptorType::StorageTexture:       return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case DescriptorType::Sampler:              return VK_DESCRIPTOR_TYPE_SAMPLER;
        case DescriptorType::CombinedImageSampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }
    return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
}

static constexpr uint32_t kDescriptorSetsPerPool = 64;

static std::vector<DescriptorLayoutKeyItem>
buildLayoutKey(std::span<const DescriptorBindingDesc> bindings)
{
    std::vector<DescriptorLayoutKeyItem> key;
    key.reserve(bindings.size());
    for (const auto& b : bindings)
        key.push_back({ b.binding, b.type });

    std::sort(key.begin(), key.end(), [](const auto& a, const auto& b) {
        if (a.binding != b.binding) return a.binding < b.binding;
        return static_cast<uint32_t>(a.type) < static_cast<uint32_t>(b.type);
    });
    return key;
}

static std::vector<VkDescriptorPoolSize>
buildPoolSizes(std::span<const DescriptorBindingDesc> bindings, uint32_t multiplier)
{
    std::unordered_map<VkDescriptorType, uint32_t> typeCounts;
    for (const auto& b : bindings)
        typeCounts[toVkDescriptorType(b.type)]++;

    std::vector<VkDescriptorPoolSize> sizes;
    sizes.reserve(typeCounts.size());
    for (const auto& [type, count] : typeCounts)
        sizes.push_back({ type, count * multiplier });

    std::sort(sizes.begin(), sizes.end(), [](const auto& a, const auto& b) {
        return static_cast<uint32_t>(a.type) < static_cast<uint32_t>(b.type);
    });
    return sizes;
}

static uint64_t hashLayoutKey(const std::vector<DescriptorLayoutKeyItem>& key)
{
    return fnv1a_hash(key.data(), key.size() * sizeof(key[0]));
}

static uint64_t hashPoolSizes(const std::vector<VkDescriptorPoolSize>& sizes)
{
    return fnv1a_hash(sizes.data(), sizes.size() * sizeof(sizes[0]));
}

static bool sameLayoutKey(const std::vector<DescriptorLayoutKeyItem>& a,
                          const std::vector<DescriptorLayoutKeyItem>& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].binding != b[i].binding) return false;
        if (a[i].type != b[i].type) return false;
    }
    return true;
}

static bool samePoolSizes(const std::vector<VkDescriptorPoolSize>& a,
                          const std::vector<VkDescriptorPoolSize>& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].type != b[i].type) return false;
        if (a[i].descriptorCount != b[i].descriptorCount) return false;
    }
    return true;
}

DescriptorSetHandle VulkanDevice::allocateDescriptorSet(const DescriptorSetDesc& desc)
{
    // Build/reuse descriptor set layout for this exact binding signature.
    const auto layoutKey = buildLayoutKey(desc.bindings);
    const uint64_t layoutHash = hashLayoutKey(layoutKey);

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    auto& layoutEntries = m_pool->descriptorLayoutCache[layoutHash];
    for (const auto& entry : layoutEntries) {
        if (sameLayoutKey(entry.key, layoutKey)) {
            layout = entry.layout;
            break;
        }
    }

    if (!layout) {
        std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
        layoutBindings.reserve(desc.bindings.size());
        for (const auto& b : desc.bindings) {
            VkDescriptorSetLayoutBinding lb{};
            lb.binding         = b.binding;
            lb.descriptorType  = toVkDescriptorType(b.type);
            lb.descriptorCount = 1;
            lb.stageFlags      = VK_SHADER_STAGE_ALL;
            layoutBindings.push_back(lb);
        }

        VkDescriptorSetLayoutCreateInfo layoutCI{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutCI.bindingCount = static_cast<uint32_t>(layoutBindings.size());
        layoutCI.pBindings    = layoutBindings.data();
        if (vkCreateDescriptorSetLayout(m_device, &layoutCI, nullptr, &layout) != VK_SUCCESS)
            return {};

        layoutEntries.push_back({ layoutKey, layout });
    }

    // Reuse/create a descriptor pool bucket for this descriptor type mix.
    const auto poolSizes = buildPoolSizes(desc.bindings, kDescriptorSetsPerPool);
    const uint64_t poolHash = hashPoolSizes(poolSizes);
    auto& buckets = m_pool->descriptorPoolBuckets[poolHash];
    Impl::DescriptorPoolBucket* bucket = nullptr;
    for (auto& b : buckets) {
        if (samePoolSizes(b.sizes, poolSizes)) {
            bucket = &b;
            break;
        }
    }
    if (!bucket) {
        buckets.push_back({ poolSizes, {} });
        bucket = &buckets.back();
    }

    VkDescriptorSet set = VK_NULL_HANDLE;
    VkDescriptorPool selectedPool = VK_NULL_HANDLE;
    auto tryAllocate = [&](VkDescriptorPool pool) -> VkResult {
        VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.descriptorPool     = pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &layout;
        return vkAllocateDescriptorSets(m_device, &allocInfo, &set);
    };

    for (auto pool : bucket->pools) {
        set = VK_NULL_HANDLE;
        const VkResult r = tryAllocate(pool);
        if (r == VK_SUCCESS) {
            selectedPool = pool;
            break;
        }
        if (r != VK_ERROR_OUT_OF_POOL_MEMORY && r != VK_ERROR_FRAGMENTED_POOL)
            return {};
    }

    if (!selectedPool) {
        VkDescriptorPoolCreateInfo poolCI{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolCI.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolCI.maxSets       = kDescriptorSetsPerPool;
        poolCI.poolSizeCount = static_cast<uint32_t>(bucket->sizes.size());
        poolCI.pPoolSizes    = bucket->sizes.data();

        VkDescriptorPool newPool = VK_NULL_HANDLE;
        if (vkCreateDescriptorPool(m_device, &poolCI, nullptr, &newPool) != VK_SUCCESS)
            return {};

        set = VK_NULL_HANDLE;
        if (tryAllocate(newPool) != VK_SUCCESS) {
            vkDestroyDescriptorPool(m_device, newPool, nullptr);
            return {};
        }

        bucket->pools.push_back(newPool);
        selectedPool = newPool;
    }

    uint64_t id = m_pool->nextDescSetId++;
    if (id >= m_pool->descSets.size())
        m_pool->descSets.resize(id + 1);
    m_pool->descSets[id] = { selectedPool, layout, set };

    DescriptorSetHandle h{};
    h.id = id;
    return h;
}

void VulkanDevice::updateDescriptorSet(DescriptorSetHandle handle,
                                       std::span<const DescriptorBindingDesc> bindings)
{
    if (!handle.valid() || handle.id >= m_pool->descSets.size()) return;
    VkDescriptorSet vkSet = m_pool->descSets[handle.id].set;
    if (!vkSet) return;

    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorBufferInfo> bufInfos;
    std::vector<VkDescriptorImageInfo> imgInfos;
    bufInfos.reserve(bindings.size());
    imgInfos.reserve(bindings.size());

    for (const auto& b : bindings) {
        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet          = vkSet;
        w.dstBinding      = b.binding;
        w.dstArrayElement = 0;
        w.descriptorCount = 1;
        w.descriptorType  = toVkDescriptorType(b.type);

        switch (b.type) {
            case DescriptorType::UniformBuffer:
            case DescriptorType::StorageBuffer: {
                if (!b.buffer.valid() || b.buffer.id >= m_pool->buffers.size()) continue;
                bufInfos.push_back({ m_pool->buffers[b.buffer.id].handle, 0, VK_WHOLE_SIZE });
                w.pBufferInfo = &bufInfos.back();
                break;
            }
            case DescriptorType::SampledTexture:
            case DescriptorType::StorageTexture: {
                if (!b.texture.valid() || b.texture.id >= m_pool->textures.size()) continue;
                VkImageLayout imgLayout = (b.type == DescriptorType::SampledTexture)
                    ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    : VK_IMAGE_LAYOUT_GENERAL;
                imgInfos.push_back({ VK_NULL_HANDLE,
                                     m_pool->textures[b.texture.id].defaultView,
                                     imgLayout });
                w.pImageInfo = &imgInfos.back();
                break;
            }
            case DescriptorType::Sampler: {
                if (!b.sampler.valid() || b.sampler.id >= m_pool->samplers.size()) continue;
                imgInfos.push_back({ m_pool->samplers[b.sampler.id],
                                     VK_NULL_HANDLE,
                                     VK_IMAGE_LAYOUT_UNDEFINED });
                w.pImageInfo = &imgInfos.back();
                break;
            }
            case DescriptorType::CombinedImageSampler: {
                bool texOk = b.texture.valid() && b.texture.id < m_pool->textures.size();
                bool samOk = b.sampler.valid() && b.sampler.id < m_pool->samplers.size();
                if (!texOk || !samOk) continue;
                imgInfos.push_back({ m_pool->samplers[b.sampler.id],
                                     m_pool->textures[b.texture.id].defaultView,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
                w.pImageInfo = &imgInfos.back();
                break;
            }
        }
        writes.push_back(w);
    }

    if (!writes.empty()) {
        vkUpdateDescriptorSets(m_device,
                               static_cast<uint32_t>(writes.size()), writes.data(),
                               0, nullptr);
    }
}

void VulkanDevice::freeDescriptorSet(DescriptorSetHandle handle)
{
    if (!handle.valid() || handle.id >= m_pool->descSets.size()) return;
    auto& entry = m_pool->descSets[handle.id];
    if (entry.pool && entry.set)
        vkFreeDescriptorSets(m_device, entry.pool, 1, &entry.set);
    entry = {};
}

} // namespace nexus::gfx
