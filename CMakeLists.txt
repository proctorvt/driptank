cmake_minimum_required(VERSION 3.22)

if(APPLE)
    # one download that runs on both Apple Silicon and Intel Macs
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

project(Driptank VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(DRIPTANK_INSTALL_AFTER_BUILD "Copy the built plugins into your plug-in folders" OFF)
option(DRIPTANK_BUILD_TESTS "Build the offline render / screenshot tool" OFF)
set(JUCE_PATH "" CACHE PATH "Optional local JUCE checkout (otherwise it is downloaded)")

if(JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.10
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(DRIPTANK_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND DRIPTANK_FORMATS AU)
endif()

juce_add_plugin(Driptank
    PRODUCT_NAME "Driptank 63"
    COMPANY_NAME "Proctor"
    BUNDLE_ID com.proctor.driptank63
    PLUGIN_MANUFACTURER_CODE Prct
    PLUGIN_CODE Drp3
    FORMATS ${DRIPTANK_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Reverb
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Driptank needs your audio interface input to hear your guitar."
    COPY_PLUGIN_AFTER_BUILD ${DRIPTANK_INSTALL_AFTER_BUILD})

target_sources(Driptank PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Driptank PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Driptank
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(DRIPTANK_BUILD_TESTS)
    juce_add_console_app(driptank_tool PRODUCT_NAME "driptank_tool")
    target_sources(driptank_tool PRIVATE
        tools/DriptankTool.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_compile_definitions(driptank_tool PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(driptank_tool PRIVATE
        juce::juce_audio_processors juce::juce_audio_formats juce::juce_dsp juce::juce_gui_basics
        juce::juce_recommended_config_flags)
endif()
