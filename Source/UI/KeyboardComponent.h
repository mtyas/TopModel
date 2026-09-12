#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "CustomLookAndFeel.h"

namespace ModelKeys
{
    class KeyboardComponent : public juce::Component
    {
    public:
        KeyboardComponent(juce::MidiKeyboardState& keyboardState)
            : keyboard(keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
        {
            // Full 88-key concert grand piano range: A0 (21) to C8 (108) = exactly 52 white keys
            keyboard.setAvailableRange(21, 108);
            keyboard.setScrollButtonsVisible(false);

            // Dark luxury keyboard styling
            keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xFFEBEAE4)); // Warm ivory
            keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xFF1A1A1E)); // Matte ebony
            keyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour(0xFF28282E));
            keyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour(0x33FFB300)); // Subtle amber glow
            keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour(0x88FFB300));     // Amber keypress
            keyboard.setColour(juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, CustomLookAndFeel::bgDark);

            addAndMakeVisible(keyboard);
        }

        ~KeyboardComponent() override = default;

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            g.setColour(CustomLookAndFeel::bgDark);
            g.fillRect(bounds);

            g.setColour(CustomLookAndFeel::borderSubtle);
            g.drawLine(0.0f, 0.0f, bounds.getRight(), 0.0f, 1.0f);
        }

        void resized() override
        {
            auto bounds = getLocalBounds();
            // 88-key piano has exactly 52 white keys (A0 to C8).
            // Dynamically scale keyWidth so keyboard stretches to fill 100% of width with zero white margin
            constexpr int numWhiteKeys = 52;
            const float keyWidth = static_cast<float>(bounds.getWidth()) / static_cast<float>(numWhiteKeys);
            keyboard.setKeyWidth(keyWidth);
            keyboard.setBounds(bounds);
        }

    private:
        juce::MidiKeyboardComponent keyboard;
    };
}
