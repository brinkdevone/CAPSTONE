# ============================================================================
#  CAPSTONE — Mastering Suite       Build : AU + VST3 + Standalone
# ============================================================================
cmake_minimum_required(VERSION 3.22)
project(CAPSTONE VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(CAPSTONE
    COMPANY_NAME             "Atelier Monolith"
    BUNDLE_ID                "com.ateliermonolith.capstone"
    PLUGIN_MANUFACTURER_CODE Atmn
    PLUGIN_CODE              Cpst
    FORMATS                  AU VST3 Standalone
    PRODUCT_NAME             "CAPSTONE"
    DESCRIPTION              "Mastering suite - LUFS, true peak, multibande, limiteur"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    COPY_PLUGIN_AFTER_BUILD  TRUE
    VST3_CATEGORIES          "Fx" "Mastering" "Dynamics" "EQ"
    AU_MAIN_TYPE             kAudioUnitType_Effect)

target_sources(CAPSTONE PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)

target_compile_definitions(CAPSTONE PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)

target_link_libraries(CAPSTONE PRIVATE juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags
         juce::juce_recommended_warning_flags)

add_executable(capstone_dsp_test tests/dsp_test.cpp)
target_compile_features(capstone_dsp_test PRIVATE cxx_std_17)
enable_testing()
add_test(NAME dsp COMMAND capstone_dsp_test)
