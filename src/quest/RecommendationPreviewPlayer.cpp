#include "beatnext/quest/RecommendationPreviewPlayer.hpp"

#include "beatnext/quest/Logger.hpp"

#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/AudioType.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Networking/DownloadHandlerAudioClip.hpp"
#include "UnityEngine/Networking/UnityWebRequest.hpp"
#include "UnityEngine/Networking/UnityWebRequestMultimedia.hpp"
#include "UnityEngine/Object.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"
#include "bsml/shared/BSML/SharedCoroutineStarter.hpp"
#include "custom-types/shared/coroutine.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>

namespace beatnext::quest::recommendation_preview {
namespace {

std::uint64_t requestGeneration = 0;
SafePtrUnity<UnityEngine::GameObject> previewObject;
SafePtrUnity<UnityEngine::AudioClip> activeClip;

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

UnityEngine::AudioSource* player() {
    if (!previewObject) {
        auto* object = UnityEngine::GameObject::New_ctor("BeatNext Preview Player");
        UnityEngine::Object::DontDestroyOnLoad(object);
        auto* source = object->AddComponent<UnityEngine::AudioSource*>();
        source->set_playOnAwake(false);
        source->set_loop(false);
        source->set_spatialBlend(0.0F);
        source->set_ignoreListenerPause(true);
        source->set_volume(0.75F);
        previewObject = object;
    }
    return previewObject.ptr()->GetComponent<UnityEngine::AudioSource*>();
}

void releaseClip() {
    if (previewObject) {
        auto* source = previewObject.ptr()->GetComponent<UnityEngine::AudioSource*>();
        if (source != nullptr) {
            source->Stop();
            source->set_clip(nullptr);
        }
    }
    if (activeClip)
        UnityEngine::Object::Destroy(activeClip.ptr());
    activeClip = nullptr;
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

    releaseClip();
    activeClip = clip;
    auto* previewPlayer = player();
    previewPlayer->set_clip(clip);
    previewPlayer->Play();
}

} // namespace

void play(const RecommendedMap& recommendation) {
    const auto generation = ++requestGeneration;
    releaseClip();

    const auto url = "https://cdn.beatsaver.com/" + lowerAscii(recommendation.map.hash) + ".mp3";
    BSML::SharedCoroutineStarter::get_instance()->StartCoroutine(
        custom_types::Helpers::CoroutineHelper::New(loadRemotePreview(url, generation)));
}

void stop() {
    ++requestGeneration;
    releaseClip();
}

} // namespace beatnext::quest::recommendation_preview
