#include "beatnext/quest/RecommendationPreviewPlayer.hpp"

#include "beatnext/quest/Logger.hpp"

#include "GlobalNamespace/LevelCollectionViewController.hpp"
#include "GlobalNamespace/SongPreviewPlayer.hpp"
#include "System/Action.hpp"
#include "System/Threading/CancellationToken.hpp"
#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/AudioType.hpp"
#include "UnityEngine/Networking/DownloadHandlerAudioClip.hpp"
#include "UnityEngine/Networking/UnityWebRequest.hpp"
#include "UnityEngine/Networking/UnityWebRequestMultimedia.hpp"
#include "UnityEngine/Object.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "bsml/shared/BSML/SharedCoroutineStarter.hpp"
#include "bsml/shared/Helpers/delegates.hpp"
#include "bsml/shared/Helpers/getters.hpp"
#include "custom-types/shared/coroutine.hpp"
#include "songcore/shared/SongCore.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>

namespace beatnext::quest::recommendation_preview {
namespace {

std::uint64_t requestGeneration = 0;

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

GlobalNamespace::SongPreviewPlayer* player() {
    return BSML::Helpers::GetDiContainer()->Resolve<GlobalNamespace::SongPreviewPlayer*>();
}

custom_types::Helpers::Coroutine loadRemotePreview(std::string url, std::uint64_t generation) {
    auto* request = UnityEngine::Networking::UnityWebRequestMultimedia::GetAudioClip(
        il2cpp_utils::newcsstr(url), UnityEngine::AudioType::MPEG);
    co_yield reinterpret_cast<System::Collections::IEnumerator*>(request->SendWebRequest());
    if (request->GetError() != UnityEngine::Networking::UnityWebRequest::UnityWebRequestError::OK) {
        logger.warn("Could not load BeatSaver preview audio");
        request->Dispose();
        co_return;
    }

    UnityW<UnityEngine::AudioClip> clip =
        UnityEngine::Networking::DownloadHandlerAudioClip::GetContent(request);
    request->Dispose();
    if (clip == nullptr)
        co_return;
    if (generation != requestGeneration) {
        UnityEngine::Object::Destroy(clip);
        co_return;
    }

    auto* previewPlayer = player();
    if (previewPlayer == nullptr) {
        UnityEngine::Object::Destroy(clip);
        co_return;
    }
    auto* cleanup = BSML::MakeDelegate<System::Action*>(std::function<void()>([clip] {
        if (clip != nullptr)
            UnityEngine::Object::Destroy(clip);
    }));
    previewPlayer->CrossfadeTo(clip, -5.0F, 0.0F, clip->get_length(), cleanup);
}

} // namespace

void play(const RecommendedMap& recommendation) {
    const auto generation = ++requestGeneration;
    if (auto* level = SongCore::API::Loading::GetLevelByHash(recommendation.map.hash)) {
        auto* controller =
            BSML::Helpers::GetDiContainer()->Resolve<GlobalNamespace::LevelCollectionViewController*>();
        if (controller != nullptr) {
            controller->SongPlayerCrossfadeToLevelAsync(level,
                                                        System::Threading::CancellationToken::get_None());
            return;
        }
    }

    const auto url = "https://cdn.beatsaver.com/" + lowerAscii(recommendation.map.hash) + ".mp3";
    BSML::SharedCoroutineStarter::get_instance()->StartCoroutine(
        custom_types::Helpers::CoroutineHelper::New(loadRemotePreview(url, generation)));
}

void stop() {
    ++requestGeneration;
    if (auto* previewPlayer = player())
        previewPlayer->CrossfadeToDefault();
}

} // namespace beatnext::quest::recommendation_preview
