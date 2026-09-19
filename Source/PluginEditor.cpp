#include "PluginEditor.h"

//==============================================================================
MidiHarmonicHUDEditor::MidiHarmonicHUDEditor (MidiHarmonicHUDProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setSize (500, 400);

    // Configurar el botón Mute
    muteButton.setColour (juce::ToggleButton::textColourId, textColour);
    muteButton.setColour (juce::ToggleButton::tickColourId, accentColour);
    muteButton.setToggleState (processorRef.muteSynth.load(), juce::dontSendNotification);
    muteButton.onClick = [this]()
    {
        processorRef.muteSynth.store (muteButton.getToggleState());
    };
    addAndMakeVisible (muteButton);

    // Timer para actualizar la UI
    startTimerHz (30);
}

MidiHarmonicHUDEditor::~MidiHarmonicHUDEditor()
{
    stopTimer();
}

//==============================================================================
void MidiHarmonicHUDEditor::paint (juce::Graphics& g)
{
    // Fondo oscuro
    g.fillAll (bgColour);

    // Título
    g.setColour (accentColour);
    g.setFont (juce::Font (20.0f, juce::Font::bold));
    g.drawText ("MIDI HARMONIC HUD", getLocalBounds().removeFromTop (40),
                juce::Justification::centred, false);

    // ---- Sección Detecting ----
    auto detectingArea = getLocalBounds().removeFromTop (140).withTrimmedTop (45).reduced (15, 5);
    g.setColour (panelColour);
    g.fillRoundedRectangle (detectingArea.toFloat(), 8.0f);

    g.setColour (dimTextColour);
    g.setFont (12.0f);
    g.drawText ("DETECTING", detectingArea.getX() + 10, detectingArea.getY() + 5,
                100, 20, juce::Justification::topLeft, false);

    g.setColour (textColour);
    g.setFont (juce::Font (36.0f, juce::Font::bold));
    g.drawText (cachedChord, detectingArea, juce::Justification::centred, false);

    // ---- Sección Historial ----
    auto historyArea = getLocalBounds().removeFromTop (240).withTrimmedTop (145).reduced (15, 5);
    g.setColour (panelColour);
    g.fillRoundedRectangle (historyArea.toFloat(), 8.0f);

    g.setColour (dimTextColour);
    g.setFont (12.0f);
    g.drawText ("HISTORY (last 4)", historyArea.getX() + 10, historyArea.getY() + 5,
                200, 20, juce::Justification::topLeft, false);

    g.setFont (16.0f);
    for (int i = 0; i < cachedHistory.size(); ++i)
    {
        g.setColour (i == 0 ? accentColour : textColour.withAlpha (0.7f));
        g.drawText (cachedHistory[i],
                    historyArea.getX() + 10,
                    historyArea.getY() + 30 + i * 24,
                    historyArea.getWidth() - 20,
                    22,
                    juce::Justification::centredLeft, false);
    }

    // ---- Sección Diatónicos ----
    auto diatonicArea = getLocalBounds().removeFromTop (340).withTrimmedTop (245).reduced (15, 5);
    g.setColour (panelColour);
    g.fillRoundedRectangle (diatonicArea.toFloat(), 8.0f);

    g.setColour (dimTextColour);
    g.setFont (12.0f);
    g.drawText ("DIATONIC INFO", diatonicArea.getX() + 10, diatonicArea.getY() + 5,
                200, 20, juce::Justification::topLeft, false);

    g.setColour (textColour);
    g.setFont (14.0f);
    g.drawText (juce::String ("Active notes: ") + juce::String (cachedActiveNoteCount),
                diatonicArea.getX() + 10, diatonicArea.getY() + 30,
                diatonicArea.getWidth() - 20, 22,
                juce::Justification::centredLeft, false);

    // Mostrar la escala sugerida (simplificada: mayor de la nota más grave)
    juce::String scaleInfo = "Scale: --";
    if (cachedActiveNoteCount > 0 && cachedChord != "---")
    {
        // Extraer la raíz del acorde (primera letra + posible #)
        juce::String root = cachedChord.substring (0, 1);
        if (cachedChord.length() > 1 && cachedChord[1] == '#')
            root += "#";
        scaleInfo = "Scale: " + root + " Major / Relative Minor";
    }
    g.drawText (scaleInfo,
                diatonicArea.getX() + 10, diatonicArea.getY() + 55,
                diatonicArea.getWidth() - 20, 22,
                juce::Justification::centredLeft, false);

    // Línea de separación inferior
    g.setColour (accentColour.withAlpha (0.3f));
    g.drawLine (0, getHeight() - 1, getWidth(), getHeight() - 1, 1.0f);
}

//==============================================================================
void MidiHarmonicHUDEditor::resized()
{
    // Botón Mute en la esquina superior derecha
    muteButton.setBounds (getWidth() - 110, 10, 100, 30);
}

//==============================================================================
void MidiHarmonicHUDEditor::timerCallback()
{
    // Actualizar el acorde actual
    {
        const juce::ScopedLock sl (processorRef.currentChordLock);
        cachedChord = processorRef.currentChord;
    }

    // Actualizar el historial
    {
        const juce::ScopedLock sl (processorRef.chordHistoryLock);
        cachedHistory = processorRef.chordHistory;
    }

    // Contar notas activas
    int count = 0;
    for (int i = 0; i < 128; ++i)
    {
        if (processorRef.activeMidiNotes[i].load())
            ++count;
    }
    cachedActiveNoteCount = count;

    // Sincronizar el estado del botón Mute por si acaso
    if (muteButton.getToggleState() != processorRef.muteSynth.load())
        muteButton.setToggleState (processorRef.muteSynth.load(), juce::dontSendNotification);

    repaint();
}
