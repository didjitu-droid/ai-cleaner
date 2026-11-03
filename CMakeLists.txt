cmake_minimum_required(VERSION 3.22)

project(AICleaner VERSION 1.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Universal (Apple Silicon + Intel) build on macOS
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
endif()

# JUCE: use a local copy if JUCE_DIR is given, otherwise download it.
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

option(AICLEANER_BUILD_TESTS "Build the offline DSP test tool" OFF)
option(AICLEANER_BUILD_PLUGIN "Build the plugin" ON)

set(ENGINE_SOURCES
    Source/CleanerEngine.cpp
    Source/CleanerEngine.h
    Source/Biquad.h
    Source/Presets.h)

if(AICLEANER_BUILD_PLUGIN)
    if(APPLE)
        set(AICLEANER_FORMATS VST3 AU)
    else()
        set(AICLEANER_FORMATS VST3)
    endif()

    juce_add_plugin(AICleaner
        COMPANY_NAME "MAAURA"
        COMPANY_WEBSITE "https://www.instagram.com/maaura.music/"
        BUNDLE_ID "com.maaura.aicleaner"
        PLUGIN_MANUFACTURER_CODE Maau
        PLUGIN_CODE AiCl
        FORMATS ${AICLEANER_FORMATS}
        PRODUCT_NAME "AI Cleaner"
        DESCRIPTION "Cleans artifacts from AI-generated stems"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        AU_MAIN_TYPE kAudioUnitType_Effect
        VST3_CATEGORIES Fx Restoration
        COPY_PLUGIN_AFTER_BUILD FALSE)

    juce_add_binary_data(AICleanerFonts
        HEADER_NAME FontData.h
        NAMESPACE FontData
        SOURCES
            Resources/Fonts/Anton-Regular.ttf
            Resources/Fonts/Outfit-SemiBold.ttf
            Resources/Fonts/Outfit-Bold.ttf)

    target_sources(AICleaner PRIVATE
        ${ENGINE_SOURCES}
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/UIComponents.cpp
        Source/UIComponents.h)

    target_compile_definitions(AICleaner PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(AICleaner
        PRIVATE
            AICleanerFonts
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

if(AICLEANER_BUILD_TESTS)
    juce_add_console_app(AICleanerTest PRODUCT_NAME "AICleanerTest")
    target_sources(AICleanerTest PRIVATE ${ENGINE_SOURCES} Tools/OfflineTest.cpp)
    target_compile_definitions(AICleanerTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(AICleanerTest
        PRIVATE juce::juce_dsp juce::juce_audio_formats
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
