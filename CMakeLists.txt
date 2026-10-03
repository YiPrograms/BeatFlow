cmake_minimum_required(VERSION 3.21)

option(BEATFLOW_BUILD_TESTS "Build portable tests" ON)
option(BEATFLOW_BUILD_QUEST "Build the Quest mod" OFF)

# QPM's generated definitions select the Android toolchain, so load them only for Quest builds.
if(BEATFLOW_BUILD_QUEST)
    if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/qpm_defines.cmake")
        message(FATAL_ERROR "Quest build definitions are missing. Run `qpm restore` first.")
    endif()
    include("${CMAKE_CURRENT_LIST_DIR}/qpm_defines.cmake")
endif()

project(BeatFlow VERSION 0.1.0 LANGUAGES CXX)

find_package(ZLIB REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
find_package(nlohmann_json 3.11 QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

add_library(beatflow_core STATIC
    src/core/Matcher.cpp
    src/core/RecommendationEngine.cpp
    src/core/TextNormalizer.cpp
)
target_include_directories(beatflow_core PUBLIC include)
target_compile_features(beatflow_core PUBLIC cxx_std_20)

add_library(beatflow_services STATIC
    src/services/AtomicJsonCache.cpp
    src/services/BeatSaverCatalog.cpp
    src/services/HttpHeaders.cpp
    src/services/OAuthClient.cpp
    src/services/RetryingHttpClient.cpp
    src/services/WorkerQueue.cpp
    src/services/YouTubeMusicProvider.cpp
    src/services/ZipArchiveExtractor.cpp
    src/services/ZipArchiveValidator.cpp
)
target_include_directories(beatflow_services PUBLIC include)
target_link_libraries(beatflow_services PUBLIC beatflow_core nlohmann_json::nlohmann_json ZLIB::ZLIB)

foreach(target beatflow_core beatflow_services)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endforeach()

if(BEATFLOW_BUILD_TESTS AND NOT ANDROID)
    enable_testing()
    add_executable(beatflow_tests
        tests/main.cpp
        tests/MatcherTests.cpp
        tests/ProviderTests.cpp
        tests/RecommendationEngineTests.cpp
        tests/StorageTests.cpp
        tests/TransportTests.cpp
    )
    target_link_libraries(beatflow_tests PRIVATE beatflow_services)
    target_compile_definitions(beatflow_tests PRIVATE BEATFLOW_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME beatflow_tests COMMAND beatflow_tests)
endif()

if(BEATFLOW_BUILD_QUEST OR ANDROID)
    if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/extern.cmake")
        message(FATAL_ERROR "Quest dependencies are missing. Run `qpm restore` first.")
    endif()

    include("${EXTERN_DIR}/includes/kaleb/shared/cmake/assets.cmake")

    set(BEATFLOW_OAUTH_CLIENT_ID "$ENV{BEATFLOW_OAUTH_CLIENT_ID}" CACHE STRING
        "Google TVs and Limited Input devices OAuth client ID")
    set(BEATFLOW_OAUTH_CLIENT_SECRET "$ENV{BEATFLOW_OAUTH_CLIENT_SECRET}" CACHE STRING
        "Google TVs and Limited Input devices OAuth client secret")
    mark_as_advanced(BEATFLOW_OAUTH_CLIENT_ID BEATFLOW_OAUTH_CLIENT_SECRET)
    string(REPLACE "\\" "\\\\" BEATFLOW_OAUTH_CLIENT_ID_ESCAPED "${BEATFLOW_OAUTH_CLIENT_ID}")
    string(REPLACE "\"" "\\\"" BEATFLOW_OAUTH_CLIENT_ID_ESCAPED "${BEATFLOW_OAUTH_CLIENT_ID_ESCAPED}")
    string(REPLACE "\\" "\\\\" BEATFLOW_OAUTH_CLIENT_SECRET_ESCAPED "${BEATFLOW_OAUTH_CLIENT_SECRET}")
    string(REPLACE "\"" "\\\"" BEATFLOW_OAUTH_CLIENT_SECRET_ESCAPED
                   "${BEATFLOW_OAUTH_CLIENT_SECRET_ESCAPED}")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/include/beatflow/quest/BuildConfig.hpp.in"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/beatflow/quest/BuildConfig.hpp"
        @ONLY
    )

    add_library(BeatFlow SHARED
        src/quest/AndroidKeystore.cpp
        src/quest/BeatFlowFlowCoordinator.cpp
        src/quest/CompositionRoot.cpp
        src/quest/ForYouViewController.cpp
        src/quest/LevelLifecycle.cpp
        src/quest/PauseRecommendedNextPanel.cpp
        src/quest/QuestHttpClient.cpp
        src/quest/QuestCredentialStore.cpp
        src/quest/QuestSongLibrary.cpp
        src/quest/RecommendedNextPanel.cpp
        src/quest/SongDetailsCatalog.cpp
        src/quest/main.cpp
    )
    file(GLOB BEATFLOW_INLINE_HOOK_SOURCES CONFIGURE_DEPENDS
        "${EXTERN_DIR}/includes/beatsaber-hook/shared/inline-hook/*.c"
        "${EXTERN_DIR}/includes/beatsaber-hook/shared/inline-hook/*.cpp")
    target_sources(BeatFlow PRIVATE ${BEATFLOW_INLINE_HOOK_SOURCES})
    add_assets(BeatFlow-assets STATIC "${CMAKE_CURRENT_SOURCE_DIR}/assets"
               "${CMAKE_CURRENT_SOURCE_DIR}/include/beatflow/quest/Assets.hpp")
    target_include_directories(BeatFlow PRIVATE include "${CMAKE_CURRENT_BINARY_DIR}/generated"
                                             "${EXTERN_DIR}/includes/${CODEGEN_ID}/include")
    target_compile_options(BeatFlow PRIVATE -frtti -fexceptions -fPIC -fvisibility=hidden)
    target_compile_definitions(BeatFlow PRIVATE
        MOD_ID="BeatFlow"
        VERSION="${PROJECT_VERSION}"
        BEATFLOW_QUEST=1
    )
    target_link_libraries(BeatFlow PRIVATE beatflow_services BeatFlow-assets -llog -lz)
    include("${CMAKE_CURRENT_LIST_DIR}/extern.cmake")
endif()
