# LOAF BREAKER VST: the plugin (VST3, Standalone; AU on a Mac). JUCE from JUCE_DIR (a checkout) or fetched.
cmake_minimum_required(VERSION 3.22)
project(LoafBreakerVST VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 20)
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 9.0.6 GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()
set(FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FORMATS AU)
endif()
juce_add_plugin(LoafBreaker
    COMPANY_NAME "FLEXOR"
    PLUGIN_MANUFACTURER_CODE Flxr
    PLUGIN_CODE Lbrk
    PRODUCT_NAME "LOAF BREAKER"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Drum
    FORMATS ${FORMATS})
juce_add_binary_data(LoafAssets SOURCES Resources/BarlowCondensed-Medium.ttf Resources/BarlowCondensed-SemiBold.ttf
    Resources/BarlowCondensed-Bold.ttf Resources/loaflord_face.png Resources/loaflord_title.png)
target_sources(LoafBreaker PRIVATE Source/Core.cpp Source/Fm1Link.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp
    Source/Ui.cpp Source/KeyMap.cpp
        Source/Exchange.cpp)
target_include_directories(LoafBreaker PRIVATE ../core)
target_compile_definitions(LoafBreaker PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(LoafBreaker PRIVATE LoafAssets juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# the host's check (Test/HostTest.cpp): lbtest PATH.vst3 OUT.wav
juce_add_console_app(lbtest PRODUCT_NAME "lbtest")
target_sources(lbtest PRIVATE Test/HostTest.cpp)
target_compile_definitions(lbtest PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(lbtest PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)
