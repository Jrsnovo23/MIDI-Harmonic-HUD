#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <array>

//==============================================================================
// Parámetros (IDs)
namespace ParamIDs
{
    static constexpr const char* muteSynth   = "muteSynth";
    static constexpr const char* presetIndex = "presetIndex";
    static constexpr const char* themeIndex  = "themeIndex";
}

//==============================================================================
class BasicSynthSound : public juce::SynthesiserSound
{
public:
    BasicSynthSound() = default;
    ~BasicSynthSound() override = default;
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

//==============================================================================
// Voz mejorada: armónicos + ADSR real + filtro paso-bajo
class BasicSynthVoice : public juce::SynthesiserVoice
{
public:
    BasicSynthVoice();

    bool canPlaySound (juce::SynthesiserSound* sound) override;

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int currentPitchWheelPosition) override;

    void stopNote (float velocity, bool allowTailOff) override;

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample, int numSamples) override;

    void setCurrentPlaybackSampleRate (double newRate) override;

    void setPreset (int presetIndex);

private:
    struct PresetParams
    {
        float harmonic2Gain;
        float harmonic3Gain;
        float harmonic5Gain;
        float attackTime;
        float decayTime;
        float sustainLevel;
        float releaseTime;
        float filterCutoff;
        float filterQ;
        float filterEnvAmount;
    };

    PresetParams preset;
    int currentPreset = 0;

    double phase1 = 0.0;
    double phase2 = 0.0;
    double phase3 = 0.0;
    double phase5 = 0.0;

    double baseFreq = 0.0;
    double sampleRate = 44100.0;
    float  velocity = 0.0f;

    enum class EnvStage { Idle, Attack, Decay, Sustain, Release };
    EnvStage envStage = EnvStage::Idle;
    float envLevel = 0.0f;
    float envAttackInc = 0.0f;
    float envDecayCoef = 0.0f;
    float envReleaseCoef = 0.0f;

    float filterState = 0.0f;
    float filterEnv = 0.0f;
    float filterCutoffHz = 2000.0f;
    float filterQ = 0.7f;

    void updateEnvelopeIncrements();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BasicSynthVoice)
};

//==============================================================================
struct ChordAnalysis
{
    juce::String name { "---" };
    juce::String baseName { "---" };
    juce::String inversionText {};
    float confidence = 0.0f;
    float tension = 0.0f;
    int   rootPitchClass = -1;
    int   bassPitchClass = -1;
    int   inversion = 0;
};

//==============================================================================
struct KeyDetection
{
    juce::String name { "---" };
    float confidence = 0.0f;
    int   tonicPitchClass = -1;
    bool  isMinor = false;
};

//==============================================================================
class MidiHarmonicHUDProcessor : public juce::AudioProcessor
{
public:
    MidiHarmonicHUDProcessor();
    ~MidiHarmonicHUDProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

    //==========================================================================
    // Estado expuesto al editor
    std::atomic<bool> activeMidiNotes[128];
    std::atomic<bool> muteSynth { false };

    std::atomic<float> chordConfidence { 0.0f };
    std::atomic<float> harmonicTension { 0.0f };
    std::atomic<int>   currentRootPC    { -1 };
    std::atomic<int>   currentBassPC    { -1 };
    std::atomic<int>   currentInversion { 0 };

    static constexpr int TENSION_HISTORY_SIZE = 256;
    std::array<std::atomic<float>, TENSION_HISTORY_SIZE> tensionHistory;
    std::atomic<int> tensionHistoryWritePos { 0 };

    std::atomic<int>   detectedKeyTonic { -1 };
    std::atomic<bool>  detectedKeyIsMinor { false };
    std::atomic<float> detectedKeyConfidence { 0.0f };

    juce::StringArray chordHistory;
    juce::CriticalSection chordHistoryLock;

    juce::String currentChord { "---" };
    juce::CriticalSection currentChordLock;

    juce::String currentKeyText { "---" };
    juce::CriticalSection currentKeyLock;

    juce::MidiKeyboardState keyboardState;

    //==========================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::Synthesiser synth;

    // Último preset aplicado a las voces (para evitar llamadas redundantes)
    int lastPreset = -1;

    std::array<std::atomic<int>, 12> pitchClassHistogram;
    std::atomic<uint32_t> totalNotesSeen { 0 };
    std::atomic<uint32_t> lastDecayMs { 0 };

    void detectChordFromActiveNotes();
    ChordAnalysis analyzeChord (const std::vector<int>& notes);
    KeyDetection detectKey();

    static juce::String pitchClassName (int pc);
    static juce::String noteName (int midiNote);

    void decayHistogramIfNeeded();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiHarmonicHUDProcessor)
};
