//==============================================================================
MidiHarmonicHUDEditor::MidiHarmonicHUDEditor (MidiHarmonicHUDProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      // ⚠️ Los attachments van DESPUÉS de los controles en la clase,
      //    así que aquí ya están construidos. Los inicializamos aquí con nullptr
      //    y los enganchamos en el cuerpo del constructor.
      muteAttachment (p.apvts, ParamIDs::muteSynth, muteButton),
      presetAttachment (p.apvts, ParamIDs::presetIndex, presetCombo),
      themeAttachment (p.apvts, ParamIDs::themeIndex, themeCombo)
{
    setSize (780, 800);
    setResizable (false, false);

    // Leer tema inicial (con protección contra nullptr)
    if (auto* param = processorRef.apvts.getRawParameterValue (ParamIDs::themeIndex))
        cachedThemeId = (int) param->load();

    theme = ThemeManager::getTheme (cachedThemeId);

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
    themeCombo.onChange = [this]()
    {
        int id = themeCombo.getSelectedId() - 1;
        theme = ThemeManager::getTheme (id);
        repaint();
    };
    addAndMakeVisible (themeCombo);

    startTimerHz (60);
}

//==============================================================================
void MidiHarmonicHUDEditor::paint (juce::Graphics& g)
{
    // Refrescar tema si cambió (leído en timerCallback, no aquí)
    if (themeCombo.getSelectedId() - 1 != cachedThemeId)
    {
        theme = ThemeManager::getTheme (cachedThemeId);
        themeCombo.setSelectedId (cachedThemeId + 1, juce::dontSendNotification);
    }

    // Fondo
    juce::ColourGradient bgGrad (theme.bg2, getWidth() * 0.5f, 0.0f,
                                  theme.bg,  getWidth() * 0.5f, (float) getHeight(), true);
    g.setGradientFill (bgGrad);
    g.fillAll();

    // Grid sutil
    g.setColour (juce::Colour (0xffffffff).withAlpha (0.012f));
    for (int x = 0; x < getWidth(); x += 20)
        g.drawVerticalLine (x, 0.0f, (float) getHeight());
    for (int y = 0; y < getHeight(); y += 20)
        g.drawHorizontalLine (y, 0.0f, (float) getWidth());

    // Layout
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
