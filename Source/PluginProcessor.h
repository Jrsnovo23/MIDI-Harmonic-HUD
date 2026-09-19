#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>

//==============================================================================
// Sonido básico: siempre se puede reproducir cualquier nota
class BasicSynthSound : public juce::SynthesiserSound
{
public:
    BasicSynthSound() = default;
    ~BasicSynthSound() override = default;

    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

//==============================================================================
// Voz básica: onda senoidal con envolvente ADSR simple (ataque lineal, release exponencial)
class BasicSynthVoice : public juce::SynthesiserVoice
{
public:
    BasicSynthVoice() = default;
    ~BasicSynthVoice() override = default;

    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<BasicSynthSound*> (sound) != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int /*currentPitchWheelPosition*/) override
    {
        currentAngle = 0.0;
        level = velocity * 0.25f;
        tailOff = 1.0f;

        // Frecuencia de la nota
        auto cyclesPerSecond = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
        auto cyclesPerSample = cyclesPerSecond / getSampleRate();
        angleDelta = cyclesPerSample * 2.0 * juce::MathConstants<double>::pi;

        // Envolvente ADSR: ataque lineal de 10 ms
        attackSamples = static_cast<int> (0.01 * getSampleRate());
        if (attackSamples <= 0)
            attackSamples = 1;
        attackCounter = 0;
        isReleasing = false;
    }

    void stopNote (float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            isReleasing = true;
            // tailOff ya está a 1.0, empezará a decaer
        }
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
                          int startSample,
                          int numSamples) override
    {
        if (angleDelta == 0.0)
            return;

        while (--numSamples >= 0)
        {
            // Envolvente de ataque
            float attackGain = 1.0f;
            if (attackCounter < attackSamples)
            {
                attackGain = static_cast<float> (attackCounter) / static_cast<float> (attackSamples);
                ++attackCounter;
            }

            // Envolvente de release exponencial
            if (isReleasing)
                tailOff *= 0.9995f;

            if (tailOff <= 0.0001f)
            {
                clearCurrentNote();
                angleDelta = 0.0;
                break;
            }

            auto currentSample = static_cast<float> (std::sin (currentAngle) * level * attackGain * tailOff);

            for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                outputBuffer.addSample (i, startSample, currentSample);

            currentAngle += angleDelta;
            ++startSample;
        }
    }

private:
    double currentAngle = 0.0;
    double angleDelta = 0.0;
    float level = 0.0f;
    float tailOff = 0.0f;

    int attackSamples = 0;
    int attackCounter = 0;
    bool isReleasing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BasicSynthVoice)
};

//==============================================================================
// Procesador principal
class MidiHarmonicHUDProcessor : public juce::AudioProcessor
{
public:
    //==========================================================================
    MidiHarmonicHUDProcessor();
    ~MidiHarmonicHUDProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==========================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==========================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Estado accesible desde el editor
    std::atomic<bool> activeMidiNotes[128];
    std::atomic<bool> muteSynth { false };

    // Historial de acordes (últimos 4)
    juce::StringArray chordHistory;
    juce::CriticalSection chordHistoryLock;

    // Acorde actual detectado
    juce::String currentChord;
    juce::CriticalSection currentChordLock;

    //==========================================================================
    juce::MidiKeyboardState keyboardState;

private:
    juce::Synthesiser synth;

    //==========================================================================
    void detectChordFromActiveNotes();
    juce::String noteName (int midiNote);
    juce::String identifyChord (const std::vector<int>& notes);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiHarmonicHUDProcessor)
};
