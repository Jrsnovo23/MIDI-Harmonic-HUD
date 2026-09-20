//==============================================================================
void MidiHarmonicHUDEditor::timerCallback()
{
    // Captura de estado
    {
        const juce::ScopedLock sl (processorRef.currentChordLock);
        cachedChord = processorRef.currentChord;
    }
    {
        const juce::ScopedLock sl (processorRef.chordHistoryLock);
        cachedHistory = processorRef.chordHistory;
    }
    {
        const juce::ScopedLock sl (processorRef.currentKeyLock);
        cachedKeyText = processorRef.currentKeyText;
    }

    int count = 0;
    for (int i = 0; i < 128; ++i)
    {
        bool on = processorRef.activeMidiNotes[i].load();
        cachedActiveNotes[static_cast<size_t> (i)] = on;
        if (on) ++count;
    }
    cachedActiveCount = count;

    cachedConfidence = processorRef.chordConfidence.load();
    cachedTension    = processorRef.harmonicTension.load();
    cachedRootPC     = processorRef.currentRootPC.load();
    cachedBassPC     = processorRef.currentBassPC.load();
    cachedInversion  = processorRef.currentInversion.load();
    cachedKeyConfidence = processorRef.detectedKeyConfidence.load();

    // Copiar buffer circular de tensión
    for (size_t i = 0; i < MidiHarmonicHUDProcessor::TENSION_HISTORY_SIZE; ++i)
        cachedTensionHistory[i] = processorRef.tensionHistory[i].load();

    // ⚠️ Leer parámetros con protección contra nullptr
    if (auto* param = processorRef.apvts.getRawParameterValue (ParamIDs::themeIndex))
        cachedThemeId = (int) param->load();

    // Animación: fade del acorde
    if (cachedChord != lastDisplayedChord)
    {
        lastDisplayedChord = cachedChord;
        chordChangeTimeMs = juce::Time::getMillisecondCounter();
    }
    auto elapsed = juce::Time::getMillisecondCounter() - chordChangeTimeMs;
    chordFadeAlpha = juce::jmin (1.0f, (float) elapsed / 350.0f);

    confidenceSmooth += (cachedConfidence - confidenceSmooth) * 0.18f;
    tensionSmooth    += (cachedTension    - tensionSmooth)    * 0.18f;

    if (cachedRootPC >= 0)
    {
        int rootIndex = 0;
        for (int i = 0; i < 12; ++i)
            if (kFifthOrderPC[i] == cachedRootPC) { rootIndex = i; break; }

        targetRotation = -juce::MathConstants<float>::twoPi
                       * ((float) rootIndex / 12.0f);
    }

    float diff = targetRotation - circleRotation;
    while (diff >  juce::MathConstants<float>::pi) diff -= juce::MathConstants<float>::twoPi;
    while (diff < -juce::MathConstants<float>::pi) diff += juce::MathConstants<float>::twoPi;
    circleRotation += diff * 0.12f;

    circleGlowPhase += 0.08f;
    if (circleGlowPhase > juce::MathConstants<float>::twoPi)
        circleGlowPhase -= juce::MathConstants<float>::twoPi;

    repaint();
}
