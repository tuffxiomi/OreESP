#include "ore_esp.hpp"
#include "runtime.hpp"

OreEspMod& OreEspMod::instance() {
    static OreEspMod mod;
    return mod;
}

bool OreEspMod::load(pl::mod::ModContext& context) { return oreesp::Runtime::get().load(context); }
bool OreEspMod::enable(pl::mod::ModContext& context) { return oreesp::Runtime::get().enable(context); }
bool OreEspMod::disable(pl::mod::ModContext& context) { return oreesp::Runtime::get().disable(context); }
bool OreEspMod::unload(pl::mod::ModContext& context) { return oreesp::Runtime::get().unload(context); }
