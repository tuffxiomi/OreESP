#pragma once

#include <cstdint>
#include <string_view>

namespace oreesp::memory {

enum class Id : std::uint8_t {
    RenderLevel,
    TessellatorBegin,
    TessellatorColor,
    TessellatorVertex,
    RenderMeshImmediately2,
    RenderMeshImmediately,
    BlockSourceGetBlock,
    ClientInstanceUpdate,
    ClientInstanceGetLocalPlayer,
};

struct Definition {
    Id id;
    std::string_view pattern;
};

bool resolveAll(std::string_view libraryName = "libminecraftpe.so");
std::uintptr_t resolve(Id id);

}
