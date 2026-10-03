#include "beatflow/quest/CompositionRoot.hpp"
#include "beatflow/quest/LevelLifecycle.hpp"
#include "beatflow/quest/Logger.hpp"
#include "beatflow/quest/UI.hpp"

#include "beatsaber-hook/shared/utils/il2cpp-functions.hpp"
#include "bsml/shared/BSML.hpp"
#include "custom-types/shared/register.hpp"
#include "scotland2/shared/modloader.h"

#define BEATFLOW_EXPORT extern "C" __attribute__((visibility("default")))

BEATFLOW_EXPORT void setup(CModInfo* info) noexcept {
    info->id = MOD_ID;
    info->version = VERSION;
    info->version_long = 0;
}

BEATFLOW_EXPORT void late_load() noexcept {
    il2cpp_functions::Init();
    BSML::Init();
    custom_types::Register::AutoRegister();
    beatflow::quest::CompositionRoot::instance().initialize();
    beatflow::quest::ui::initialize();
    beatflow::quest::installLevelLifecycleHooks();
    Paper::Logger::fmtLogTag<Paper::LogLevel::INF>("BeatFlow {} loaded", MOD_ID, VERSION);
}

BEATFLOW_EXPORT void unload() noexcept {
    beatflow::quest::CompositionRoot::instance().shutdown();
}
