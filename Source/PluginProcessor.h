#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>

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
class BasicSynthVoice : public juce::SynthesiserVoice
{
public:
    BasicSynthVoice() = default;

    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<BasicSynthSound*> (sound) != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int) override
    {
        currentAngle = 0.0;
        level = velocity * 0.25f;
        tailOff = 1.0f;

        auto cyclesPerSecond = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
        auto cyclesPerSample = cyclesPerSecond / getSampleRate();
        angleDelta = cyclesPerSample * 2.0 * juce::MathConstants<double>::pi;

        attackSamples = static_cast<int> (0.01 * getSampleRate());
        if (attackSamples <= 0) attackSamples = 1;
        attackCounter = 0;
        isReleasing = false;
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff) { isReleasing = true; }
        else
        {
            clearCurrentNote();
            angleDelta = 0.0;
            tailOff = 0.0;
            isReleasing = false;
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample, int numSamples) override
    {
        if (angleDelta == 0.0) return;

        while (--numSamples >= 0)
        {
            float attackGain = 1.0f;
            if (attackCounter < attackSamples)
            {
                attackGain = static_cast<float> (attackCounter) / static_cast<float> (attackSamples);
                ++attackCounter;
            }

            if (isReleasing) tailOff *= 0.9995f;

            if (tailOff <= 0.0001f)
            {
                clearCurrentNote();
                angleDelta = 0.0;
                break;
            }

            auto s = static_cast<float> (std::sin (currentAngle) * level * attackGain * tailOff);
            for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                outputBuffer.addSample (i, startSample, s);

            currentAngle += angleDelta;
            ++startSample;
        }
    }

private:
    double currentAngle = 0.0, angleDelta = 0.0;
    float level = 0.0f, tailOff = 0.0f;
    int attackSamples = 0, attackCounter = 0;
    bool isReleasing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BasicSynthVoice)
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
    // Estado expuesto al editor
    std::atomic<bool> activeMidiNotes[128];
    std::atomic<bool> muteSynth { false };

    // Análisis armónico
    std::atomic<float> chordConfidence { 0.0f };   // 0..1
    std::atomic<float> harmonicTension { 0.0f };   // 0..1
    std::atomic<int>   currentRootPC    { -1 };    // pitch class 0..11 o -1

    // Historial FIFO de 4 acordes
    juce::StringArray chordHistory;
    juce::CriticalSection chordHistoryLock;

    juce::String currentChord { "---" };
    juce::CriticalSection currentChordLock;

    juce::MidiKeyboardState keyboardState;

private:
    juce::Synthesiser synth;

    // Estructura de análisis
    struct ChordAnalysis
    {
        juce::String name { "---" };
        float confidence = 0.0f;
        float tension = 0.0f;
        int   rootPitchClass = -1;
    };

    void detectChordFromActiveNotes();
    ChordAnalysis analyzeChord (const std::vector<int>& notes);
    static juce::String pitchClassName (int pc);
    static juce::String noteName (int midiNote);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiHarmonicHUDProcessor)
};
