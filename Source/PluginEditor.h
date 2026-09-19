#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
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

    MidiHarmonicHUDProcessor& processorRef;

    // Componentes de la UI
    juce::ToggleButton muteButton { "Mute Piano" };

    // Fuentes y colores
    juce::Colour bgColour      { 0xff1e1e2e };
    juce::Colour panelColour   { 0xff2a2a3e };
    juce::Colour accentColour  { 0xff7aa2f7 };
    juce::Colour textColour    { 0xffe0e0e0 };
    juce::Colour dimTextColour { 0xff8888aa };

    // Datos cacheados para el paint
    juce::String cachedChord;
    juce::StringArray cachedHistory;
    int cachedActiveNoteCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiHarmonicHUDEditor)
};
