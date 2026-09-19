cmake_minimum_required(VERSION 3.22)

# ============================================================================
# CRÍTICO: declarar C Y CXX
# ============================================================================
# JUCE necesita compilar juceaide (herramienta interna) en C puro.
# Si solo declaramos CXX, CMake falla con:
#   "Missing variable is: CMAKE_C_COMPILE_OBJECT"
project(MidiHarmonicHUD VERSION 1.0.0 LANGUAGES C CXX)

# ============================================================================
# C++17 (más seguro que C++20 en toolchains antiguos)
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# CRÍTICO PARA MAC PRO 5,1 + HIGH SIERRA
# ============================================================================
set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Solo Intel" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "High Sierra" FORCE)

# ============================================================================
# JUCE 8 vía FetchContent
# ============================================================================
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# ============================================================================
# Plugin: sintetizador (para que Ableton Live 12 procese MIDI)
# ============================================================================
# ⚠️ CAMBIOS IMPORTANTES:
#   - PLUGIN_MANUFACTURER_CODE: exactamente 4 caracteres (era 'Hlabs' = 5)
#   - BUNDLE_ID: sin espacios (era 'com.Harmonic Labs.MidiHarmonicHUD')
# ============================================================================
juce_add_plugin(MidiHarmonicHUD
    COMPANY_NAME "Harmonic Labs"
    PLUGIN_MANUFACTURER_CODE Hlbd          # 👈 4 caracteres exactos
    PLUGIN_CODE Mhud                       # 4 caracteres (ya estaba OK)
    BUNDLE_ID "com.harmoniclabs.midiharmonichud"   # 👈 sin espacios
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
# Fuentes
# ============================================================================
target_sources(MidiHarmonicHUD PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(MidiHarmonicHUD PRIVATE Source)

# ============================================================================
# Módulos JUCE
# ============================================================================
target_link_libraries(MidiHarmonicHUD PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# ============================================================================
# Flags específicos para macOS High Sierra + Mac Pro 5,1
# ============================================================================
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

# ============================================================================
# Definiciones de preprocesador CRÍTICAS
# ============================================================================
target_compile_definitions(MidiHarmonicHUD PUBLIC
    # ---- Compatibilidad macOS 10.13 / sin Metal ----
    JUCE_COREGRAPHICS_RENDER_WITH_METAL=0
    JUCE_COREGRAPHICS_DRAW_ASYNC=0

    # ---- Desactivar features que no usamos ----
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_CAMERA=0
    JUCE_VST3_CAN_REPLACE_VST2=0

    # ---- Splash screen ----
    JUCE_DISPLAY_SPLASH_SCREEN=0

    # ---- OpenGL opcional ----
    JUCE_OPENGL=1
    JUCE_OPENGL_ES=0
)
