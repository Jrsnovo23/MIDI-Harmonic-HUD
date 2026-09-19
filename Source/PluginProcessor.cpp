#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Perfiles de Krumhansl-Kessler para detección de tonalidad
namespace
{
    constexpr float kMajorProfile[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f,
                                           2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    constexpr float kMinorProfile[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f,
                                           2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

    // Pesos de disonancia por intervalo (desde la raíz)
    constexpr float kTensionWeights[12] = {
        0.00f, 1.00f, 0.70f, 0.15f, 0.15f, 0.35f,
        0.95f, 0.10f, 0.40f, 0.30f, 0.50f, 0.75f
    };
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
        case 0:  // Electric Piano (default)
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

    // Coeficiente exponencial para decay y release
    auto expCoef = [] (float timeSec, double sr) -> float
    {
        if (timeSec <= 0.0001f) return 0.0f;
        // Factor para que llegue a ~1% del valor inicial en "timeSec"
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

    // Resetear fases con pequeño offset para que no suenen idénticas
    phase1 = phase2 = phase3 = phase5 = 0.0;

    // Envolvente
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

    // Filtro paso-bajo de un polo (simplificado)
    const float filterBase = filterCutoffHz;
    const float filterEnvAmt = preset.filterEnvAmount;

    while (--numSamples >= 0)
    {
        // --- Envolvente ---
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

        // --- Osciladores ---
        float sample = 0.0f;
        sample += (float) std::sin (phase1 * juce::MathConstants<double>::twoPi);
        sample += preset.harmonic2Gain * (float) std::sin (phase2 * juce::MathConstants<double>::twoPi);
        sample += preset.harmonic3Gain * (float) std::sin (phase3 * juce::MathConstants<double>::twoPi);
        sample += preset.harmonic5Gain * (float) std::sin (phase5 * juce::MathConstants<double>::twoPi);

        phase1 += dPhase1; if (phase1 >= 1.0) phase1 -= 1.0;
        phase2 += dPhase2; if (phase2 >= 1.0) phase2 -= 1.0;
        phase3 += dPhase3; if (phase3 >= 1.0) phase3 -= 1.0;
        phase5 += dPhase5; if (phase5 >= 1.0) phase5 -= 1.0;

        // --- Filtro (un polo, cutoff modulado por env de filtro) ---
        float cutoff = filterBase * (1.0f + filterEnvAmt * (filterEnv - 0.5f) * 2.0f);
        cutoff = juce::jlimit (80.0f, (float) (sampleRate * 0.45), cutoff);
        float alpha = juce::jlimit (0.001f, 0.99f,
                                     (float) (juce::MathConstants<double>::twoPi * cutoff * invSr));
        filterState += alpha * (sample - filterState);

        // Decaimiento del env de filtro
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

    for (int i = 0; i < 12; ++i)
        pitchClassHistogram[i].store (0);

    for (int i = 0; i < TENSION_HISTORY_SIZE; ++i)
        tensionHistory[i].store (0.0f);

    for (int i = 0; i < 8; ++i)
        synth.addVoice (new BasicSynthVoice());

    synth.addSound (new BasicSynthSound());

    // Configurar listeners de parámetros
    apvts.addParameterListener (ParamIDs::muteSynth, this);
    apvts.addParameterListener (ParamIDs::presetIndex, this);
}

MidiHarmonicHUDProcessor::~MidiHarmonicHUDProcessor()
{
    apvts.removeParameterListener (ParamIDs::muteSynth, this);
    apvts.removeParameterListener (ParamIDs::presetIndex, this);
}

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

    if (now - last < 500) return;  // Decaer cada 500ms
    lastDecayMs.store (now);

    // Reducir el histograma en un 5% para "olvidar" notas viejas lentamente
    for (int i = 0; i < 12; ++i)
    {
        int v = pitchClassHistogram[i].load();
        if (v > 0)
            pitchClassHistogram[i].store ((int) (v * 0.95f));
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

    // Capturar eventos MIDI
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            int n = message.getNoteNumber();
            if (juce::isPositiveAndBelow (n, 128))
            {
                activeMidiNotes[n].store (true);

                // Actualizar histograma de pitch classes
                int pc = n % 12;
                pitchClassHistogram[pc].fetch_add (1);
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

    // Análisis armónico
    decayHistogramIfNeeded();
    detectChordFromActiveNotes();

    // Sincronizar mute con APVTS
    bool muteParam = apvts.getRawParameterValue (ParamIDs::muteSynth)->load() > 0.5f;
    muteSynth.store (muteParam);

    // Sincronizar preset con las voces
    int preset = (int) apvts.getRawParameterValue (ParamIDs::presetIndex)->load();
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* v = dynamic_cast<BasicSynthVoice*> (synth.getVoice (i)))
            v->setPreset (preset);
    }

    // Render de audio
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

    // Escribir en el buffer circular de tensión
    int pos = tensionHistoryWritePos.load();
    tensionHistory[pos].store (analysis.tension);
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

    // Nota más grave = bajo (para inversiones)
    int bassMidiNote = sorted[0];
    result.bassPitchClass = bassMidiNote % 12;

    std::set<int> pcSet;
    for (int n : sorted) pcSet.insert (n % 12);

    int root = bassMidiNote % 12;
    result.rootPitchClass = root;

    // Calcular intervalos respecto a la nota más grave
    std::set<int> intervals;
    for (int pc : pcSet)
        intervals.insert ((pc - root + 12) % 12);

    // --- Tensión armónica ---
    float tensionSum = 0.0f;
    int   tensionCount = 0;
    for (int i : intervals)
    {
        if (i == 0) continue;
        tensionSum += kTensionWeights[i];
        ++tensionCount;
    }
    result.tension = (tensionCount > 0)
        ? juce::jlimit (0.0f, 1.0f, (tensionSum / tensionCount) * 1.4f)
        : 0.0f;

    // --- Nota única ---
    if (notes.size() == 1)
    {
        result.name = noteName (sorted[0]);
        result.baseName = result.name;
        result.confidence = 0.45f;
        return result;
    }

    // --- Detección de acorde (permitiendo inversiones) ---
    // Buscamos la raíz que mejor se ajuste al conjunto de pitch classes
    auto matchesPattern = [] (const std::set<int>& iv, std::initializer_list<int> pat)
    {
        return iv == std::set<int> (pat.begin(), pat.end());
    };

    // Estructura de patrón: (intervalos, sufijo)
    struct ChordPattern { std::vector<int> intervals; const char* suffix; };
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

    // Probar cada nota del conjunto como posible raíz
    int bestRoot = -1;
    const ChordPattern* bestPattern = nullptr;

    for (int tryRoot : pcSet)
    {
        std::set<int> tryIntervals;
        for (int pc : pcSet)
            tryIntervals.insert ((pc - tryRoot + 12) % 12);

        for (const auto& p : patterns)
        {
            if (matchesPattern (tryIntervals,
                                 std::initializer_list<int> (p.intervals.begin(),
                                                              p.intervals.end())))
            {
                // Preferir la raíz que sea la nota más grave (evita falsos positivos)
                if (bestRoot < 0 || tryRoot == root)
                {
                    bestRoot = tryRoot;
                    bestPattern = &p;

                    if (tryRoot == root) break;  // Perfecto match: paramos
                }
            }
        }

        if (bestRoot == root && bestPattern != nullptr) break;
    }

    if (bestPattern != nullptr)
    {
        result.rootPitchClass = bestRoot;
        result.baseName = pitchClassName (bestRoot) + bestPattern->suffix;

        // --- Detección de inversión ---
        // Calcular la posición del bajo respecto a la raíz en el acorde
        int bassInterval = (result.bassPitchClass - bestRoot + 12) % 12;

        if (bassInterval == 0)
        {
            result.inversion = 0;
            result.name = result.baseName;
        }
        else
        {
            // Determinar qué grado del acorde es el bajo
            // 4 = tercera mayor, 3 = tercera menor, 7 = quinta, etc.
            if (bassInterval == 3 || bassInterval == 4)
                result.inversion = 1;   // Primera inversión
            else if (bassInterval == 7)
                result.inversion = 2;   // Segunda inversión
            else
                result.inversion = 3;   // Otra (séptima, etc.)

            result.inversionText = "/" + pitchClassName (result.bassPitchClass);
            result.name = result.baseName + result.inversionText;
        }

        result.confidence = 1.0f;
        return result;
    }

    // --- Fallback: no coincide ---
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

    // Si no hay suficientes notas, devolvemos "---"
    if (totalNotesSeen.load() < 3)
        return result;

    // Construir el vector de observaciones normalizado
    std::array<float, 12> obs {};
    float total = 0.0f;
    for (int i = 0; i < 12; ++i)
    {
        obs[i] = (float) pitchClassHistogram[i].load();
        total += obs[i];
    }

    if (total < 1.0f) return result;

    for (int i = 0; i < 12; ++i)
        obs[i] /= total;

    // Correlación de Pearson con cada perfil
    auto correlate = [] (const std::array<float, 12>& a, const float* b) -> float
    {
        float meanA = 0.0f, meanB = 0.0f;
        for (int i = 0; i < 12; ++i) { meanA += a[i]; meanB += b[i]; }
        meanA /= 12.0f; meanB /= 12.0f;

        float num = 0.0f, denA = 0.0f, denB = 0.0f;
        for (int i = 0; i < 12; ++i)
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

    // Probar 12 tonos mayores y 12 menores
    float bestScore = -2.0f;
    int bestTonic = 0;
    bool bestIsMinor = false;

    for (int tonic = 0; tonic < 12; ++tonic)
    {
        // Rotar el perfil para este tónico
        std::array<float, 12> majorRot {}, minorRot {};
        for (int i = 0; i < 12; ++i)
        {
            int idx = (i - tonic + 12) % 12;
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
// APVTS parameter listener (implementado via lambda o clase)
//==============================================================================
//==============================================================================
// Nota: este processor no hereda de AudioProcessorValueTreeState::Listener
// por simplicidad. Usamos polling en processBlock.

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
