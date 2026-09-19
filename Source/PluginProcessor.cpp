#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MidiHarmonicHUDProcessor::MidiHarmonicHUDProcessor()
    : AudioProcessor (BusesProperties()
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // Inicializar todas las notas como inactivas
    for (int i = 0; i < 128; ++i)
        activeMidiNotes[i].store (false);

    // Añadir 8 voces de polifonía
    for (int i = 0; i < 8; ++i)
        synth.addVoice (new BasicSynthVoice());

    // Añadir el sonido
    synth.addSound (new BasicSynthSound());
}

MidiHarmonicHUDProcessor::~MidiHarmonicHUDProcessor() = default;

//==============================================================================
void MidiHarmonicHUDProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
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
    // Solo aceptamos salida estéreo
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

//==============================================================================
void MidiHarmonicHUDProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Limpiar canales de entrada no usados (por si acaso)
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Procesar eventos MIDI entrantes
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            int noteNumber = message.getNoteNumber();
            if (noteNumber >= 0 && noteNumber < 128)
                activeMidiNotes[noteNumber].store (true);
        }
        else if (message.isNoteOff())
        {
            int noteNumber = message.getNoteNumber();
            if (noteNumber >= 0 && noteNumber < 128)
                activeMidiNotes[noteNumber].store (false);
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            for (int i = 0; i < 128; ++i)
                activeMidiNotes[i].store (false);
        }
    }

    // Detectar acorde actual a partir de las notas activas
    detectChordFromActiveNotes();

    // Renderizar audio del sintetizador (si no está muteado)
    if (!muteSynth.load())
    {
        synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
    }
    else
    {
        // Silenciar el buffer de audio sin interrumpir la detección MIDI
        buffer.clear();
        // Aun así, debemos procesar el MIDI para que el synth no se desincronice
        // pero sin generar audio. Una forma segura es llamar a renderNextBlock
        // en un buffer temporal y descartarlo, o simplemente no llamarlo.
        // Como no llamamos a renderNextBlock, las voces no avanzan, pero al
        // desmutear, el estado del synth podría estar desfasado. Para evitar
        // problemas, procesamos el MIDI en un buffer temporal.
        juce::AudioBuffer<float> tempBuffer (buffer.getNumChannels(), buffer.getNumSamples());
        tempBuffer.clear();
        synth.renderNextBlock (tempBuffer, midiMessages, 0, tempBuffer.getNumSamples());
    }
}

//==============================================================================
void MidiHarmonicHUDProcessor::detectChordFromActiveNotes()
{
    std::vector<int> notes;
    for (int i = 0; i < 128; ++i)
    {
        if (activeMidiNotes[i].load())
            notes.push_back (i);
    }

    juce::String detected;

    if (notes.empty())
    {
        detected = "---";
    }
    else if (notes.size() == 1)
    {
        detected = noteName (notes[0]);
    }
    else
    {
        detected = identifyChord (notes);
    }

    {
        const juce::ScopedLock sl (currentChordLock);
        if (currentChord != detected)
        {
            currentChord = detected;

            if (detected != "---")
            {
                const juce::ScopedLock histLock (chordHistoryLock);
                chordHistory.insert (0, detected);
                if (chordHistory.size() > 4)
                    chordHistory.remove (chordHistory.size() - 1);
            }
        }
    }
}

//==============================================================================
juce::String MidiHarmonicHUDProcessor::noteName (int midiNote)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F",
                                   "F#", "G", "G#", "A", "A#", "B" };
    int octave = (midiNote / 12) - 1;
    int pitchClass = midiNote % 12;
    return juce::String (names[pitchClass]) + juce::String (octave);
}

//==============================================================================
juce::String MidiHarmonicHUDProcessor::identifyChord (const std::vector<int>& notes)
{
    // Ordenar las notas
    std::vector<int> sorted = notes;
    std::sort (sorted.begin(), sorted.end());

    // Obtener las clases de altura (pitch classes) sin duplicados
    std::set<int> pitchClassSet;
    for (int n : sorted)
        pitchClassSet.insert (n % 12);

    std::vector<int> pcs (pitchClassSet.begin(), pitchClassSet.end());

    // Si todas las notas están en un intervalo de octava, tratamos como acorde
    // Simplificación: usamos la raíz como la nota más grave
    int root = sorted[0] % 12;

    // Normalizar intervalos respecto a la raíz
    std::set<int> intervals;
    for (int pc : pcs)
    {
        int interval = (pc - root + 12) % 12;
        intervals.insert (interval);
    }

    // Patrones comunes de acordes (intervalos desde la raíz)
    // Tríadas
    if (intervals == std::set<int>{0, 4, 7}) return noteName (root) + "maj";
    if (intervals == std::set<int>{0, 3, 7}) return noteName (root) + "min";
    if (intervals == std::set<int>{0, 3, 6}) return noteName (root) + "dim";
    if (intervals == std::set<int>{0, 4, 8}) return noteName (root) + "aug";

    // Séptimas
    if (intervals == std::set<int>{0, 4, 7, 11}) return noteName (root) + "maj7";
    if (intervals == std::set<int>{0, 4, 7, 10}) return noteName (root) + "7";
    if (intervals == std::set<int>{0, 3, 7, 10}) return noteName (root) + "m7";
    if (intervals == std::set<int>{0, 3, 7, 11}) return noteName (root) + "mMaj7";
    if (intervals == std::set<int>{0, 3, 6, 10}) return noteName (root) + "m7b5";
    if (intervals == std::set<int>{0, 3, 6, 9}) return noteName (root) + "dim7";

    // Suspendidos
    if (intervals == std::set<int>{0, 2, 7}) return noteName (root) + "sus2";
    if (intervals == std::set<int>{0, 5, 7}) return noteName (root) + "sus4";

    // Sextas
    if (intervals == std::set<int>{0, 4, 7, 9}) return noteName (root) + "6";
    if (intervals == std::set<int>{0, 3, 7, 9}) return noteName (root) + "m6";

    // Novenas
    if (intervals == std::set<int>{0, 4, 7, 10, 2}) return noteName (root) + "9";
    if (intervals == std::set<int>{0, 3, 7, 10, 2}) return noteName (root) + "m9";
    if (intervals == std::set<int>{0, 4, 7, 11, 2}) return noteName (root) + "maj9";

    // Si no coincide, devolver las notas individuales
    juce::String result;
    for (size_t i = 0; i < sorted.size(); ++i)
    {
        if (i > 0) result += " ";
        result += noteName (sorted[i]);
    }
    return result;
}

//==============================================================================
juce::AudioProcessorEditor* MidiHarmonicHUDProcessor::createEditor()
{
    return new MidiHarmonicHUDEditor (*this);
}

bool MidiHarmonicHUDProcessor::hasEditor() const { return true; }

//==============================================================================
const juce::String MidiHarmonicHUDProcessor::getName() const { return "MIDI Harmonic HUD"; }
bool MidiHarmonicHUDProcessor::acceptsMidi() const { return true; }
bool MidiHarmonicHUDProcessor::producesMidi() const { return false; }
bool MidiHarmonicHUDProcessor::isMidiEffect() const { return false; }
double MidiHarmonicHUDProcessor::getTailLengthSeconds() const { return 2.0; }

//==============================================================================
int MidiHarmonicHUDProcessor::getNumPrograms() { return 1; }
int MidiHarmonicHUDProcessor::getCurrentProgram() { return 0; }
void MidiHarmonicHUDProcessor::setCurrentProgram (int /*index*/) {}
const juce::String MidiHarmonicHUDProcessor::getProgramName (int /*index*/) { return {}; }
void MidiHarmonicHUDProcessor::changeProgramName (int /*index*/, const juce::String& /*newName*/) {}

//==============================================================================
void MidiHarmonicHUDProcessor::getStateInformation (juce::MemoryBlock& /*destData*/)
{
    // No hay parámetros persistentes por ahora
}

void MidiHarmonicHUDProcessor::setStateInformation (const void* /*data*/, int /*sizeInBytes*/)
{
    // No hay parámetros persistentes por ahora
}

//==============================================================================
// Creación del plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MidiHarmonicHUDProcessor();
}
