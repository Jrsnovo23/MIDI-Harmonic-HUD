#include "PluginEditor.h"

//==============================================================================
namespace
{
    constexpr int kFifthOrderPC[12] = { 0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5 };
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
    : AudioProcessorEditor (&p),
      processorRef (p),
      muteAttachment (p.apvts, ParamIDs::muteSynth, muteButton),
      presetAttachment (p.apvts, ParamIDs::presetIndex, presetCombo),
      themeAttachment (p.apvts, ParamIDs::themeIndex, themeCombo)
{
    setSize (780, 800);
    setResizable (false, false);

    // Cargar tema inicial
    int themeId = 0;
    if (auto* p2 = processorRef.apvts.getRawParameterValue (ParamIDs::themeIndex))
        themeId = (int) p2->load();
    cachedThemeId = themeId;
    theme = ThemeManager::getTheme (themeId);

    // ---- Mute Button ----
    muteButton.setColour (juce::ToggleButton::textColourId, theme.text);
    muteButton.setColour (juce::ToggleButton::tickColourId, theme.accent);
    muteButton.setColour (juce::ToggleButton::tickDisabledColourId, theme.dimText);
    addAndMakeVisible (muteButton);

    // ---- Preset Combo ----
    presetLabel.setText ("Preset", juce::dontSendNotification);
    presetLabel.setColour (juce::Label::textColourId, theme.dimText);
    presetLabel.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    presetLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (presetLabel);

    presetCombo.addItemList ({ "Electric Piano", "Warm Pad", "Pluck" }, 1);
    presetCombo.setColour (juce::ComboBox::backgroundColourId, theme.panel);
    presetCombo.setColour (juce::ComboBox::textColourId, theme.text);
    presetCombo.setColour (juce::ComboBox::outlineColourId, theme.panelStroke);
    presetCombo.setColour (juce::ComboBox::arrowColourId, theme.accent);
    addAndMakeVisible (presetCombo);

    // ---- Theme Combo ----
    themeLabel.setText ("Theme", juce::dontSendNotification);
    themeLabel.setColour (juce::Label::textColourId, theme.dimText);
    themeLabel.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    themeLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (themeLabel);

    themeCombo.addItemList (ThemeManager::getThemeNames(), 1);
    themeCombo.setColour (juce::ComboBox::backgroundColourId, theme.panel);
    themeCombo.setColour (juce::ComboBox::textColourId, theme.text);
    themeCombo.setColour (juce::ComboBox::outlineColourId, theme.panelStroke);
    themeCombo.setColour (juce::ComboBox::arrowColourId, theme.accent);
    themeCombo.setSelectedId (themeId + 1, juce::dontSendNotification);

    // Reaccionar al cambio de tema desde la UI
    themeCombo.onChange = [this]()
    {
        int id = themeCombo.getSelectedId() - 1;
        cachedThemeId = id;
        theme = ThemeManager::getTheme (id);

        muteButton.setColour (juce::ToggleButton::textColourId, theme.text);
        muteButton.setColour (juce::ToggleButton::tickColourId, theme.accent);
        presetLabel.setColour (juce::Label::textColourId, theme.dimText);
        themeLabel.setColour (juce::Label::textColourId, theme.dimText);
        presetCombo.setColour (juce::ComboBox::backgroundColourId, theme.panel);
        presetCombo.setColour (juce::ComboBox::textColourId, theme.text);
        presetCombo.setColour (juce::ComboBox::outlineColourId, theme.panelStroke);
        presetCombo.setColour (juce::ComboBox::arrowColourId, theme.accent);
        themeCombo.setColour (juce::ComboBox::backgroundColourId, theme.panel);
        themeCombo.setColour (juce::ComboBox::textColourId, theme.text);
        themeCombo.setColour (juce::ComboBox::outlineColourId, theme.panelStroke);
        themeCombo.setColour (juce::ComboBox::arrowColourId, theme.accent);

        repaint();
    };
    addAndMakeVisible (themeCombo);

    startTimerHz (60);
}

MidiHarmonicHUDEditor::~MidiHarmonicHUDEditor()
{
    stopTimer();
}

//==============================================================================
void MidiHarmonicHUDEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient bgGrad (theme.bg2, getWidth() * 0.5f, 0.0f,
                                  theme.bg,  getWidth() * 0.5f, (float) getHeight(), true);
    g.setGradientFill (bgGrad);
    g.fillAll();

    g.setColour (juce::Colour (0xffffffff).withAlpha (0.012f));
    for (int x = 0; x < getWidth(); x += 20)
        g.drawVerticalLine (x, 0.0f, (float) getHeight());
    for (int y = 0; y < getHeight(); y += 20)
        g.drawHorizontalLine (y, 0.0f, (float) getWidth());

    auto bounds = getLocalBounds().reduced (12);

    auto headerArea = bounds.removeFromTop (40);
    bounds.removeFromTop (8);
    drawHeader (g, headerArea);

    auto row1 = bounds.removeFromTop (260);
    auto detectingArea = row1.removeFromLeft (380);
    row1.removeFromLeft (8);
    auto circleArea = row1;

    drawDetectingPanel (g, detectingArea);
    drawCircleOfFifths (g, circleArea);

    bounds.removeFromTop (8);

    auto keyboardArea = bounds.removeFromTop (110);
    drawPianoKeyboard (g, keyboardArea);

    bounds.removeFromTop (8);

    auto row3 = bounds.removeFromTop (160);
    auto historyArea = row3.removeFromLeft (380);
    row3.removeFromLeft (8);
    auto diatonicArea = row3;

    drawHistoryPanel (g, historyArea);
    drawDiatonicPanel (g, diatonicArea);

    bounds.removeFromTop (8);

    auto tensionArea = bounds.removeFromTop (100);
    drawTensionGraph (g, tensionArea);
}

//==============================================================================
void MidiHarmonicHUDEditor::drawHeader (juce::Graphics& g, juce::Rectangle<int> area)
{
    drawGlowText (g, "MIDI HARMONIC HUD",
                  area.removeFromLeft (300),
                  theme.accent, 22.0f, 1.0f);

    g.setColour (theme.dimText);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("V 2.0",
                area.removeFromLeft (50),
                juce::Justification::centredLeft, false);
}

//==============================================================================
void MidiHarmonicHUDEditor::drawGlowText (juce::Graphics& g, const juce::String& text,
                                           juce::Rectangle<int> area,
                                           juce::Colour colour,
                                           float fontSize, float alpha)
{
    juce::Graphics::ScopedSaveState state (g);
    g.setOpacity (alpha);

    g.setColour (colour.withAlpha (0.35f));
    g.setFont (juce::Font (juce::FontOptions (fontSize + 2.0f).withStyle ("Bold")));
    g.drawText (text, area.translated (0, 1), juce::Justification::centredLeft, false);

    g.setColour (colour);
    g.setFont (juce::Font (juce::FontOptions (fontSize).withStyle ("Bold")));
    g.drawText (text, area, juce::Justification::centredLeft, false);
}

//==============================================================================
void MidiHarmonicHUDEditor::drawDetectingPanel (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (theme.panel);
    g.fillRoundedRectangle (area.toFloat(), 12.0f);
    g.setColour (theme.panelStroke);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 12.0f, 1.0f);

    auto inner = area.reduced (14);

    g.setColour (theme.dimText);
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText ("DETECTING", inner.removeFromTop (16),
                juce::Justification::topLeft, false);

    inner.removeFromTop (6);

    auto chordArea = inner.removeFromTop (96);
    bool hasChord = (cachedChord != "---" && ! cachedChord.isEmpty());

    if (hasChord)
    {
        {
            juce::Graphics::ScopedSaveState state (g);
            g.setOpacity (chordFadeAlpha * 0.25f);
            g.setColour (theme.accent);
            g.setFont (juce::Font (juce::FontOptions (48.0f).withStyle ("Bold")));
            g.drawText (cachedChord, chordArea.translated (0, 2),
                        juce::Justification::centred, false);
        }
        {
            juce::Graphics::ScopedSaveState state (g);
            g.setOpacity (chordFadeAlpha);
            g.setColour (theme.text);
            g.setFont (juce::Font (juce::FontOptions (46.0f).withStyle ("Bold")));
            g.drawText (cachedChord, chordArea, juce::Justification::centred, false);
        }
    }
    else
    {
        g.setColour (theme.dimText.withAlpha (0.4f));
        g.setFont (juce::Font (juce::FontOptions (46.0f).withStyle ("Bold")));
        g.drawText ("---", chordArea, juce::Justification::centred, false);
    }

    inner.removeFromTop (4);

    auto confArea = inner.removeFromTop (32);
    drawConfidenceMeter (g, confArea);

    inner.removeFromTop (6);

    auto tensArea = inner.removeFromTop (32);
    drawTensionGradient (g, tensArea);
}

//==============================================================================
void MidiHarmonicHUDEditor::drawConfidenceMeter (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (theme.dimText);
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText ("CONFIDENCE", area.getX(), area.getY(), area.getWidth(), 14,
                juce::Justification::topLeft, false);

    g.setColour (theme.accent);
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText (juce::String (juce::roundToInt (confidenceSmooth * 100.0f)) + "%",
                area.getX(), area.getY(), area.getWidth(), 14,
                juce::Justification::topRight, false);

    auto barArea = area.withTrimmedTop (16).withHeight (10).toFloat();

    g.setColour (theme.bg.darker (0.4f));
    g.fillRoundedRectangle (barArea, 5.0f);

    if (confidenceSmooth > 0.001f)
    {
        auto fillWidth = barArea.getWidth() * confidenceSmooth;
        auto fillArea = barArea.withWidth (fillWidth);

        juce::ColourGradient grad (theme.accent3, barArea.getX(), barArea.getY(),
                                    theme.accent, barArea.getRight(), barArea.getY(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fillArea, 5.0f);

        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.fillRoundedRectangle (fillArea.withHeight (3.0f), 1.5f);
    }

    g.setColour (theme.panelStroke);
    g.drawRoundedRectangle (barArea, 5.0f, 1.0f);
}

//==============================================================================
void MidiHarmonicHUDEditor::drawTensionGradient (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (theme.dimText);
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText ("HARMONIC TENSION", area.getX(), area.getY(), area.getWidth(), 14,
                juce::Justification::topLeft, false);

    const char* label = "Consonant";
    if (tensionSmooth > 0.66f) label = "Dissonant";
    else if (tensionSmooth > 0.33f) label = "Mild";

    g.setColour (theme.danger.withAlpha (0.9f));
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText (label, area.getX(), area.getY(), area.getWidth(), 14,
                juce::Justification::topRight, false);

    auto barArea = area.withTrimmedTop (16).withHeight (10).toFloat();

    juce::ColourGradient grad (theme.accent3, barArea.getX(), barArea.getY(),
                                theme.danger, barArea.getRight(), barArea.getY(), false);
    grad.addColour (0.5, theme.warn);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (barArea, 5.0f);

    if (cachedActiveCount == 0)
    {
        g.setColour (theme.bg.withAlpha (0.7f));
        g.fillRoundedRectangle (barArea, 5.0f);
    }
    else
    {
        float markerX = barArea.getX() + barArea.getWidth() * tensionSmooth;

        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.fillEllipse (markerX - 6.0f, barArea.getCentreY() - 6.0f, 12.0f, 12.0f);

        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (markerX - 1.5f, barArea.getY() - 2.0f,
                                 3.0f, barArea.getHeight() + 4.0f, 1.5f);
    }

    g.setColour (theme.panelStroke);
    g.drawRoundedRectangle (barArea, 5.0f, 1.0f);
}

//==============================================================================
void MidiHarmonicHUDEditor::drawCircleOfFifths (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (theme.panel);
    g.fillRoundedRectangle (area.toFloat(), 12.0f);
    g.setColour (theme.panelStroke);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 12.0f, 1.0f);

    auto inner = area.reduced (14);
    g.setColour (theme.dimText);
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText ("CIRCLE OF FIFTHS", inner.removeFromTop (16),
                juce::Justification::topLeft, false);

    auto centre = area.getCentre().toFloat();
    centre.y += 8.0f;
    float radius = juce::jmin ((float) area.getWidth(), (float) area.getHeight()) * 0.31f;

    g.setColour (theme.accent.withAlpha (0.12f));
    g.drawEllipse (centre.x - radius - 4.0f, centre.y - radius - 4.0f,
                   (radius + 4.0f) * 2.0f, (radius + 4.0f) * 2.0f, 1.5f);

    float innerR = radius * 0.55f;
    g.setColour (theme.bg.darker (0.3f));
    g.fillEllipse (centre.x - innerR, centre.y - innerR, innerR * 2.0f, innerR * 2.0f);

    if (cachedActiveCount > 0)
    {
        float pulse = 0.5f + 0.5f * std::sin (circleGlowPhase);
        g.setColour (theme.accent.withAlpha (0.15f + 0.1f * pulse));
        g.drawEllipse (centre.x - innerR - 3.0f, centre.y - innerR - 3.0f,
                       (innerR + 3.0f) * 2.0f, (innerR + 3.0f) * 2.0f, 2.0f);
    }

    {
        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (chordFadeAlpha);
        g.setColour (theme.text);
        g.setFont (juce::Font (juce::FontOptions (18.0f).withStyle ("Bold")));

        juce::String centreText = (cachedChord.isEmpty() || cachedChord == "---")
            ? "---" : cachedChord;
        g.drawText (centreText,
                    juce::Rectangle<float> (centre.x - innerR, centre.y - 12.0f,
                                             innerR * 2.0f, 24.0f).toNearestInt(),
                    juce::Justification::centred, false);
    }

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
            float pulse = 0.5f + 0.5f * std::sin (circleGlowPhase * 1.4f);
            g.setColour (theme.accent.withAlpha (0.25f + 0.15f * pulse));
            g.fillEllipse (px - circleSize * 0.9f, py - circleSize * 0.9f,
                           circleSize * 1.8f, circleSize * 1.8f);
        }

        g.setColour (isRoot ? theme.accent : theme.panelStroke);
        g.fillEllipse (px - circleSize * 0.5f, py - circleSize * 0.5f,
                       circleSize, circleSize);

        g.setColour (isRoot ? juce::Colours::white.withAlpha (0.7f) : theme.panelStroke);
        g.drawEllipse (px - circleSize * 0.5f, py - circleSize * 0.5f,
                       circleSize, circleSize, 1.0f);

        g.setColour (isRoot ? juce::Colours::white : theme.text.withAlpha (0.75f));
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
    g.setColour (theme.panel);
    g.fillRoundedRectangle (area.toFloat(), 12.0f);
    g.setColour (theme.panelStroke);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 12.0f, 1.0f);

    auto inner = area.reduced (14, 10);

    g.setColour (theme.dimText);
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText ("ACTIVE NOTES", inner.removeFromTop (14),
                juce::Justification::topLeft, false);

    inner.removeFromTop (4);

    const int lowMidi  = 36;
    const int highMidi = 72;
    const int numWhite = 22;

    auto keyArea = inner;
    const float whiteWidth  = (float) keyArea.getWidth() / numWhite;
    const float blackWidth  = whiteWidth * 0.62f;
    const float blackHeight = keyArea.getHeight() * 0.62f;

    int whiteIndex = 0;
    for (int n = lowMidi; n <= highMidi; ++n)
    {
        if (isBlackKey (n)) continue;

        float x = keyArea.getX() + whiteIndex * whiteWidth;
        juce::Rectangle<float> key (x, (float) keyArea.getY(),
                                     whiteWidth, (float) keyArea.getHeight());
        key.reduce (0.75f, 0.0f);

        bool active = cachedActiveNotes[static_cast<size_t> (n)];

        if (active)
        {
            juce::ColourGradient grad (theme.accent.brighter (0.3f), key.getX(), key.getY(),
                                        theme.accent.darker (0.2f), key.getX(), key.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (key, 2.0f);

            g.setColour (juce::Colours::white.withAlpha (0.4f));
            g.fillRoundedRectangle (key.withHeight (3.0f), 1.5f);
        }
        else
        {
            g.setColour (theme.text.withAlpha (0.9f));
            g.fillRoundedRectangle (key, 2.0f);
        }

        g.setColour (theme.panelStroke);
        g.drawRoundedRectangle (key, 2.0f, 1.0f);

        if (n % 12 == 0 && whiteWidth > 20.0f)
        {
            g.setColour (active ? juce::Colours::white : theme.dimText);
            g.setFont (juce::Font (juce::FontOptions (8.5f).withStyle ("Bold")));
            g.drawText ("C" + juce::String (n / 12 - 1),
                        key.toNearestInt().removeFromBottom (12),
                        juce::Justification::centred, false);
        }

        ++whiteIndex;
    }

    whiteIndex = 0;
    for (int n = lowMidi; n <= highMidi; ++n)
    {
        if (! isBlackKey (n)) { ++whiteIndex; continue; }

        float x = keyArea.getX() + whiteIndex * whiteWidth - blackWidth * 0.5f;
        juce::Rectangle<float> key (x, (float) keyArea.getY(), blackWidth, blackHeight);

        bool active = cachedActiveNotes[static_cast<size_t> (n)];

        if (active)
        {
            juce::ColourGradient grad (theme.accent2.brighter (0.3f), key.getX(), key.getY(),
                                        theme.accent2.darker (0.3f), key.getX(), key.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (key, 2.0f);

            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.fillRoundedRectangle (key.withHeight (2.5f), 1.25f);
        }
        else
        {
            juce::ColourGradient grad (theme.bg.brighter (0.3f), key.getX(), key.getY(),
                                        theme.bg.darker (0.5f), key.getX(), key.getBottom(), false);
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
    g.setColour (theme.panel);
    g.fillRoundedRectangle (area.toFloat(), 12.0f);
    g.setColour (theme.panelStroke);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 12.0f, 1.0f);

    auto inner = area.reduced (14);

    g.setColour (theme.dimText);
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText ("HISTORY", inner.removeFromTop (16),
                juce::Justification::topLeft, false);

    inner.removeFromTop (6);

    if (cachedHistory.isEmpty())
    {
        g.setColour (theme.dimText.withAlpha (0.4f));
        g.setFont (juce::Font (juce::FontOptions (14.0f)));
        g.drawText ("No chords yet", inner,
                    juce::Justification::centred, false);
        return;
    }

    for (int i = 0; i < cachedHistory.size(); ++i)
    {
        auto rowArea = inner.removeFromTop (28);
        bool isCurrent = (i == 0);

        if (isCurrent)
        {
            g.setColour (theme.accent.withAlpha (0.12f));
            g.fillRoundedRectangle (rowArea.toFloat(), 4.0f);
        }

        g.setColour (theme.dimText.withAlpha (0.5f));
        g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
        g.drawText ("#" + juce::String (i + 1),
                    rowArea.removeFromLeft (28),
                    juce::Justification::centredLeft, false);

        g.setColour (isCurrent ? theme.accent : theme.text.withAlpha (0.75f));
        g.setFont (juce::Font (juce::FontOptions (isCurrent ? 16.0f : 14.0f)
                                .withStyle (isCurrent ? "Bold" : "Regular")));
        g.drawText (cachedHistory[i], rowArea,
                    juce::Justification::centredLeft, false);
    }
}

//==============================================================================
void MidiHarmonicHUDEditor::drawDiatonicPanel (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (theme.panel);
    g.fillRoundedRectangle (area.toFloat(), 12.0f);
    g.setColour (theme.panelStroke);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 12.0f, 1.0f);

    auto inner = area.reduced (14);

    g.setColour (theme.dimText);
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText ("DIATONIC CONTEXT", inner.removeFromTop (16),
                juce::Justification::topLeft, false);

    inner.removeFromTop (6);

    auto keyRow = inner.removeFromTop (30);
    g.setColour (theme.dimText);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("Key:", keyRow.removeFromLeft (40),
                juce::Justification::centredLeft, false);

    g.setColour (theme.text);
    g.setFont (juce::Font (juce::FontOptions (16.0f).withStyle ("Bold")));
    g.drawText (cachedKeyText, keyRow.removeFromLeft (140),
                juce::Justification::centredLeft, false);

    if (cachedKeyConfidence > 0.01f)
    {
        juce::String confStr = juce::String (juce::roundToInt (cachedKeyConfidence * 100.0f)) + "%";
        g.setColour (theme.accent.withAlpha (0.75f));
        g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
        g.drawText (confStr, keyRow.removeFromLeft (50),
                    juce::Justification::centredLeft, false);
    }

    inner.removeFromTop (6);

    static const int majorScale[7] = { 0, 2, 4, 5, 7, 9, 11 };
    static const int minorScale[7] = { 0, 2, 3, 5, 7, 8, 10 };

    int tonic = processorRef.detectedKeyTonic.load();

    if (tonic >= 0 && cachedActiveCount > 0)
    {
        bool isMinor = processorRef.detectedKeyIsMinor.load();
        const int* scale = isMinor ? minorScale : majorScale;

        auto scaleRow = inner.removeFromTop (30);
        float cellW = (float) scaleRow.getWidth() / 7.0f;

        for (int deg = 0; deg < 7; ++deg)
        {
            int pc = (tonic + scale[deg]) % 12;
            juce::Rectangle<float> cell ((float) scaleRow.getX() + deg * cellW,
                                          (float) scaleRow.getY(),
                                          cellW - 4.0f,
                                          (float) scaleRow.getHeight());

            g.setColour (theme.panelStroke);
            g.fillRoundedRectangle (cell, 4.0f);

            g.setColour (theme.text.withAlpha (0.85f));
            g.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
            g.drawText (kPCNames[pc], cell.toNearestInt(),
                        juce::Justification::centred, false);
        }
    }
    else
    {
        g.setColour (theme.dimText.withAlpha (0.5f));
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText ("Play notes to detect key...", inner,
                    juce::Justification::centredLeft, false);
    }
}

//==============================================================================
void MidiHarmonicHUDEditor::drawTensionGraph (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (theme.panel);
    g.fillRoundedRectangle (area.toFloat(), 12.0f);
    g.setColour (theme.panelStroke);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 12.0f, 1.0f);

    auto inner = area.reduced (14);

    g.setColour (theme.dimText);
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText ("TENSION HISTORY", inner.removeFromTop (16),
                juce::Justification::topLeft, false);

    inner.removeFromTop (4);

    auto graphArea = inner.toFloat();

    g.setColour (theme.bg.darker (0.3f));
    g.fillRoundedRectangle (graphArea, 4.0f);

    g.setColour (juce::Colour (0xffffffff).withAlpha (0.04f));
    for (int i = 1; i < 4; ++i)
    {
        float y = graphArea.getY() + graphArea.getHeight() * (float) i / 4.0f;
        g.drawHorizontalLine ((int) y, graphArea.getX(), graphArea.getRight());
    }

    const int size = MidiHarmonicHUDProcessor::TENSION_HISTORY_SIZE;
    int writePos = processorRef.tensionHistoryWritePos.load();

    juce::Path path;
    bool started = false;

    for (int i = 0; i < size; ++i)
    {
        int idx = (writePos + i) % size;
        float v = processorRef.tensionHistory[static_cast<size_t> (idx)].load();
        float x = graphArea.getX() + graphArea.getWidth() * ((float) i / (float) (size - 1));
        float y = graphArea.getBottom() - graphArea.getHeight() * juce::jlimit (0.0f, 1.0f, v);

        if (! started) { path.startNewSubPath (x, y); started = true; }
        else           { path.lineTo (x, y); }
    }

    if (started)
    {
        juce::Path fillPath = path;
        fillPath.lineTo (graphArea.getRight(), graphArea.getBottom());
        fillPath.lineTo (graphArea.getX(), graphArea.getBottom());
        fillPath.closeSubPath();

        juce::ColourGradient grad (theme.accent.withAlpha (0.35f), graphArea.getX(), graphArea.getY(),
                                    theme.accent.withAlpha (0.02f), graphArea.getX(), graphArea.getBottom(), false);
        g.setGradientFill (grad);
        g.fillPath (fillPath);

        g.setColour (theme.accent);
        g.strokePath (path, juce::PathStrokeType (1.5f));
    }
}

//==============================================================================
void MidiHarmonicHUDEditor::resized()
{
    // Mute Button (esquina superior derecha)
    muteButton.setBounds (getWidth() - 130, 16, 118, 26);

    // ⚠️ FIX: combos más anchos (80 px en vez de 66) para que no se trunquen
    presetLabel.setBounds (getWidth() - 130, 50, 34, 20);
    presetCombo.setBounds (getWidth() - 92, 48, 80, 24);

    themeLabel.setBounds (getWidth() - 130, 78, 34, 20);
    themeCombo.setBounds (getWidth() - 92, 76, 80, 24);
}

//==============================================================================
void MidiHarmonicHUDEditor::timerCallback()
{
    // Sincronizar tema si el host cambió el parámetro
    int themeId = 0;
    if (auto* p = processorRef.apvts.getRawParameterValue (ParamIDs::themeIndex))
        themeId = (int) p->load();

    if (themeId != cachedThemeId)
    {
        cachedThemeId = themeId;
        theme = ThemeManager::getTheme (themeId);
        themeCombo.setSelectedId (themeId + 1, juce::dontSendNotification);

        muteButton.setColour (juce::ToggleButton::textColourId, theme.text);
        muteButton.setColour (juce::ToggleButton::tickColourId, theme.accent);
        presetLabel.setColour (juce::Label::textColourId, theme.dimText);
        themeLabel.setColour (juce::Label::textColourId, theme.dimText);
    }

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
        cachedActiveNotes[static_cast<size_t> (i)] = processorRef.activeMidiNotes[i].load();
        if (cachedActiveNotes[static_cast<size_t> (i)]) ++count;
    }
    cachedActiveCount = count;

    cachedConfidence    = processorRef.chordConfidence.load();
    cachedTension       = processorRef.harmonicTension.load();
    cachedRootPC        = processorRef.currentRootPC.load();
    cachedBassPC        = processorRef.currentBassPC.load();
    cachedInversion     = processorRef.currentInversion.load();
    cachedKeyConfidence = processorRef.detectedKeyConfidence.load();

    confidenceSmooth += (cachedConfidence - confidenceSmooth) * 0.20f;
    tensionSmooth    += (cachedTension    - tensionSmooth)    * 0.15f;

    juce::uint32 now = juce::Time::getMillisecondCounter();
    if (cachedChord != lastDisplayedChord)
    {
        lastDisplayedChord = cachedChord;
        chordChangeTimeMs = now;
    }
    float elapsed = (float) (now - chordChangeTimeMs) / 350.0f;
    chordFadeAlpha = juce::jlimit (0.0f, 1.0f, elapsed);

    if (cachedRootPC >= 0)
    {
        for (int i = 0; i < 12; ++i)
        {
            if (kFifthOrderPC[i] == cachedRootPC)
            {
                targetRotation = -juce::MathConstants<float>::twoPi * ((float) i / 12.0f);
                break;
            }
        }
    }
    circleRotation += (targetRotation - circleRotation) * 0.08f;

    circleGlowPhase += 0.05f;
    if (circleGlowPhase > juce::MathConstants<float>::twoPi)
        circleGlowPhase -= juce::MathConstants<float>::twoPi;

    repaint();
}
