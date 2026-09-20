#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "PluginProcessor.h"
#include "ThemeManager.h"

//==============================================================================
class MidiHarmonicHUDEditor : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    explicit MidiHarmonicHUDEditor (MidiHarmonicHUDProcessor&);
    ~MidiHarmonicHUDEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    // Subsistemas de dibujo
    void drawHeader (juce::Graphics&, juce::Rectangle<int>);
    void drawDetectingPanel (juce::Graphics&, juce::Rectangle<int>);
    void drawCircleOfFifths (juce::Graphics&, juce::Rectangle<int>);
    void drawPianoKeyboard (juce::Graphics&, juce::Rectangle<int>);
    void drawHistoryPanel (juce::Graphics&, juce::Rectangle<int>);
    void drawDiatonicPanel (juce::Graphics&, juce::Rectangle<int>);
    void drawTensionGraph (juce::Graphics&, juce::Rectangle<int>);
    void drawConfidenceMeter (juce::Graphics&, juce::Rectangle<int>);
    void drawTensionGradient (juce::Graphics&, juce::Rectangle<int>);
    void drawGlowText (juce::Graphics&, const juce::String&, juce::Rectangle<int>,
                       juce::Colour, float fontSize, float alpha);

    MidiHarmonicHUDProcessor& processorRef;

    // =========================================================================
    // ⚠️ ORDEN CRÍTICO: PRIMERO los controles de UI, DESPUÉS los attachments.
    // En C++, los miembros se inicializan en el orden de declaración, NO en
    // el orden de la lista de inicialización del constructor. Si los
    // attachments van antes que los controles, intentan engancharse a un
    // objeto que aún no existe → CRASH al instanciar en Ableton Live.
    // =========================================================================

    // ---- 1) Controles de UI ----
    juce::ToggleButton muteButton { "Mute Piano" };
    juce::ComboBox presetCombo;
    juce::ComboBox themeCombo;
    juce::Label presetLabel;
    juce::Label themeLabel;

    // ---- 2) Attachments (deben ir DESPUÉS de los controles) ----
    juce::AudioProcessorValueTreeState::ButtonAttachment   muteAttachment;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment presetAttachment;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment themeAttachment;

    // ---- 3) Tema actual ----
    ThemeColors theme;

    // ---- 4) Estado cacheado ----
    juce::String cachedChord { "---" };
    juce::StringArray cachedHistory;
    std::array<bool, 128> cachedActiveNotes {};
    float cachedConfidence = 0.0f;
    float cachedTension    = 0.0f;
    int   cachedRootPC     = -1;
    int   cachedBassPC     = -1;
    int   cachedInversion  = 0;
    int   cachedActiveCount = 0;
    juce::String cachedKeyText { "---" };
    float cachedKeyConfidence = 0.0f;
    int   cachedThemeId = 0;

    // ---- 5) Buffer circular de tensión ----
    std::array<float, MidiHarmonicHUDProcessor::TENSION_HISTORY_SIZE> cachedTensionHistory {};

    // ---- 6) Animaciones ----
    juce::String lastDisplayedChord;
    juce::uint32 chordChangeTimeMs = 0;
    float chordFadeAlpha = 1.0f;

    float circleRotation = 0.0f;
    float targetRotation = 0.0f;
    float circleGlowPhase = 0.0f;

    float confidenceSmooth = 0.0f;
    float tensionSmooth    = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiHarmonicHUDEditor)
};
