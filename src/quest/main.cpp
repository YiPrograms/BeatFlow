#include "beatnext/quest/CompositionRoot.hpp"
#include "beatnext/quest/LevelLifecycle.hpp"
#include "beatnext/quest/Logger.hpp"
#include "beatnext/quest/UI.hpp"

#include "beatsaber-hook/shared/utils/il2cpp-functions.hpp"
#include "bsml/shared/BSML.hpp"
#include "custom-types/shared/register.hpp"
#include "scotland2/shared/modloader.h"

#define BEATNEXT_EXPORT extern "C" __attribute__((visibility("default")))

BEATNEXT_EXPORT void setup(CModInfo* info) noexcept {
    info->id = MOD_ID;
    info->version = VERSION;
    info->version_long = 0;
}

BEATNEXT_EXPORT void late_load() noexcept {
    il2cpp_functions::Init();
    BSML::Init();
    custom_types::Register::AutoRegister();
    beatnext::quest::CompositionRoot::instance().initialize();
    beatnext::quest::ui::initialize();
    beatnext::quest::installLevelLifecycleHooks();
    Paper::Logger::fmtLogTag<Paper::LogLevel::INF>("BeatNext {} loaded", MOD_ID, VERSION);
}

BEATNEXT_EXPORT void unload() noexcept {
    beatnext::quest::CompositionRoot::instance().shutdown();
}
