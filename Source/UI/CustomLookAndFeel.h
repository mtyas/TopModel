#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ModelKeys
{
    class CustomLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        CustomLookAndFeel();
        ~CustomLookAndFeel() override = default;

        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPosProportional, float rotaryStartAngle,
                              float rotaryEndAngle, juce::Slider& slider) override;

        void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                  const juce::Colour& backgroundColour,
                                  bool shouldDrawButtonAsHighlighted,
                                  bool shouldDrawButtonAsDown) override;

        void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                          int buttonX, int buttonY, int buttonW, int buttonH,
                          juce::ComboBox& box) override;

        void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;

        void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                               bool isSeparator, bool isActive, bool isHighlighted,
                               bool isTicked, bool hasSubMenu, const juce::String& text,
                               const juce::String& shortcutKeyText,
                               const juce::Drawable* icon, const juce::Colour* textColour) override;

        // Color Palette
        static const juce::Colour bgDark;
        static const juce::Colour bgPanel;
        static const juce::Colour bgCard;
        static const juce::Colour borderSubtle;
        static const juce::Colour accentAmber;
        static const juce::Colour accentCyan;
        static const juce::Colour accentOrange;
        static const juce::Colour textBright;
        static const juce::Colour textDimmed;
    };
}
