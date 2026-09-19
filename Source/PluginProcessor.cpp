#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
namespace
{
    constexpr float kMajorProfile[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f,
                                           2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    constexpr float kMinorProfile[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f,
                                           2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

    constexpr float kTensionWeights[12] = {
        0.00f, 1.00f, 0.70f, 0.15f, 0.15f, 0.35f,
        0.95f, 0.10f, 0.40f, 0.30f, 0.50f, 0.75f
    };

    // Comparar un set de intervalos contra un patrón de acorde
    bool matchPattern (const std::set<int>& iv, const std::vector<int>& pat)
    {
        if (iv.size() != pat.size()) return false;
        for (int i : pat)
            if (iv.count (i) == 0) return false;
        return true;
    }
}

//==============================================================================
//========================== BasicSynthVoice ==================================
//==============================================================================
BasicSynthVoice::BasicSynthVoice()
{
    setPreset (0);
}

bool BasicSynthVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<BasicSynthSound*> (sound) != nullptr;
}

void BasicSynthVoice::setCurrentPlaybackSampleRate (double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
    if (newRate > 0.0)
        sampleRate = newRate;
    updateEnvelopeIncrements();
}

void BasicSynthVoice::setPreset (int presetIndex)
{
    currentPreset = juce::jlimit (0, 2, presetIndex);

    switch (currentPreset)
    {
        case 0:  // Electric Piano
            preset = { 0.35f, 0.15f, 0.05f,
                       0.005f, 0.4f, 0.35f, 0.5f,
                       2500.0f, 0.7f, 0.4f };
            break;

        case 1:  // Warm Pad
            preset = { 0.5f, 0.25f, 0.15f,
                       0.4f, 0.6f, 0.7f, 1.2f,
                       1200.0f, 1.2f, 0.15f };
            break;

        case 2:  // Pluck
        default:
            preset = { 0.25f, 0.35f, 0.20f,
                       0.001f, 0.15f, 0.15f, 0.25f,
                       4000.0f, 0.5f, 0.7f };
            break;
    }

    updateEnvelopeIncrements();
}

void BasicSynthVoice::updateEnvelopeIncrements()
{
    if (sampleRate <= 0.0) return;

    envAttackInc = (preset.attackTime > 0.0001f)
        ? 1.0f / (float) (preset.attackTime * sampleRate)
        : 1.0f;

    auto expCoef = [] (float timeSec, double sr) -> float
    {
        if (timeSec <= 0.0001f) return 0.0f;
        return std::exp (-4.6f / (float) (timeSec * sr));
    };

    envDecayCoef   = expCoef (preset.decayTime,   sampleRate);
    envReleaseCoef = expCoef (preset.releaseTime, sampleRate);

    filterCutoffHz = preset.filterCutoff;
    filterQ = preset.filterQ;
}

void BasicSynthVoice::startNote (int midiNoteNumber, float velocityIn,
                                  juce::SynthesiserSound*, int)
{
    baseFreq = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    velocity = velocityIn;

    phase1 = phase2 = phase3 = phase5 = 0.0;

    envStage = EnvStage::Attack;
    envLevel = 0.0f;
    filterEnv = 1.0f;
    filterState = 0.0f;

    updateEnvelopeIncrements();
}

void BasicSynthVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        envStage = EnvStage::Release;
    }
    else
    {
        clearCurrentNote();
        envStage = EnvStage::Idle;
        envLevel = 0.0f;
    }
}

void BasicSynthVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                        int startSample, int numSamples)
{
    if (envStage == EnvStage::Idle)
        return;

    const double invSr = 1.0 / sampleRate;
    const double dPhase1 = baseFreq * invSr;
    const double dPhase2 = baseFreq * 2.0 * invSr;
    const double dPhase3 = baseFreq * 3.0 * invSr;
    const double dPhase5 = baseFreq * 5.0 * invSr;

    const float filterBase = filterCutoffHz;
    const float filterEnvAmt = preset.filterEnvAmount;

    while (--numSamples >= 0)
    {
        switch (envStage)
        {
            case EnvStage::Attack:
                envLevel += envAttackInc;
                if (envLevel >= 1.0f) { envLevel = 1.0f; envStage = EnvStage::Decay; }
                break;

            case EnvStage::Decay:
                envLevel *= envDecayCoef;
                if (envLevel <= preset.sustainLevel)
                {
                    envLevel = preset.sustainLevel;
                    envStage = EnvStage::Sustain;
                }
                break;

            case EnvStage::Sustain:
                envLevel = preset.sustainLevel;
                break;

            case EnvStage::Release:
                envLevel *= envReleaseCoef;
                if (envLevel <= 0.0001f)
                {
                    clearCurrentNote();
                    envStage = EnvStage::Idle;
                    return;
                }
                break;

            case EnvStage::Idle:
            default:
                break;
        }

        float sample = 0.0f;
        sample += (float) std::sin (phase1 * juce::MathConstants<double>::twoPi);
        sample += preset.harmonic2Gain * (float) std::sin (phase2 * juce::MathConstants<double>::twoPi);
        sample += preset.harmonic3Gain * (float) std::sin (phase3 * juce::MathConstants<double>::twoPi);
        sample += preset.harmonic5Gain * (float) std::sin (phase5 * juce::MathConstants<double>::twoPi);

        phase1 += dPhase1; if (phase1 >= 1.0) phase1 -= 1.0;
        phase2 += dPhase2; if (phase2 >= 1.0) phase2 -= 1.0;
        phase3 += dPhase3; if (phase3 >= 1.0) phase3 -= 1.0;
        phase5 += dPhase5; if (phase5 >= 1.0) phase5 -= 1.0;

        float cutoff = filterBase * (1.0f + filterEnvAmt * (filterEnv - 0.5f) * 2.0f);
        cutoff = juce::jlimit (80.0f, (float) (sampleRate * 0.45), cutoff);
        float alpha = juce::jlimit (0.001f, 0.99f,
                                     (float) (juce::MathConstants<double>::twoPi * cutoff * invSr));
        filterState += alpha * (sample - filterState);

        filterEnv *= 0.99995f;

        float out = filterState * envLevel * velocity * 0.22f;

        for (auto ch = outputBuffer.getNumChannels(); --ch >= 0;)
            outputBuffer.addSample (ch, startSample, out);

        ++startSample;
    }
}

//==============================================================================
//=================== MidiHarmonicHUDProcessor ================================
//==============================================================================
MidiHarmonicHUDProcessor::MidiHarmonicHUDProcessor()
    : AudioProcessor (BusesProperties()
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (int i = 0; i < 128; ++i)
        activeMidiNotes[i].store (false);

    for (size_t i = 0; i < pitchClassHistogram.size(); ++i)
        pitchClassHistogram[i].store (0);

    for (size_t i = 0; i < tensionHistory.size(); ++i)
        tensionHistory[i].store (0.0f);

    for (int i = 0; i < 8; ++i)
        synth.addVoice (new BasicSynthVoice());

    synth.addSound (new BasicSynthSound());

    // ⚠️ Sin addParameterListener: usamos polling en processBlock
}

MidiHarmonicHUDProcessor::~MidiHarmonicHUDProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
MidiHarmonicHUDProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::muteSynth, "Mute Piano", false));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::presetIndex, "Preset",
        juce::StringArray { "Electric Piano", "Warm Pad", "Pluck" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::themeIndex, "Theme",
        juce::StringArray { "Tokyo Night", "Dracula", "Nord", "Gruvbox", "One Dark" }, 0));

    return { params.begin(), params.end() };
}

//==============================================================================
void MidiHarmonicHUDProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    keyboardState.reset();
}

void MidiHarmonicHUDProcessor::releaseResources()
{
    keyboardState.reset();
}

bool MidiHarmonicHUDProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
void MidiHarmonicHUDProcessor::decayHistogramIfNeeded()
{
    auto now = juce::Time::getMillisecondCounter();
    auto last = lastDecayMs.load();

    if (now - last < 500) return;
    lastDecayMs.store (now);

    for (size_t i = 0; i < pitchClassHistogram.size(); ++i)
    {
        int v = pitchClassHistogram[i].load();
        if (v > 0)
            pitchClassHistogram[i].store ((int) ((float) v * 0.95f));
    }
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

    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            int n = message.getNoteNumber();
            if (juce::isPositiveAndBelow (n, 128))
            {
                activeMidiNotes[n].store (true);

                int pc = n % 12;
                pitchClassHistogram[static_cast<size_t> (pc)].fetch_add (1);
                totalNotesSeen.fetch_add (1);
            }
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

    decayHistogramIfNeeded();
    detectChordFromActiveNotes();

    bool muteParam = apvts.getRawParameterValue (ParamIDs::muteSynth)->load() > 0.5f;
    muteSynth.store (muteParam);

    int preset = (int) apvts.getRawParameterValue (ParamIDs::presetIndex)->load();
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* v = dynamic_cast<BasicSynthVoice*> (synth.getVoice (i)))
            v->setPreset (preset);
    }

    if (!muteSynth.load())
    {
        synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
    }
    else
    {
        buffer.clear();
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
    auto key = detectKey();

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

    {
        const juce::ScopedLock sl (currentKeyLock);
        currentKeyText = key.name;
    }

    chordConfidence.store (analysis.confidence);
    harmonicTension.store (analysis.tension);
    currentRootPC.store (analysis.rootPitchClass);
    currentBassPC.store (analysis.bassPitchClass);
    currentInversion.store (analysis.inversion);

    detectedKeyTonic.store (key.tonicPitchClass);
    detectedKeyIsMinor.store (key.isMinor);
    detectedKeyConfidence.store (key.confidence);

    int pos = tensionHistoryWritePos.load();
    tensionHistory[static_cast<size_t> (pos)].store (analysis.tension);
    tensionHistoryWritePos.store ((pos + 1) % TENSION_HISTORY_SIZE);
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
ChordAnalysis MidiHarmonicHUDProcessor::analyzeChord (const std::vector<int>& notes)
{
    ChordAnalysis result;
    if (notes.empty()) return result;

    std::vector<int> sorted = notes;
    std::sort (sorted.begin(), sorted.end());

    int bassMidiNote = sorted[0];
    result.bassPitchClass = bassMidiNote % 12;

    std::set<int> pcSet;
    for (int n : sorted) pcSet.insert (n % 12);

    int root = bassMidiNote % 12;
    result.rootPitchClass = root;

    std::set<int> intervals;
    for (int pc : pcSet)
        intervals.insert ((pc - root + 12) % 12);

    // --- Tensión ---
    float tensionSum = 0.0f;
    int   tensionCount = 0;
    for (int i : intervals)
    {
        if (i == 0) continue;
        tensionSum += kTensionWeights[i];
        ++tensionCount;
    }
    result.tension = (tensionCount > 0)
        ? juce::jlimit (0.0f, 1.0f, (tensionSum / (float) tensionCount) * 1.4f)
        : 0.0f;

    if (notes.size() == 1)
    {
        result.name = noteName (sorted[0]);
        result.baseName = result.name;
        result.confidence = 0.45f;
        return result;
    }

    // --- Patrones ---
    struct ChordPattern
    {
        std::vector<int> intervals;
        const char* suffix;
    };

    static const std::vector<ChordPattern> patterns = {
        {{0, 4, 7},        "maj"},
        {{0, 3, 7},        "min"},
        {{0, 3, 6},        "dim"},
        {{0, 4, 8},        "aug"},
        {{0, 2, 7},        "sus2"},
        {{0, 5, 7},        "sus4"},
        {{0, 4, 7, 11},    "maj7"},
        {{0, 4, 7, 10},    "7"},
        {{0, 3, 7, 10},    "m7"},
        {{0, 3, 7, 11},    "mMaj7"},
        {{0, 3, 6, 10},    "m7b5"},
        {{0, 3, 6, 9},     "dim7"},
        {{0, 4, 8, 11},    "aug7"},
        {{0, 4, 7, 9},     "6"},
        {{0, 3, 7, 9},     "m6"},
        {{0, 4, 7, 10, 2}, "9"},
        {{0, 3, 7, 10, 2}, "m9"},
        {{0, 4, 7, 11, 2}, "maj9"},
        {{0, 5, 7, 10},    "7sus4"},
        {{0, 2, 7, 10},    "7sus2"}
    };

    int bestRoot = -1;
    const ChordPattern* bestPattern = nullptr;

    for (int tryRoot : pcSet)
    {
        std::set<int> tryIntervals;
        for (int pc : pcSet)
            tryIntervals.insert ((pc - tryRoot + 12) % 12);

        for (const auto& p : patterns)
        {
            if (matchPattern (tryIntervals, p.intervals))
            {
                if (bestRoot < 0 || tryRoot == root)
                {
                    bestRoot = tryRoot;
                    bestPattern = &p;

                    if (tryRoot == root) break;
                }
            }
        }

        if (bestRoot == root && bestPattern != nullptr) break;
    }

    if (bestPattern != nullptr)
    {
        result.rootPitchClass = bestRoot;
        result.baseName = pitchClassName (bestRoot) + bestPattern->suffix;

        int bassInterval = (result.bassPitchClass - bestRoot + 12) % 12;

        if (bassInterval == 0)
        {
            result.inversion = 0;
            result.name = result.baseName;
        }
        else
        {
            if (bassInterval == 3 || bassInterval == 4)
                result.inversion = 1;
            else if (bassInterval == 7)
                result.inversion = 2;
            else
                result.inversion = 3;

            result.inversionText = "/" + pitchClassName (result.bassPitchClass);
            result.name = result.baseName + result.inversionText;
        }

        result.confidence = 1.0f;
        return result;
    }

    juce::String fallback;
    for (size_t i = 0; i < sorted.size(); ++i)
    {
        if (i > 0) fallback += " ";
        fallback += noteName (sorted[i]);
    }
    result.name = fallback;
    result.baseName = fallback;
    result.confidence = 0.35f;
    return result;
}

//==============================================================================
KeyDetection MidiHarmonicHUDProcessor::detectKey()
{
    KeyDetection result;

    if (totalNotesSeen.load() < 3)
        return result;

    std::array<float, 12> obs {};
    float total = 0.0f;
    for (size_t i = 0; i < obs.size(); ++i)
    {
        obs[i] = (float) pitchClassHistogram[i].load();
        total += obs[i];
    }

    if (total < 1.0f) return result;

    for (size_t i = 0; i < obs.size(); ++i)
        obs[i] /= total;

    auto correlate = [] (const std::array<float, 12>& a, const float* b) -> float
    {
        float meanA = 0.0f, meanB = 0.0f;
        for (size_t i = 0; i < a.size(); ++i)
        {
            meanA += a[i];
            meanB += b[i];
        }
        meanA /= 12.0f; meanB /= 12.0f;

        float num = 0.0f, denA = 0.0f, denB = 0.0f;
        for (size_t i = 0; i < a.size(); ++i)
        {
            float da = a[i] - meanA;
            float db = b[i] - meanB;
            num  += da * db;
            denA += da * da;
            denB += db * db;
        }

        float den = std::sqrt (denA * denB);
        return (den > 0.0001f) ? (num / den) : 0.0f;
    };

    float bestScore = -2.0f;
    int bestTonic = 0;
    bool bestIsMinor = false;

    for (int tonic = 0; tonic < 12; ++tonic)
    {
        std::array<float, 12> majorRot {}, minorRot {};
        for (size_t i = 0; i < 12; ++i)
        {
            int idx = ((int) i - tonic + 12) % 12;
            majorRot[i] = kMajorProfile[idx];
            minorRot[i] = kMinorProfile[idx];
        }

        float majorScore = correlate (obs, majorRot.data());
        float minorScore = correlate (obs, minorRot.data());

        if (majorScore > bestScore) { bestScore = majorScore; bestTonic = tonic; bestIsMinor = false; }
        if (minorScore > bestScore) { bestScore = minorScore; bestTonic = tonic; bestIsMinor = true; }
    }

    result.tonicPitchClass = bestTonic;
    result.isMinor = bestIsMinor;
    result.name = pitchClassName (bestTonic) + (bestIsMinor ? " Minor" : " Major");
    result.confidence = juce::jlimit (0.0f, 1.0f, (bestScore + 1.0f) * 0.5f);

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

//==============================================================================
void MidiHarmonicHUDProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void MidiHarmonicHUDProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MidiHarmonicHUDProcessor();
}
