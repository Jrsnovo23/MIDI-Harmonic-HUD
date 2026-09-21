#include "ThemeManager.h"

//==============================================================================
ThemeColors ThemeManager::getTheme (int id)
{
    id = clampThemeId (id);

    switch (id)
    {
        //==========================================================
        case TokyoNight:
            return {
                "Tokyo Night",
                juce::Colour (0xff0f0f19),
                juce::Colour (0xff181828),
                juce::Colour (0xff1e1e30),
                juce::Colour (0xff2a2a45),
                juce::Colour (0xff7aa2f7),
                juce::Colour (0xffbb9af7),
                juce::Colour (0xff9ece6a),
                juce::Colour (0xfff7768e),
                juce::Colour (0xffe0af68),
                juce::Colour (0xffe6e6f0),
                juce::Colour (0xff6a6a88)
            };

        //==========================================================
        case Dracula:
            return {
                "Dracula",
                juce::Colour (0xff1e1f29),
                juce::Colour (0xff282a36),
                juce::Colour (0xff343746),
                juce::Colour (0xff44475a),
                juce::Colour (0xffbd93f9),
                juce::Colour (0xffff79c6),
                juce::Colour (0xff50fa7b),
                juce::Colour (0xffff5555),
                juce::Colour (0xfff1fa8c),
                juce::Colour (0xfff8f8f2),
                juce::Colour (0xff6272a4)
            };

        //==========================================================
        case Nord:
            return {
                "Nord",
                juce::Colour (0xff2e3440),
                juce::Colour (0xff3b4252),
                juce::Colour (0xff434c5e),
                juce::Colour (0xff4c566a),
                juce::Colour (0xff88c0d0),
                juce::Colour (0xffb48ead),
                juce::Colour (0xffa3be8c),
                juce::Colour (0xffbf616a),
                juce::Colour (0xffebcb8b),
                juce::Colour (0xffeceff4),
                juce::Colour (0xff7a8a9c)
            };

        //==========================================================
        case Gruvbox:
            return {
                "Gruvbox",
                juce::Colour (0xff1d2021),
                juce::Colour (0xff282828),
                juce::Colour (0xff32302f),
                juce::Colour (0xff504945),
                juce::Colour (0xff83a598),
                juce::Colour (0xffd3869b),
                juce::Colour (0xffb8bb26),
                juce::Colour (0xfffb4934),
                juce::Colour (0xfffabd2f),
                juce::Colour (0xffebdbb2),
                juce::Colour (0xff928374)
            };

        //==========================================================
        case OneDark:
        default:
            return {
                "One Dark",
                juce::Colour (0xff1e2127),
                juce::Colour (0xff282c34),
                juce::Colour (0xff2f333d),
                juce::Colour (0xff3e4451),
                juce::Colour (0xff61afef),
                juce::Colour (0xffc678dd),
                juce::Colour (0xff98c379),
                juce::Colour (0xffe06c75),
                juce::Colour (0xffe5c07b),
                juce::Colour (0xffabb2bf),
                juce::Colour (0xff5c6370)
            };
    }
}

//==============================================================================
juce::StringArray ThemeManager::getThemeNames()
{
    return { "Tokyo Night", "Dracula", "Nord", "Gruvbox", "One Dark" };
}
