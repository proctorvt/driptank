cmake_minimum_required(VERSION 3.22)

if(APPLE)
    # one download that runs on both Apple Silicon and Intel Macs
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

project(Mudpuppy VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MUDPUPPY_INSTALL_AFTER_BUILD "Copy the built plugins into your plug-in folders" OFF)
option(MUDPUPPY_BUILD_TESTS "Build the offline render / screenshot tool" OFF)
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

set(MUDPUPPY_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND MUDPUPPY_FORMATS AU)
endif()

juce_add_plugin(Mudpuppy
    PRODUCT_NAME "Mudpuppy"
    COMPANY_NAME "Proctor"
    BUNDLE_ID com.proctor.mudpuppy
    PLUGIN_MANUFACTURER_CODE Prct
    PLUGIN_CODE Mdp1
    FORMATS ${MUDPUPPY_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Delay
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Mudpuppy needs your audio interface input to hear your guitar."
    COPY_PLUGIN_AFTER_BUILD ${MUDPUPPY_INSTALL_AFTER_BUILD})

target_sources(Mudpuppy PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Mudpuppy PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Mudpuppy
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(MUDPUPPY_BUILD_TESTS)
    juce_add_console_app(mudpuppy_tool PRODUCT_NAME "mudpuppy_tool")
    target_sources(mudpuppy_tool PRIVATE
        tools/MudpuppyTool.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_compile_definitions(mudpuppy_tool PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(mudpuppy_tool PRIVATE
        juce::juce_audio_processors juce::juce_audio_formats juce::juce_dsp juce::juce_gui_basics
        juce::juce_recommended_config_flags)
endif()
