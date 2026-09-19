cmake_minimum_required(VERSION 3.22)

project(MidiHarmonicHUD VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Solo Intel" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "High Sierra" FORCE)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MidiHarmonicHUD
    COMPANY_NAME "Harmonic Labs"
    PLUGIN_MANUFACTURER_CODE Hlbd
    PLUGIN_CODE Mhud
    BUNDLE_ID "com.harmoniclabs.midiharmonichud"
    FORMATS VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    PLUGIN_NAME "MIDI Harmonic HUD"
)

# ============================================================================
# Fuentes (añadimos ThemeManager.cpp)
# ============================================================================
target_sources(MidiHarmonicHUD PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ThemeManager.cpp
)

target_include_directories(MidiHarmonicHUD PRIVATE Source)

target_link_libraries(MidiHarmonicHUD PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

if(APPLE)
    target_compile_options(MidiHarmonicHUD PRIVATE
        -mmacosx-version-min=10.13
        -Wno-deprecated-declarations
        -Wno-unguarded-availability-new
    )
    target_link_options(MidiHarmonicHUD PRIVATE
        -mmacosx-version-min=10.13
    )
endif()

target_compile_definitions(MidiHarmonicHUD PUBLIC
    JUCE_COREGRAPHICS_RENDER_WITH_METAL=0
    JUCE_COREGRAPHICS_DRAW_ASYNC=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_CAMERA=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_OPENGL=1
    JUCE_OPENGL_ES=0
)
