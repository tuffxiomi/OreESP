#pragma once

#include <pl/Mod.hpp>
#include <cstdint>
#include <string>
#include <vector>

struct OreEspConfig {
    int radiusHorizontal = 16;
    int radiusVertical = 8;
    int maxHighlights = 384;
    int refreshMs = 160;
    float lineWidth = 1.0f;
    bool enabled = true;
};

class OreEspMod {
public:
    static OreEspMod& instance();

    bool load(pl::mod::ModContext& context);
    bool enable(pl::mod::ModContext& context);
    bool disable(pl::mod::ModContext& context);
    bool unload(pl::mod::ModContext& context);

private:
    OreEspMod() = default;
    OreEspMod(const OreEspMod&) = delete;
    OreEspMod& operator=(const OreEspMod&) = delete;

    friend class OreEspRuntime;
};
