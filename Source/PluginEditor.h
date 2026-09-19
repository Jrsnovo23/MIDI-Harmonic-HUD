#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "PluginProcessor.h"

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
    void drawConfidenceMeter (juce::Graphics&, juce::Rectangle<int>);
    void drawTensionGradient (juce::Graphics&, juce::Rectangle<int>);
    void drawGlowText (juce::Graphics&, const juce::String&, juce::Rectangle<int>,
                       juce::Colour, float fontSize, float alpha);

    MidiHarmonicHUDProcessor& processorRef;
    juce::ToggleButton muteButton { "Mute Piano" };

    // ===== Paleta =====
    juce::Colour bgColour      { 0xff0f0f19 };
    juce::Colour bgColour2     { 0xff181828 };
    juce::Colour panelColour   { 0xff1e1e30 };
    juce::Colour panelStroke   { 0xff2a2a45 };
    juce::Colour accentColour  { 0xff7aa2f7 };
    juce::Colour accent2Colour { 0xffbb9af7 };
    juce::Colour accent3Colour { 0xff9ece6a };
    juce::Colour dangerColour  { 0xfff7768e };
    juce::Colour textColour    { 0xffe6e6f0 };
    juce::Colour dimTextColour { 0xff6a6a88 };

    // ===== Estado cacheado =====
    juce::String cachedChord { "---" };
    juce::StringArray cachedHistory;
    std::array<bool, 128> cachedActiveNotes {};
    float cachedConfidence = 0.0f;
    float cachedTension    = 0.0f;
    int   cachedRootPC     = -1;
    int   cachedActiveCount = 0;

    // ===== Animaciones =====
    juce::String lastDisplayedChord;
    juce::uint32 chordChangeTimeMs = 0;
    float chordFadeAlpha = 1.0f;

    float circleRotation = 0.0f;         // radianes actuales
    float targetRotation = 0.0f;
    float circleGlowPhase = 0.0f;        // para pulso

    float confidenceSmooth = 0.0f;       // interpolación suave
    float tensionSmooth    = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiHarmonicHUDEditor)
};
