#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MidiHarmonicHUDProcessor::MidiHarmonicHUDProcessor()
    : AudioProcessor (BusesProperties()
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    for (int i = 0; i < 128; ++i)
        activeMidiNotes[i].store (false);

    for (int i = 0; i < 8; ++i)
        synth.addVoice (new BasicSynthVoice());

    synth.addSound (new BasicSynthSound());
}

MidiHarmonicHUDProcessor::~MidiHarmonicHUDProcessor() = default;

//==============================================================================
void MidiHarmonicHUDProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    keyboardState.reset();
}

void MidiHarmonicHUDProcessor::releaseResources() { keyboardState.reset(); }

bool MidiHarmonicHUDProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
void MidiHarmonicHUDProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Capturar eventos MIDI
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            int n = message.getNoteNumber();
            if (juce::isPositiveAndBelow (n, 128))
                activeMidiNotes[n].store (true);
        }
        else if (message.isNoteOff())
        {
            int n = message.getNoteNumber();
            if (juce::isPositiveAndBelow (n, 128))
                activeMidiNotes[n].store (false);
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            for (int i = 0; i < 128; ++i)
                activeMidiNotes[i].store (false);
        }
    }

    // Análisis armónico (siempre activo)
    detectChordFromActiveNotes();

    // Render de audio
    if (!muteSynth.load())
    {
        synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
    }
    else
    {
        buffer.clear();
        // Procesar MIDI en buffer temporal para mantener el synth sincronizado
        juce::AudioBuffer<float> tempBuffer (buffer.getNumChannels(), buffer.getNumSamples());
        tempBuffer.clear();
        synth.renderNextBlock (tempBuffer, midiMessages, 0, tempBuffer.getNumSamples());
    }
}

//==============================================================================
void MidiHarmonicHUDProcessor::detectChordFromActiveNotes()
{
    std::vector<int> notes;
    notes.reserve (16);
    for (int i = 0; i < 128; ++i)
        if (activeMidiNotes[i].load())
            notes.push_back (i);

    auto analysis = analyzeChord (notes);

    {
        const juce::ScopedLock sl (currentChordLock);
        if (currentChord != analysis.name)
        {
            currentChord = analysis.name;
            if (analysis.name != "---")
            {
                const juce::ScopedLock histLock (chordHistoryLock);
                chordHistory.insert (0, analysis.name);
                if (chordHistory.size() > 4)
                    chordHistory.remove (chordHistory.size() - 1);
            }
        }
    }

    chordConfidence.store (analysis.confidence);
    harmonicTension.store (analysis.tension);
    currentRootPC.store (analysis.rootPitchClass);
}

//==============================================================================
juce::String MidiHarmonicHUDProcessor::pitchClassName (int pc)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F",
                                   "F#", "G", "G#", "A", "A#", "B" };
    return names[((pc % 12) + 12) % 12];
}

juce::String MidiHarmonicHUDProcessor::noteName (int midiNote)
{
    int octave = (midiNote / 12) - 1;
    return pitchClassName (midiNote) + juce::String (octave);
}

//==============================================================================
MidiHarmonicHUDProcessor::ChordAnalysis
MidiHarmonicHUDProcessor::analyzeChord (const std::vector<int>& notes)
{
    ChordAnalysis result;

    if (notes.empty())
        return result;

    std::vector<int> sorted = notes;
    std::sort (sorted.begin(), sorted.end());

    std::set<int> pcSet;
    for (int n : sorted) pcSet.insert (n % 12);

    int root = sorted[0] % 12;
    result.rootPitchClass = root;

    std::set<int> intervals;
    for (int pc : pcSet)
        intervals.insert ((pc - root + 12) % 12);

    // === Cálculo de tensión armónica ===
    // Pesos de disonancia por intervalo (0 = unísono, 6 = tritono)
    static const float tensionWeights[12] = {
        0.00f, // 0 unísono
        1.00f, // 1 m2
        0.70f, // 2 M2
        0.15f, // 3 m3
        0.15f, // 4 M3
        0.35f, // 5 P4
        0.95f, // 6 tritono
        0.10f, // 7 P5
        0.40f, // 8 m6
        0.30f, // 9 M6
        0.50f, // 10 m7
        0.75f  // 11 M7
    };

    float tensionSum = 0.0f;
    int   tensionCount = 0;
    for (int i : intervals)
    {
        if (i == 0) continue;
        tensionSum += tensionWeights[i];
        ++tensionCount;
    }
    result.tension = (tensionCount > 0)
        ? juce::jlimit (0.0f, 1.0f, (tensionSum / tensionCount) * 1.4f)
        : 0.0f;

    // === Nota única ===
    if (notes.size() == 1)
    {
        result.name = noteName (sorted[0]);
        result.confidence = 0.45f;
        return result;
    }

    // === Emparejamiento con patrones ===
    auto matches = [&] (std::initializer_list<int> pat)
    {
        return intervals == std::set<int> (pat.begin(), pat.end());
    };

    auto rootStr = [&]() { return pitchClassName (root); };

    struct Match { bool hit; const char* suffix; };
    auto tryMatch = [&] (std::initializer_list<int> pat, const char* suffix) -> bool
    {
        if (matches (pat))
        {
            result.name = rootStr() + suffix;
            result.confidence = 1.0f;
            return true;
        }
        return false;
    };

    // Tríadas
    if (tryMatch ({0, 4, 7}, "maj")) return result;
    if (tryMatch ({0, 3, 7}, "min")) return result;
    if (tryMatch ({0, 3, 6}, "dim")) return result;
    if (tryMatch ({0, 4, 8}, "aug")) return result;
    if (tryMatch ({0, 2, 7}, "sus2")) return result;
    if (tryMatch ({0, 5, 7}, "sus4")) return result;

    // Séptimas
    if (tryMatch ({0, 4, 7, 11}, "maj7")) return result;
    if (tryMatch ({0, 4, 7, 10}, "7"))    return result;
    if (tryMatch ({0, 3, 7, 10}, "m7"))   return result;
    if (tryMatch ({0, 3, 7, 11}, "mMaj7"))return result;
    if (tryMatch ({0, 3, 6, 10}, "m7b5")) return result;
    if (tryMatch ({0, 3, 6, 9},  "dim7")) return result;
    if (tryMatch ({0, 4, 8, 11}, "aug7")) return result;

    // Sextas
    if (tryMatch ({0, 4, 7, 9}, "6"))  return result;
    if (tryMatch ({0, 3, 7, 9}, "m6")) return result;

    // Novenas
    if (tryMatch ({0, 4, 7, 10, 2}, "9"))    return result;
    if (tryMatch ({0, 3, 7, 10, 2}, "m9"))   return result;
    if (tryMatch ({0, 4, 7, 11, 2}, "maj9")) return result;

    // Séptimas suspendidas
    if (tryMatch ({0, 5, 7, 10}, "7sus4")) return result;
    if (tryMatch ({0, 2, 7, 10}, "7sus2")) return result;

    // No coincide: mostrar notas individuales
    juce::String fallback;
    for (size_t i = 0; i < sorted.size(); ++i)
    {
        if (i > 0) fallback += " ";
        fallback += noteName (sorted[i]);
    }
    result.name = fallback;
    result.confidence = 0.35f;
    return result;
}

//==============================================================================
juce::AudioProcessorEditor* MidiHarmonicHUDProcessor::createEditor()
{
    return new MidiHarmonicHUDEditor (*this);
}

bool MidiHarmonicHUDProcessor::hasEditor() const { return true; }

const juce::String MidiHarmonicHUDProcessor::getName() const { return "MIDI Harmonic HUD"; }
bool MidiHarmonicHUDProcessor::acceptsMidi() const { return true; }
bool MidiHarmonicHUDProcessor::producesMidi() const { return false; }
bool MidiHarmonicHUDProcessor::isMidiEffect() const { return false; }
double MidiHarmonicHUDProcessor::getTailLengthSeconds() const { return 2.0; }

int MidiHarmonicHUDProcessor::getNumPrograms() { return 1; }
int MidiHarmonicHUDProcessor::getCurrentProgram() { return 0; }
void MidiHarmonicHUDProcessor::setCurrentProgram (int) {}
const juce::String MidiHarmonicHUDProcessor::getProgramName (int) { return {}; }
void MidiHarmonicHUDProcessor::changeProgramName (int, const juce::String&) {}

void MidiHarmonicHUDProcessor::getStateInformation (juce::MemoryBlock&) {}
void MidiHarmonicHUDProcessor::setStateInformation (const void*, int) {}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MidiHarmonicHUDProcessor();
}
