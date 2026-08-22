#include "runtime.hpp"
#include "signatures.hpp"
#include <pl/memory/Hook.hpp>
#include <dlfcn.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>

extern "C" void oreespSetClientPlayer(void* player);
extern "C" void oreespRender(void* levelRenderer, void* screenContext);
extern "C" void oreespConfigureRender(std::uintptr_t begin, std::uintptr_t color, std::uintptr_t vertex, std::uintptr_t mesh, std::uintptr_t getBlock);

namespace oreesp {
namespace {
std::atomic_bool g_enabled{false};
std::atomic_bool g_resolved{false};
std::atomic_bool g_installed{false};
std::mutex g_mutex;

void* (*g_dlopenOriginal)(const char*, int) = nullptr;
bool (*g_clientUpdateOriginal)(void*, bool) = nullptr;
void* (*g_getLocalPlayer)(void*) = nullptr;
void (*g_renderLevelOriginal)(void*, void*, void*) = nullptr;

bool launcherContextImpl() {
    FILE* f = std::fopen("/proc/self/cmdline", "rb");
    if (!f) return false;
    char command[256]{};
    const auto n = std::fread(command, 1, sizeof(command) - 1, f);
    std::fclose(f);
    if (n == 0) return false;
    return std::strcmp(command, "org.levimc.launcher") == 0
        || std::strcmp(command, "org.levimc.launcher:minecraft") == 0
        || std::strcmp(command, "com.mojang.minecraftpe") == 0;
}

bool resolveUnlocked() {
    if (g_resolved.load(std::memory_order_acquire)) return true;
    const bool ok = memory::resolveAll("libminecraftpe.so");
    if (ok) {
        g_getLocalPlayer = reinterpret_cast<void* (*)(void*)>(memory::resolve(memory::Id::ClientInstanceGetLocalPlayer));
        oreespConfigureRender(
            memory::resolve(memory::Id::TessellatorBegin),
            memory::resolve(memory::Id::TessellatorColor),
            memory::resolve(memory::Id::TessellatorVertex),
            memory::resolve(memory::Id::RenderMeshImmediately2) ? memory::resolve(memory::Id::RenderMeshImmediately2) : memory::resolve(memory::Id::RenderMeshImmediately),
            memory::resolve(memory::Id::BlockSourceGetBlock)
        );
    }
    g_resolved.store(ok, std::memory_order_release);
    return ok;
}

bool installUnlocked() {
    if (g_installed.load(std::memory_order_acquire)) return true;
    if (!resolveUnlocked()) return false;

    const auto clientUpdate = memory::resolve(memory::Id::ClientInstanceUpdate);
    const auto renderLevel = memory::resolve(memory::Id::RenderLevel);
    if (!clientUpdate || !renderLevel || !g_getLocalPlayer) return false;

    g_clientUpdateOriginal = reinterpret_cast<bool (*)(void*, bool)>(clientUpdate);
    g_renderLevelOriginal = reinterpret_cast<void (*)(void*, void*, void*)>(renderLevel);

    const int a = pl::memory::hook(
        reinterpret_cast<void*>(clientUpdate),
        reinterpret_cast<void*>(+[](void* client, bool force) -> bool {
            const bool result = g_clientUpdateOriginal ? g_clientUpdateOriginal(client, force) : true;
            if (g_getLocalPlayer) oreespSetClientPlayer(g_getLocalPlayer(client));
            return result;
        }),
        reinterpret_cast<void**>(&g_clientUpdateOriginal)
    );
    if (a != 0) return false;

    const int b = pl::memory::hook(
        reinterpret_cast<void*>(renderLevel),
        reinterpret_cast<void*>(+[](void* self, void* screenContext, void* a3) {
            if (g_renderLevelOriginal) g_renderLevelOriginal(self, screenContext, a3);
            if (g_enabled.load(std::memory_order_acquire)) oreespRender(self, screenContext);
        }),
        reinterpret_cast<void**>(&g_renderLevelOriginal)
    );
    if (b != 0) return false;

    g_installed.store(true, std::memory_order_release);
    return true;
}

void* dlopenDetour(const char* filename, int flags) {
    void* handle = g_dlopenOriginal ? g_dlopenOriginal(filename, flags) : nullptr;
    if (handle && filename && std::strstr(filename, "libminecraftpe.so") && g_enabled.load(std::memory_order_acquire)) {
        std::lock_guard lock(g_mutex);
        installUnlocked();
    }
    return handle;
}
}

Runtime& Runtime::get() {
    static Runtime runtime;
    return runtime;
}

bool Runtime::launcherContext() const {
    return launcherContextImpl();
}

bool Runtime::resolveSignatures() {
    std::lock_guard lock(g_mutex);
    return resolveUnlocked();
}

bool Runtime::installHooks() {
    std::lock_guard lock(g_mutex);
    return installUnlocked();
}

void Runtime::minecraftLoaded() {
    if (!g_enabled.load(std::memory_order_acquire)) return;
    installHooks();
}

bool Runtime::load(pl::mod::ModContext&) {
    if (!launcherContextImpl()) return true;

    void* minecraft = dlopen("libminecraftpe.so", RTLD_NOW | RTLD_NOLOAD);
    if (minecraft) {
        dlclose(minecraft);
        resolveSignatures();
        return true;
    }

    void* libdl = dlopen("libdl.so", RTLD_NOW | RTLD_NOLOAD);
    if (!libdl) libdl = dlopen("libdl.so", RTLD_NOW);
    if (!libdl) return true;

    void* symbol = dlsym(libdl, "dlopen");
    if (symbol) {
        pl::memory::hook(symbol, reinterpret_cast<void*>(dlopenDetour), reinterpret_cast<void**>(&g_dlopenOriginal));
    }
    dlclose(libdl);
    return true;
}

bool Runtime::enable(pl::mod::ModContext&) {
    g_enabled.store(true, std::memory_order_release);
    if (!launcherContextImpl()) return true;

    void* minecraft = dlopen("libminecraftpe.so", RTLD_NOW | RTLD_NOLOAD);
    if (!minecraft) return true;
    resolveSignatures();
    installHooks();
    dlclose(minecraft);
    return true;
}

bool Runtime::disable(pl::mod::ModContext&) {
    g_enabled.store(false, std::memory_order_release);
    return true;
}

bool Runtime::unload(pl::mod::ModContext&) {
    g_enabled.store(false, std::memory_order_release);
    return true;
}

}
