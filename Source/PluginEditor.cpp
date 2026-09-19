#include "PluginEditor.h"

//==============================================================================
namespace
{
    // Orden del círculo de quintas (pitch classes)
    constexpr int kFifthOrderPC[12] = { 0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5 };

    // Nombres con sostenidos
    const char* const kPCNames[12] = { "C", "C#", "D", "D#", "E", "F",
                                       "F#", "G", "G#", "A", "A#", "B" };

    bool isBlackKey (int midiNote)
    {
        int pc = midiNote % 12;
        return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
    }
}

//==============================================================================
MidiHarmonicHUDEditor::MidiHarmonicHUDEditor (MidiHarmonicHUDProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setSize (720, 620);
    setResizable (false, false);

    muteButton.setColour (juce::ToggleButton::textColourId, textColour);
    muteButton.setColour (juce::ToggleButton::tickColourId, accentColour);
    muteButton.setColour (juce::ToggleButton::tickDisabledColourId, dimTextColour);
    muteButton.setToggleState (processorRef.muteSynth.load(), juce::dontSendNotification);
    muteButton.onClick = [this]()
    {
        processorRef.muteSynth.store (muteButton.getToggleState());
    };
    addAndMakeVisible (muteButton);

    startTimerHz (60);
}

MidiHarmonicHUDEditor::~MidiHarmonicHUDEditor()
{
    stopTimer();
}

//==============================================================================
void MidiHarmonicHUDEditor::paint (juce::Graphics& g)
{
    // Fondo con gradiente radial sutil
    juce::ColourGradient bgGrad (bgColour2, getWidth() * 0.5f, 0.0f,
                                  bgColour,  getWidth() * 0.5f, (float) getHeight(), true);
    g.setGradientFill (bgGrad);
    g.fillAll();

    // Grid sutil de fondo
    g.setColour (juce::Colour (0xffffffff).withAlpha (0.012f));
    for (int x = 0; x < getWidth(); x += 20)
        g.drawVerticalLine (x, 0.0f, (float) getHeight());
    for (int y = 0; y < getHeight(); y += 20)
        g.drawHorizontalLine (y, 0.0f, (float) getWidth());

    // ---- Layout ----
    auto bounds = getLocalBounds().reduced (12);

    // Header
    auto headerArea = bounds.removeFromTop (40);
    bounds.removeFromTop (8);
    drawHeader (g, headerArea);

    // Fila 1: Detecting + Circle of Fifths
    auto row1 = bounds.removeFromTop (260);
    auto detectingArea = row1.removeFromLeft (340);
    row1.removeFromLeft (8);
    auto circleArea = row1;

    drawDetectingPanel (g, detectingArea);
    drawCircleOfFifths (g, circleArea);

    bounds.removeFromTop (8);

    // Fila 2: Piano
    auto keyboardArea = bounds.removeFromTop (120);
    drawPianoKeyboard (g, keyboardArea);

    bounds.removeFromTop (8);

    // Fila 3: History + Diatonic
    auto row3 = bounds.removeFromTop (152);
    auto historyArea = row3.removeFromLeft (340);
    row3.removeFromLeft (8);
    auto diatonicArea = row3;

    drawHistoryPanel (g, historyArea);
    drawDiatonicPanel (g, diatonicArea);
}

//==============================================================================
void MidiHarmonicHUDEditor::drawHeader (juce::Graphics& g, juce::Rectangle<int> area)
{
    // Título con glow
    drawGlowText (g, "MIDI HARMONIC HUD",
                  area.removeFromLeft (400),
                  accentColour, 22.0f, 1.0f);

    // Subtítulo decorativo a la derecha del título
    g.setColour (dimTextColour);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("V 1.0",
                area.withWidth (80).withTrimmedLeft (2),
                juce::Justification::centredLeft, false);

    // Botón Mute a la derecha
    muteButton.setBounds (getWidth() - 110, 10, 100, 30);
}

//==============================================================================
void MidiHarmonicHUDEditor::drawGlowText (juce::Graphics& g, const juce::String& text,
                                          juce::Rectangle<int> area,
                                          juce::Colour colour,
                                          float fontSize, float alpha)
{
    // ✅ ScopedSaveState guarda y restaura TODO el estado del Graphics
    //    (incluida la opacidad) automáticamente al salir del scope.
    juce::Graphics::ScopedSaveState state (g);
    g.setOpacity (alpha);

    // Sombra/glow detrás
    g.setColour (colour.withAlpha (0.35f));
    g.setFont (juce::Font (juce::FontOptions (fontSize + 2.0f).withStyle ("Bold")));
    g.drawText (text, area.translated (0, 1), juce::Justification::centredLeft, false);

    // Texto principal
    g.setColour (colour);
    g.setFont (juce::Font (juce::FontOptions (fontSize).withStyle ("Bold")));
    g.drawText (text, area, juce::Justification::centredLeft, false);
}

//==============================================================================
void MidiHarmonicHUDEditor::drawDetectingPanel (juce::Graphics& g, juce::Rectangle<int> area)
{
    // Fondo con borde
    g.setColour (panelColour);
    g.fillRoundedRectangle (area.toFloat(), 12.0f);
    g.setColour (panelStroke);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 12.0f, 1.0f);

    auto inner = area.reduced (14);

    // Header
    g.setColour (dimTextColour);
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText ("DETECTING", inner.removeFromTop (16),
                juce::Justification::topLeft, false);

    inner.removeFromTop (6);

    // ---- Nombre del acorde con fade-in y glow ----
    auto chordArea = inner.removeFromTop (96);

    bool hasChord = (cachedChord != "---" && !cachedChord.isEmpty());

    if (hasChord)
    {
        // Glow detrás del acorde (con opacidad reducida)
        {
            juce::Graphics::ScopedSaveState state (g);
            g.setOpacity (chordFadeAlpha * 0.25f);
            g.setColour (accentColour);
            g.setFont (juce::Font (juce::FontOptions (48.0f).withStyle ("Bold")));
            g.drawText (cachedChord, chordArea.translated (0, 2),
                        juce::Justification::centred, false);
        }

        // Texto principal del acorde
        {
            juce::Graphics::ScopedSaveState state (g);
            g.setOpacity (chordFadeAlpha);
            g.setColour (textColour);
            g.setFont (juce::Font (juce::FontOptions (46.0f).withStyle ("Bold")));
            g.drawText (cachedChord, chordArea, juce::Justification::centred, false);
        }
    }
    else
    {
        g.setColour (dimTextColour.withAlpha (0.4f));
        g.setFont (juce::Font (juce::FontOptions (46.0f).withStyle ("Bold")));
        g.drawText ("---", chordArea, juce::Justification::centred, false);
    }

    inner.removeFromTop (4);

    // ---- Confidence meter ----
    auto confArea = inner.removeFromTop (32);
    drawConfidenceMeter (g, confArea);

    inner.removeFromTop (6);

    // ---- Tension gradient ----
    auto tensArea = inner.removeFromTop (32);
    drawTensionGradient (g, tensArea);
}

//==============================================================================
void MidiHarmonicHUDEditor::drawConfidenceMeter (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (dimTextColour);
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText ("CONFIDENCE", area.getX(), area.getY(), area.getWidth(), 14,
                juce::Justification::topLeft, false);

    // Porcentaje a la derecha
    g.setColour (accentColour);
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText (juce::String (juce::roundToInt (confidenceSmooth * 100.0f)) + "%",
                area.getX(), area.getY(), area.getWidth(), 14,
                juce::Justification::topRight, false);

    auto barArea = area.withTrimmedTop (16).withHeight (10).toFloat();

    // Fondo
    g.setColour (bgColour.darker (0.4f));
    g.fillRoundedRectangle (barArea, 5.0f);

    // Relleno con gradiente
    if (confidenceSmooth > 0.001f)
    {
        auto fillWidth = barArea.getWidth() * confidenceSmooth;
        auto fillArea = barArea.withWidth (fillWidth);

        juce::ColourGradient grad (accent3Colour, barArea.getX(), barArea.getY(),
                                    accentColour, barArea.getRight(), barArea.getY(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fillArea, 5.0f);

        // Highlight brillante en el borde superior
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.fillRoundedRectangle (fillArea.withHeight (3.0f), 1.5f);
    }

    // Borde
    g.setColour (panelStroke);
    g.drawRoundedRectangle (barArea, 5.0f, 1.0f);
}

//==============================================================================
void MidiHarmonicHUDEditor::drawTensionGradient (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (dimTextColour);
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText ("HARMONIC TENSION", area.getX(), area.getY(), area.getWidth(), 14,
                juce::Justification::topLeft, false);

    // Etiqueta cualitativa
    const char* label = "Consonant";
    if (tensionSmooth > 0.66f) label = "Dissonant";
    else if (tensionSmooth > 0.33f) label = "Mild";

    g.setColour (dangerColour.withAlpha (0.9f));
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText (label, area.getX(), area.getY(), area.getWidth(), 14,
                juce::Justification::topRight, false);

    auto barArea = area.withTrimmedTop (16).withHeight (10).toFloat();

    // Gradiente consonante → disonante
    juce::ColourGradient grad (accent3Colour, barArea.getX(), barArea.getY(),
                                dangerColour, barArea.getRight(), barArea.getY(), false);
    grad.addColour (0.5, juce::Colour (0xffe0af68)); // amarillo medio
    g.setGradientFill (grad);
    g.fillRoundedRectangle (barArea, 5.0f);

    // Atenuar cuando no hay notas
    if (cachedActiveCount == 0)
    {
        g.setColour (bgColour.withAlpha (0.7f));
        g.fillRoundedRectangle (barArea, 5.0f);
    }
    else
    {
        // Marcador vertical (indicador deslizante)
        float markerX = barArea.getX() + barArea.getWidth() * tensionSmooth;

        // Halo del marcador
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.fillEllipse (markerX - 6.0f, barArea.getCentreY() - 6.0f, 12.0f, 12.0f);

        // Marcador
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (markerX - 1.5f,
                                barArea.getY() - 2.0f,
                                3.0f,
                                barArea.getHeight() + 4.0f,
                                1.5f);
    }

    // Borde
    g.setColour (panelStroke);
    g.drawRoundedRectangle (barArea, 5.0f, 1.0f);
}

//==============================================================================
void MidiHarmonicHUDEditor::drawCircleOfFifths (juce::Graphics& g, juce::Rectangle<int> area)
{
    // Fondo
    g.setColour (panelColour);
    g.fillRoundedRectangle (area.toFloat(), 12.0f);
    g.setColour (panelStroke);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 12.0f, 1.0f);

    auto inner = area.reduced (14);
    g.setColour (dimTextColour);
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText ("CIRCLE OF FIFTHS", inner.removeFromTop (16),
                juce::Justification::topLeft, false);

    // Centro y radio
    auto centre = area.getCentre().toFloat();
    centre.y += 8.0f;
    float radius = juce::jmin ((float) area.getWidth(), (float) area.getHeight()) * 0.31f;

    // Anillo exterior decorativo
    g.setColour (accentColour.withAlpha (0.12f));
    g.drawEllipse (centre.x - radius - 4.0f, centre.y - radius - 4.0f,
                   (radius + 4.0f) * 2.0f, (radius + 4.0f) * 2.0f, 1.5f);

    // Anillo interior (círculo central donde va el acorde)
    float innerR = radius * 0.55f;
    g.setColour (bgColour.darker (0.3f));
    g.fillEllipse (centre.x - innerR, centre.y - innerR, innerR * 2.0f, innerR * 2.0f);

    // Glow pulsante si hay un acorde activo
    if (cachedActiveCount > 0)
    {
        float pulse = 0.5f + 0.5f * std::sin (circleGlowPhase);
        g.setColour (accentColour.withAlpha (0.15f + 0.1f * pulse));
        g.drawEllipse (centre.x - innerR - 3.0f, centre.y - innerR - 3.0f,
                       (innerR + 3.0f) * 2.0f, (innerR + 3.0f) * 2.0f, 2.0f);
    }

    // Acorde actual en el centro
    {
        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (chordFadeAlpha);
        g.setColour (textColour);
        g.setFont (juce::Font (juce::FontOptions (18.0f).withStyle ("Bold")));

        juce::String centreText = (cachedChord.isEmpty() || cachedChord == "---")
            ? "---" : cachedChord;
        g.drawText (centreText,
                    juce::Rectangle<float> (centre.x - innerR, centre.y - 12.0f,
                                             innerR * 2.0f, 24.0f).toNearestInt(),
                    juce::Justification::centred, false);
    }

    // Etiquetas de pitch class alrededor
    for (int i = 0; i < 12; ++i)
    {
        int pc = kFifthOrderPC[i];
        float angle = juce::MathConstants<float>::twoPi * ((float) i / 12.0f)
                    - juce::MathConstants<float>::halfPi
                    + circleRotation;

        float px = centre.x + std::cos (angle) * radius;
        float py = centre.y + std::sin (angle) * radius;

        bool isRoot = (pc == cachedRootPC) && (cachedActiveCount > 0);

        float circleSize = isRoot ? 26.0f : 20.0f;

        if (isRoot)
        {
            // Glow exterior
            float pulse = 0.5f + 0.5f * std::sin (circleGlowPhase * 1.4f);
            g.setColour (accentColour.withAlpha (0.25f + 0.15f * pulse));
            g.fillEllipse (px - circleSize * 0.9f, py - circleSize * 0.9f,
                           circleSize * 1.8f, circleSize * 1.8f);
        }

        g.setColour (isRoot ? accentColour : juce::Colour (0xff2a2a45));
        g.fillEllipse (px - circleSize * 0.5f, py - circleSize * 0.5f,
                       circleSize, circleSize);

        // Borde
        g.setColour (isRoot ? juce::Colours::white.withAlpha (0.7f) : panelStroke);
        g.drawEllipse (px - circleSize * 0.5f, py - circleSize * 0.5f,
                       circleSize, circleSize, 1.0f);

        // Texto
        g.setColour (isRoot ? juce::Colours::white : textColour.withAlpha (0.75f));
        g.setFont (juce::Font (juce::FontOptions (isRoot ? 12.0f : 11.0f)
                                .withStyle (isRoot ? "Bold" : "Regular")));

        juce::Rectangle<float> textRect (px - 20.0f, py - 8.0f, 40.0f, 16.0f);
        g.drawText (kPCNames[pc], textRect.toNearestInt(),
                    juce::Justification::centred, false);
    }
}

//==============================================================================
void MidiHarmonicHUDEditor::drawPianoKeyboard (juce::Graphics& g, juce::Rectangle<int> area)
{
    // Fondo del panel
    g.setColour (panelColour);
    g.fillRoundedRectangle (area.toFloat(), 12.0f);
    g.setColour (panelStroke);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 12.0f, 1.0f);

    auto inner = area.reduced (14, 10);

    // Header
    g.setColour (dimTextColour);
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText ("ACTIVE NOTES", inner.removeFromTop (14),
                juce::Justification::topLeft, false);

    inner.removeFromTop (4);

    const int lowMidi  = 36;  // C2
    const int highMidi = 72;  // C5
    const int numWhite = 22;

    auto keyArea = inner;
    const float whiteWidth  = (float) keyArea.getWidth() / numWhite;
    const float blackWidth  = whiteWidth * 0.62f;
    const float blackHeight = keyArea.getHeight() * 0.62f;

    // ---- Teclas blancas ----
    int whiteIndex = 0;
    for (int n = lowMidi; n <= highMidi; ++n)
    {
        if (isBlackKey (n)) continue;

        float x = keyArea.getX() + whiteIndex * whiteWidth;
        juce::Rectangle<float> key (x, (float) keyArea.getY(),
                                     whiteWidth, (float) keyArea.getHeight());
        key.reduce (0.75f, 0.0f);

        // ✅ Cast a size_t para evitar warning de signedness
        bool active = cachedActiveNotes[static_cast<size_t> (n)];

        if (active)
        {
            // Gradiente para tecla activa
            juce::ColourGradient grad (accentColour.brighter (0.3f), key.getX(), key.getY(),
                                        accentColour.darker (0.2f), key.getX(), key.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (key, 2.0f);

            // Glow superior
            g.setColour (juce::Colours::white.withAlpha (0.4f));
            g.fillRoundedRectangle (key.withHeight (3.0f), 1.5f);
        }
        else
        {
            g.setColour (juce::Colour (0xffe8e8f0));
            g.fillRoundedRectangle (key, 2.0f);
        }

        // Borde de la tecla
        g.setColour (juce::Colour (0xff2a2a45));
        g.drawRoundedRectangle (key, 2.0f, 1.0f);

        // Etiqueta C
        if (n % 12 == 0 && whiteWidth > 20.0f)
        {
            g.setColour (active ? juce::Colours::white : juce::Colour (0xff8888a0));
            g.setFont (juce::Font (juce::FontOptions (8.5f).withStyle ("Bold")));
            g.drawText ("C" + juce::String (n / 12 - 1),
                        key.toNearestInt().removeFromBottom (12),
                        juce::Justification::centred, false);
        }

        ++whiteIndex;
    }

    // ---- Teclas negras ----
    whiteIndex = 0;
    for (int n = lowMidi; n <= highMidi; ++n)
    {
        if (!isBlackKey (n))
        {
            ++whiteIndex;
            continue;
        }

        float x = keyArea.getX() + whiteIndex * whiteWidth - blackWidth * 0.5f;
        juce::Rectangle<float> key (x, (float) keyArea.getY(), blackWidth, blackHeight);

        // ✅ Cast a size_t para evitar warning de signedness
        bool active = cachedActiveNotes[static_cast<size_t> (n)];

        if (active)
        {
            juce::ColourGradient grad (accent2Colour.brighter (0.3f), key.getX(), key.getY(),
                                        accent2Colour.darker (0.3f), key.getX(), key.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (key, 2.0f);

            // Highlight
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.fillRoundedRectangle (key.withHeight (2.5f), 1.25f);
        }
        else
        {
            juce::ColourGradient grad (juce::Colour (0xff2a2a3a), key.getX(), key.getY(),
                                        juce::Colour (0xff0f0f1a), key.getX(), key.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (key, 2.0f);
        }

        g.setColour (juce::Colour (0xff000000).withAlpha (0.7f));
        g.drawRoundedRectangle (key, 2.0f, 1.0f);
    }
}

//==============================================================================
void MidiHarmonicHUDEditor::drawHistoryPanel (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (panelColour);
    g.fillRoundedRectangle (area.toFloat(), 12.0f);
    g.setColour (panelStroke);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 12.0f, 1.0f);

    auto inner = area.reduced (14);

    g.setColour (dimTextColour);
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText ("HISTORY (FIFO - last 4)", inner.removeFromTop (16),
                juce::Justification::topLeft, false);

    inner.removeFromTop (6);

    for (int i = 0; i < 4; ++i)
    {
        auto rowArea = inner.removeFromTop (28);
        inner.removeFromTop (2);

        bool hasItem = i < cachedHistory.size();

        // Número de índice
        g.setColour (dimTextColour.withAlpha (0.6f));
        g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
        g.drawText ("#" + juce::String (i + 1),
                    rowArea.removeFromLeft (26),
                    juce::Justification::centredLeft, false);

        if (hasItem)
        {
            // Chip de color para el más reciente
            bool isLatest = (i == 0);
            juce::Colour chipColour = isLatest ? accentColour : accent2Colour.withAlpha (0.4f);

            auto chipArea = rowArea.reduced (0, 2);

            g.setColour (chipColour.withAlpha (isLatest ? 0.2f : 0.08f));
            g.fillRoundedRectangle (chipArea.toFloat(), 6.0f);

            if (isLatest)
            {
                g.setColour (chipColour);
                g.fillRoundedRectangle (chipArea.toFloat().withWidth (3.0f), 1.5f);
            }

            g.setColour (isLatest ? textColour : textColour.withAlpha (0.75f));
            g.setFont (juce::Font (juce::FontOptions (14.0f)
                                    .withStyle (isLatest ? "Bold" : "Regular")));
            g.drawText (cachedHistory[i],
                        chipArea.reduced (10, 0),
                        juce::Justification::centredLeft, false);
        }
        else
        {
            g.setColour (dimTextColour.withAlpha (0.35f));
            g.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Regular")));
            g.drawText ("--",
                        rowArea,
                        juce::Justification::centredLeft, false);
        }
    }
}

//==============================================================================
void MidiHarmonicHUDEditor::drawDiatonicPanel (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (panelColour);
    g.fillRoundedRectangle (area.toFloat(), 12.0f);
    g.setColour (panelStroke);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 12.0f, 1.0f);

    auto inner = area.reduced (14);

    g.setColour (dimTextColour);
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText ("DIATONIC INFO", inner.removeFromTop (16),
                juce::Justification::topLeft, false);

    inner.removeFromTop (8);

    auto drawInfoRow = [&] (const juce::String& label, const juce::String& value,
                            juce::Colour valueColour)
    {
        auto row = inner.removeFromTop (24);
        g.setColour (dimTextColour);
        g.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Regular")));
        g.drawText (label, row.removeFromLeft (95),
                    juce::Justification::centredLeft, false);

        g.setColour (valueColour);
        g.setFont (juce::Font (juce::FontOptions (13.0f).withStyle ("Bold")));
        g.drawText (value, row, juce::Justification::centredLeft, false);
    };

    // Fila 1: Active notes
    drawInfoRow ("Active notes:", juce::String (cachedActiveCount),
                 cachedActiveCount > 0 ? accentColour : dimTextColour);

    // Fila 2: Root note
    juce::String rootStr = "---";
    if (cachedRootPC >= 0 && cachedActiveCount > 0)
        rootStr = juce::String (kPCNames[cachedRootPC]);
    drawInfoRow ("Root:", rootStr,
                 cachedRootPC >= 0 ? accent2Colour : dimTextColour);

    // Fila 3: Escala sugerida (heurística simple)
    juce::String scaleStr = "---";
    if (cachedRootPC >= 0 && cachedActiveCount > 0)
    {
        // ✅ FIX: envolver kPCNames en juce::String antes de concatenar
        // Relativo menor está 3 semitonos abajo (9 arriba)
        int relMinorPC = (cachedRootPC + 9) % 12;
        scaleStr = juce::String (kPCNames[cachedRootPC]) + " Major / "
                 + juce::String (kPCNames[relMinorPC]) + " Minor";
    }
    drawInfoRow ("Suggested scale:", scaleStr,
                 cachedRootPC >= 0 ? accent3Colour : dimTextColour);

    // Fila 4: Estado
    juce::String stateStr;
    if (cachedActiveCount == 0) stateStr = "Idle";
    else if (cachedConfidence > 0.9f) stateStr = "Matched pattern";
    else if (cachedConfidence > 0.4f) stateStr = "Partial analysis";
    else stateStr = "Cluster / unclassified";

    drawInfoRow ("Analysis:", stateStr,
                 cachedConfidence > 0.7f ? accent3Colour : textColour.withAlpha (0.8f));
}

//==============================================================================
void MidiHarmonicHUDEditor::resized()
{
    muteButton.setBounds (getWidth() - 110, 10, 100, 30);
}

//==============================================================================
void MidiHarmonicHUDEditor::timerCallback()
{
    // ===== Captura de estado desde el procesador =====
    {
        const juce::ScopedLock sl (processorRef.currentChordLock);
        cachedChord = processorRef.currentChord;
    }
    {
        const juce::ScopedLock sl (processorRef.chordHistoryLock);
        cachedHistory = processorRef.chordHistory;
    }

    int count = 0;
    for (int i = 0; i < 128; ++i)
    {
        bool on = processorRef.activeMidiNotes[i].load();
        // ✅ Cast a size_t para evitar warning de signedness
        cachedActiveNotes[static_cast<size_t> (i)] = on;
        if (on) ++count;
    }
    cachedActiveCount = count;

    cachedConfidence = processorRef.chordConfidence.load();
    cachedTension    = processorRef.harmonicTension.load();
    cachedRootPC     = processorRef.currentRootPC.load();

    // ===== Sincronización del botón mute =====
    if (muteButton.getToggleState() != processorRef.muteSynth.load())
        muteButton.setToggleState (processorRef.muteSynth.load(), juce::dontSendNotification);

    // ===== Animación 1: fade-in del cambio de acorde =====
    if (cachedChord != lastDisplayedChord)
    {
        lastDisplayedChord = cachedChord;
        chordChangeTimeMs = juce::Time::getMillisecondCounter();
    }
    auto elapsed = juce::Time::getMillisecondCounter() - chordChangeTimeMs;
    chordFadeAlpha = juce::jmin (1.0f, (float) elapsed / 350.0f);

    // ===== Animación 2: interpolación suave de confidence y tension =====
    confidenceSmooth += (cachedConfidence - confidenceSmooth) * 0.18f;
    tensionSmooth    += (cachedTension    - tensionSmooth)    * 0.18f;

    // ===== Animación 3: rotación del círculo de quintas =====
    if (cachedRootPC >= 0)
    {
        int rootIndex = 0;
        for (int i = 0; i < 12; ++i)
        {
            if (kFifthOrderPC[i] == cachedRootPC) { rootIndex = i; break; }
        }
        targetRotation = -juce::MathConstants<float>::twoPi
                       * ((float) rootIndex / 12.0f);
    }

    // Interpolar tomando el camino más corto
    float diff = targetRotation - circleRotation;
    while (diff >  juce::MathConstants<float>::pi) diff -= juce::MathConstants<float>::twoPi;
    while (diff < -juce::MathConstants<float>::pi) diff += juce::MathConstants<float>::twoPi;
    circleRotation += diff * 0.12f;

    // ===== Animación 4: fase de glow pulsante =====
    circleGlowPhase += 0.08f;
    if (circleGlowPhase > juce::MathConstants<float>::twoPi)
        circleGlowPhase -= juce::MathConstants<float>::twoPi;

    repaint();
}
