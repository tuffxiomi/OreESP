#include "ore_esp.hpp"
#include "runtime.hpp"
#include "signatures.hpp"
#include <pl/memory/Hook.hpp>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
struct BlockPos { int x, y, z; };
struct Vec3 { float x, y, z; };
struct Highlight { BlockPos pos; std::uint32_t color; };

using TessBeginFn = void(*)(void*, void*, int, int, int);
using TessColorFn = void(*)(void*, float, float, float, float);
using TessVertexFn = void(*)(void*, float, float, float);
using RenderMeshFn = void(*)(void*, void*, void*, char*);
using GetBlockFn = const void*(*)(void*, const BlockPos&);

constexpr std::size_t kActorDimension = 448;
constexpr std::size_t kDimensionBlockSource = 208;
constexpr std::size_t kLevelRendererPlayer = 0x420;
constexpr std::size_t kCameraPos = 0x61C;
constexpr std::size_t kSelectionMaterial = 0x1030;
constexpr std::size_t kScreenTessellator = 0xB8;
constexpr std::size_t kScreenColorHolder = 0x30;
constexpr std::size_t kBlockType = 0x68;
constexpr std::size_t kBlockTypeNameInfo = 0x88;
constexpr std::size_t kNameInfoFullName = 0x40;
constexpr std::size_t kHashedStringString = 0x8;
constexpr int kLines = 4;

std::atomic<void*> g_localPlayer{nullptr};
std::mutex g_cacheMutex;
std::vector<Highlight> g_cache;
std::chrono::steady_clock::time_point g_lastRefresh{};
OreEspConfig g_config{};

TessBeginFn g_tessBegin = nullptr;
TessColorFn g_tessColor = nullptr;
TessVertexFn g_tessVertex = nullptr;
RenderMeshFn g_renderMesh = nullptr;
GetBlockFn g_getBlock = nullptr;
bool g_ready = false;

std::uint32_t colorForName(const std::string& name) {
    if (name.find("diamond") != std::string::npos) return 0xFF59E3FF;
    if (name.find("emerald") != std::string::npos) return 0xFF54E38A;
    if (name.find("gold") != std::string::npos) return 0xFFFFD24D;
    if (name.find("redstone") != std::string::npos) return 0xFFFF4A4A;
    if (name.find("lapis") != std::string::npos) return 0xFF4D75FF;
    if (name.find("copper") != std::string::npos) return 0xFFFF8A4D;
    if (name.find("iron") != std::string::npos) return 0xFFE2E6EB;
    if (name.find("coal") != std::string::npos) return 0xFFAAAAAA;
    if (name.find("quartz") != std::string::npos) return 0xFFFFFFFF;
    if (name.find("ancient_debris") != std::string::npos) return 0xFFFF9B4A;
    return 0;
}

bool isTargetOre(const std::string& name, std::uint32_t& color) {
    if (name.find("_ore") == std::string::npos && name.find("ancient_debris") == std::string::npos) return false;
    color = colorForName(name);
    return color != 0;
}

std::string blockName(const void* block) {
    if (!block) return {};
    const auto base = reinterpret_cast<std::uintptr_t>(block);
    const auto type = *reinterpret_cast<void* const*>(base + kBlockType);
    if (!type) return {};
    const auto nameInfo = *reinterpret_cast<void* const*>(reinterpret_cast<std::uintptr_t>(type) + kBlockTypeNameInfo);
    if (!nameInfo) return {};
    const auto fullName = *reinterpret_cast<void* const*>(reinterpret_cast<std::uintptr_t>(nameInfo) + kNameInfoFullName);
    if (!fullName) return {};
    const auto* str = reinterpret_cast<const std::string*>(reinterpret_cast<std::uintptr_t>(fullName) + kHashedStringString);
    if (str->size() > 96) return {};
    return *str;
}

void refreshCache(void* player) {
    if (!player || !g_getBlock) return;
    const auto dimension = *reinterpret_cast<void* const*>(reinterpret_cast<std::uintptr_t>(player) + kActorDimension);
    if (!dimension) return;
    const auto region = *reinterpret_cast<void* const*>(reinterpret_cast<std::uintptr_t>(dimension) + kDimensionBlockSource);
    if (!region) return;

    struct PosHash {
        std::size_t operator()(const std::uint64_t v) const noexcept { return static_cast<std::size_t>(v ^ (v >> 33)); }
    };

    std::vector<Highlight> next;
    next.reserve(static_cast<std::size_t>(g_config.maxHighlights));

    // The local player's state vector is the first component in the supplied BedrockTools layout.
    constexpr std::size_t kActorStateVector = 0x208;
    const auto state = *reinterpret_cast<void* const*>(reinterpret_cast<std::uintptr_t>(player) + kActorStateVector);
    if (!state) return;
    const auto position = *reinterpret_cast<const Vec3*>(state);

    const int rx = g_config.radiusHorizontal;
    const int ry = g_config.radiusVertical;
    const int baseX = static_cast<int>(std::floor(position.x));
    const int baseY = static_cast<int>(std::floor(position.y));
    const int baseZ = static_cast<int>(std::floor(position.z));

    for (int x = -rx; x <= rx && static_cast<int>(next.size()) < g_config.maxHighlights; ++x) {
        for (int y = -ry; y <= ry && static_cast<int>(next.size()) < g_config.maxHighlights; ++y) {
            for (int z = -rx; z <= rx && static_cast<int>(next.size()) < g_config.maxHighlights; ++z) {
                BlockPos p{baseX + x, baseY + y, baseZ + z};
                const void* block = g_getBlock(region, p);
                if (!block) continue;
                const std::string name = blockName(block);
                std::uint32_t color = 0;
                if (isTargetOre(name, color)) next.push_back({p, color});
            }
        }
    }

    std::lock_guard lock(g_cacheMutex);
    g_cache.swap(next);
    g_lastRefresh = std::chrono::steady_clock::now();
}

void drawBox(void* screenContext, void* tessellator, void* material, const Vec3& cam, const Highlight& h) {
    if (!g_tessBegin || !g_tessColor || !g_tessVertex || !g_renderMesh || !material) return;

    const float minX = static_cast<float>(h.pos.x);
    const float minY = static_cast<float>(h.pos.y);
    const float minZ = static_cast<float>(h.pos.z);
    const float maxX = minX + 1.0f;
    const float maxY = minY + 1.0f;
    const float maxZ = minZ + 1.0f;

    const float r = static_cast<float>((h.color >> 16) & 0xFF) / 255.0f;
    const float g = static_cast<float>((h.color >> 8) & 0xFF) / 255.0f;
    const float b = static_cast<float>(h.color & 0xFF) / 255.0f;

    g_tessBegin(tessellator, nullptr, kLines, 24, 0);
    g_tessColor(tessellator, r, g, b, 1.0f);

    const Vec3 p[8] = {
        {minX,minY,minZ},{maxX,minY,minZ},{maxX,minY,maxZ},{minX,minY,maxZ},
        {minX,maxY,minZ},{maxX,maxY,minZ},{maxX,maxY,maxZ},{minX,maxY,maxZ}
    };
    const int edges[24] = {
        0,1, 1,2, 2,3, 3,0,
        4,5, 5,6, 6,7, 7,4,
        0,4, 1,5, 2,6, 3,7
    };
    for (int i = 0; i < 24; ++i) {
        const auto& v = p[edges[i]];
        g_tessVertex(tessellator, v.x - cam.x, v.y - cam.y, v.z - cam.z);
    }

    char pad[0x58]{};
    g_renderMesh(screenContext, tessellator, material, pad);
}

void render(void* levelRenderer, void* screenContext) {
    if (!g_ready || !screenContext || !g_config.enabled) return;
    void* player = g_localPlayer.load(std::memory_order_acquire);
    if (!player) return;

    const auto now = std::chrono::steady_clock::now();
    if (g_lastRefresh.time_since_epoch().count() == 0
        || std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastRefresh).count() >= g_config.refreshMs) {
        refreshCache(player);
    }

    const auto lrp = *reinterpret_cast<void* const*>(reinterpret_cast<std::uintptr_t>(levelRenderer) + kLevelRendererPlayer);
    if (!lrp) return;
    const auto cam = *reinterpret_cast<const Vec3*>(reinterpret_cast<std::uintptr_t>(lrp) + kCameraPos);
    const auto material = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(lrp) + kSelectionMaterial);
    const auto tessellator = *reinterpret_cast<void* const*>(reinterpret_cast<std::uintptr_t>(screenContext) + kScreenTessellator);
    if (!tessellator || !material) return;

    const auto colorHolder = *reinterpret_cast<float* const*>(reinterpret_cast<std::uintptr_t>(screenContext) + kScreenColorHolder);
    if (!colorHolder) return;
    const float saved[4] = {colorHolder[0], colorHolder[1], colorHolder[2], colorHolder[3]};
    colorHolder[0] = colorHolder[1] = colorHolder[2] = colorHolder[3] = 1.0f;

    std::lock_guard lock(g_cacheMutex);
    for (const auto& h : g_cache) drawBox(screenContext, tessellator, material, cam, h);

    colorHolder[0] = saved[0];
    colorHolder[1] = saved[1];
    colorHolder[2] = saved[2];
    colorHolder[3] = saved[3];
}
}


extern "C" void oreespConfigureRender(std::uintptr_t begin, std::uintptr_t color, std::uintptr_t vertex, std::uintptr_t mesh, std::uintptr_t getBlock) {
    g_tessBegin = reinterpret_cast<TessBeginFn>(begin);
    g_tessColor = reinterpret_cast<TessColorFn>(color);
    g_tessVertex = reinterpret_cast<TessVertexFn>(vertex);
    g_renderMesh = reinterpret_cast<RenderMeshFn>(mesh);
    g_getBlock = reinterpret_cast<GetBlockFn>(getBlock);
    g_ready = g_tessBegin && g_tessColor && g_tessVertex && g_renderMesh && g_getBlock;
}

extern "C" void oreespSetClientPlayer(void* player) {
    g_localPlayer.store(player, std::memory_order_release);
}

extern "C" void oreespRender(void* levelRenderer, void* screenContext) {
    render(levelRenderer, screenContext);
}

