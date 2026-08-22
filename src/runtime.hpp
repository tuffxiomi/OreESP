#pragma once

#include <pl/Mod.hpp>
#include "ore_esp.hpp"

namespace oreesp {

class Runtime {
public:
    static Runtime& get();
    bool load(pl::mod::ModContext& context);
    bool enable(pl::mod::ModContext& context);
    bool disable(pl::mod::ModContext& context);
    bool unload(pl::mod::ModContext& context);

private:
    bool launcherContext() const;
    bool resolveSignatures();
    void minecraftLoaded();
    bool installHooks();
};

}
