#include <nexus/gfx/Allocator.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace nexus::gfx {

struct AllocationRecord {
    uint64_t handle = 0;
    uint64_t size = 0;
    uint32_t typeFlags = 0;
    bool dedicated = false;
};

struct MemoryHeapStats {
    uint64_t totalSize = 0;
    uint64_t usedSize = 0;
    uint64_t allocationCount = 0;
    uint32_t memoryTypeIndex = 0;
    bool isDeviceLocal = false;
};

struct MemoryBudget {
    uint64_t usageBytes = 0;
    uint64_t budgetBytes = 0;
    float usagePercent = 0.0f;
};

namespace {
    std::unordered_map<uint64_t, AllocationRecord> g_allocations;
    std::vector<MemoryHeapStats> g_heapStats;
    MemoryBudget g_budget;
    uint64_t g_nextHandle = 1;

    void updateBudget() {
        if (g_budget.budgetBytes > 0) {
            g_budget.usagePercent = static_cast<float>(g_budget.usageBytes) /
                                    static_cast<float>(g_budget.budgetBytes) * 100.0f;
        }
    }

    void updateHeapStats() {
        g_heapStats.clear();
        std::unordered_map<uint32_t, MemoryHeapStats> heapMap;

        for (const auto& [handle, rec] : g_allocations) {
            uint32_t typeIdx = rec.typeFlags & 0x1F;
            auto& stats = heapMap[typeIdx];
            stats.memoryTypeIndex = typeIdx;
            stats.usedSize += rec.size;
            stats.allocationCount++;
            if (rec.dedicated) {
                stats.totalSize = std::max(stats.totalSize, static_cast<uint64_t>(stats.usedSize * 2));
            }
        }

        for (auto& [idx, stats] : heapMap) {
            if (stats.totalSize == 0) {
                stats.totalSize = std::max(static_cast<uint64_t>(stats.usedSize * 3), static_cast<uint64_t>(256ULL * 1024 * 1024));
            }
            g_heapStats.push_back(stats);
        }

        std::sort(g_heapStats.begin(), g_heapStats.end(),
                  [](const MemoryHeapStats& a, const MemoryHeapStats& b) {
                      return a.memoryTypeIndex < b.memoryTypeIndex;
                  });
    }
}

uint64_t allocateFromBudget(uint64_t size, uint32_t memoryTypeIndex, bool dedicated) {
    AllocationRecord rec;
    rec.handle = g_nextHandle++;
    rec.size = size;
    rec.typeFlags = memoryTypeIndex;
    rec.dedicated = dedicated;

    g_allocations[rec.handle] = rec;
    g_budget.usageBytes += size;
    updateBudget();
    updateHeapStats();

    return rec.handle;
}

bool deallocateFromBudget(uint64_t handle) {
    auto it = g_allocations.find(handle);
    if (it == g_allocations.end()) return false;

    g_budget.usageBytes -= it->second.size;
    g_allocations.erase(it);
    updateBudget();
    updateHeapStats();

    return true;
}

MemoryBudget queryBudget() {
    return g_budget;
}

void setBudget(uint64_t budgetBytes) {
    g_budget.budgetBytes = budgetBytes;
    updateBudget();
}

std::vector<MemoryHeapStats> queryHeapStats() {
    updateHeapStats();
    return g_heapStats;
}

size_t getAllocationCount() {
    return g_allocations.size();
}

uint64_t getTotalAllocatedBytes() {
    uint64_t total = 0;
    for (const auto& [handle, rec] : g_allocations) {
        total += rec.size;
    }
    return total;
}

float getFragmentationPercent() {
    if (g_heapStats.empty()) return 0.0f;

    uint64_t largestFreeBlock = 0;
    for (const auto& stats : g_heapStats) {
        uint64_t freeSpace = stats.totalSize - stats.usedSize;
        largestFreeBlock = std::max(largestFreeBlock, freeSpace);
    }

    uint64_t totalFree = 0;
    for (const auto& stats : g_heapStats) {
        totalFree += (stats.totalSize - stats.usedSize);
    }

    if (totalFree == 0) return 0.0f;
    return (1.0f - static_cast<float>(largestFreeBlock) / static_cast<float>(totalFree)) * 100.0f;
}

bool wouldExceedBudget(uint64_t requestedSize) {
    if (g_budget.budgetBytes == 0) return false;
    return (g_budget.usageBytes + requestedSize) > g_budget.budgetBytes;
}

} // namespace nexus::gfx
