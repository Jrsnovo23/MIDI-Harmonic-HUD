cmake_minimum_required(VERSION 3.22)
project(MidiHarmonicHUD VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Forzar compilación universal (Intel + Apple Silicon)
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Universal binary" FORCE)
# Target mínimo macOS 10.13 (High Sierra)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version" FORCE)

# JUCE 8 vía FetchContent
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# Crear el plugin
juce_add_plugin(MidiHarmonicHUD
    COMPANY_NAME "Harmonic Labs"
    PLUGIN_MANUFACTURER_CODE Hlabs
    PLUGIN_CODE Mhud
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

# Fuentes
target_sources(MidiHarmonicHUD PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(MidiHarmonicHUD PRIVATE Source)

# Módulos JUCE necesarios
target_link_libraries(MidiHarmonicHUD PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Flags adicionales para evitar símbolos faltantes en libc++ en macOS 10.13
if(APPLE)
    target_compile_options(MidiHarmonicHUD PRIVATE
        -mmacosx-version-min=10.13
        -Wno-deprecated-declarations
    )
    target_link_options(MidiHarmonicHUD PRIVATE
        -mmacosx-version-min=10.13
    )
endif()

# Definiciones para el preprocesador
target_compile_definitions(MidiHarmonicHUD PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)
