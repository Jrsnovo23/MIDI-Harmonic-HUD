#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
struct ThemeColors
{
    juce::String name;
    juce::Colour bg;
    juce::Colour bg2;
    juce::Colour panel;
    juce::Colour panelStroke;
    juce::Colour accent;
    juce::Colour accent2;
    juce::Colour accent3;
    juce::Colour danger;
    juce::Colour warn;
    juce::Colour text;
    juce::Colour dimText;
};

//==============================================================================
class ThemeManager
{
public:
    enum ThemeId
    {
        TokyoNight = 0,
        Dracula,
        Nord,
        Gruvbox,
        OneDark,
        NumThemes
    };

    static ThemeColors getTheme (int id);
    static juce::StringArray getThemeNames();
    static int clampThemeId (int id) { return juce::jlimit (0, NumThemes - 1, id); }

    JUCE_DECLARE_NON_COPYABLE (ThemeManager)
};
