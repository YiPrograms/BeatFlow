cmake_minimum_required(VERSION 3.21)

option(BEATNEXT_BUILD_TESTS "Build portable tests" ON)
option(BEATNEXT_BUILD_QUEST "Build the Quest mod" OFF)

# QPM's generated definitions select the Android toolchain, so load them only for Quest builds.
if(BEATNEXT_BUILD_QUEST)
    if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/qpm_defines.cmake")
        message(FATAL_ERROR "Quest build definitions are missing. Run `qpm restore` first.")
    endif()
    include("${CMAKE_CURRENT_LIST_DIR}/qpm_defines.cmake")
endif()

project(BeatNext VERSION 0.1.0 LANGUAGES CXX)

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

add_library(beatnext_core STATIC
    src/core/Matcher.cpp
    src/core/RecommendationEngine.cpp
    src/core/RecommendationSession.cpp
    src/core/TextNormalizer.cpp
)
target_include_directories(beatnext_core PUBLIC include)
target_compile_features(beatnext_core PUBLIC cxx_std_20)

add_library(beatnext_services STATIC
    src/services/AtomicJsonCache.cpp
    src/services/BeatSaverCatalog.cpp
    src/services/HttpHeaders.cpp
    src/services/RetryingHttpClient.cpp
    src/services/WorkerQueue.cpp
    src/services/YouTubeMusicProvider.cpp
    src/services/ZipArchiveExtractor.cpp
    src/services/ZipArchiveValidator.cpp
)
target_include_directories(beatnext_services PUBLIC include)
target_link_libraries(beatnext_services PUBLIC beatnext_core nlohmann_json::nlohmann_json ZLIB::ZLIB)

foreach(target beatnext_core beatnext_services)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endforeach()

if(BEATNEXT_BUILD_TESTS AND NOT ANDROID)
    enable_testing()
    add_executable(beatnext_tests
        tests/main.cpp
        tests/MatcherTests.cpp
        tests/ProviderTests.cpp
        tests/RecommendationEngineTests.cpp
        tests/RecommendationSessionTests.cpp
        tests/StorageTests.cpp
        tests/TransportTests.cpp
    )
    target_link_libraries(beatnext_tests PRIVATE beatnext_services)
    target_compile_definitions(beatnext_tests PRIVATE BEATNEXT_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME beatnext_tests COMMAND beatnext_tests)
endif()

if(BEATNEXT_BUILD_QUEST OR ANDROID)
    if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/extern.cmake")
        message(FATAL_ERROR "Quest dependencies are missing. Run `qpm restore` first.")
    endif()

    include("${EXTERN_DIR}/includes/kaleb/shared/cmake/assets.cmake")

    add_library(BeatNext SHARED
        src/quest/BeatNextFlowCoordinator.cpp
        src/quest/CompositionRoot.cpp
        src/quest/LevelLifecycle.cpp
        src/quest/QuestHttpClient.cpp
        src/quest/QuestMapInstaller.cpp
        src/quest/SettingsViewController.cpp
        src/quest/SongSelectionNavigator.cpp
        src/quest/SongDetailsCatalog.cpp
        src/quest/UpNextListCell.cpp
        src/quest/UpNextPanelController.cpp
        src/quest/main.cpp
    )
    file(GLOB BEATNEXT_INLINE_HOOK_SOURCES CONFIGURE_DEPENDS
        "${EXTERN_DIR}/includes/beatsaber-hook/shared/inline-hook/*.c"
        "${EXTERN_DIR}/includes/beatsaber-hook/shared/inline-hook/*.cpp")
    target_sources(BeatNext PRIVATE ${BEATNEXT_INLINE_HOOK_SOURCES})
    add_assets(BeatNext-assets STATIC "${CMAKE_CURRENT_SOURCE_DIR}/assets"
               "${CMAKE_CURRENT_SOURCE_DIR}/include/beatnext/quest/Assets.hpp")
    target_include_directories(BeatNext PRIVATE include
                                             "${EXTERN_DIR}/includes/${CODEGEN_ID}/include")
    target_compile_options(BeatNext PRIVATE -frtti -fexceptions -fPIC -fvisibility=hidden)
    target_compile_definitions(BeatNext PRIVATE
        MOD_ID="BeatNext"
        VERSION="${PROJECT_VERSION}"
        BEATNEXT_QUEST=1
    )
    target_link_libraries(BeatNext PRIVATE beatnext_services BeatNext-assets -llog -lz)
    include("${CMAKE_CURRENT_LIST_DIR}/extern.cmake")
endif()
