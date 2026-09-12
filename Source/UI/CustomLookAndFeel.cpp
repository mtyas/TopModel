#include "CustomLookAndFeel.h"

namespace ModelKeys
{
    const juce::Colour CustomLookAndFeel::bgDark        = juce::Colour(0xff0d0f13);
    const juce::Colour CustomLookAndFeel::bgPanel       = juce::Colour(0xff14171d);
    const juce::Colour CustomLookAndFeel::bgCard        = juce::Colour(0xff1b2028);
    const juce::Colour CustomLookAndFeel::borderSubtle  = juce::Colour(0xff2a313d);
    const juce::Colour CustomLookAndFeel::accentAmber   = juce::Colour(0xffff9d00);
    const juce::Colour CustomLookAndFeel::accentCyan    = juce::Colour(0xff00d5ff);
    const juce::Colour CustomLookAndFeel::accentOrange  = juce::Colour(0xffff5500);
    const juce::Colour CustomLookAndFeel::textBright    = juce::Colour(0xffe6edf3);
    const juce::Colour CustomLookAndFeel::textDimmed    = juce::Colour(0xff8b949e);

    CustomLookAndFeel::CustomLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, bgDark);
        setColour(juce::PopupMenu::backgroundColourId, bgCard);
        setColour(juce::PopupMenu::textColourId, textBright);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, accentAmber.withAlpha(0.35f));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::ComboBox::backgroundColourId, bgCard);
        setColour(juce::ComboBox::textColourId, textBright);
        setColour(juce::ComboBox::outlineColourId, borderSubtle);
        setColour(juce::ComboBox::arrowColourId, textDimmed);
        setColour(juce::TextButton::textColourOffId, textBright);
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }

    void CustomLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPosProportional, float rotaryStartAngle,
                                             float rotaryEndAngle, juce::Slider& slider)
    {
        const float radius = static_cast<float>(std::min(width, height)) * 0.46f;
        const float centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const float centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const float rx = centreX - radius;
        const float ry = centreY - radius;
        const float rw = radius * 2.0f;
        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // 1. Background Track
        const float trackWidth = 3.5f;
        juce::Path bgTrack;
        bgTrack.addCentredArc(centreX, centreY, radius - trackWidth * 0.5f, radius - trackWidth * 0.5f,
                              0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff222731));
        g.strokePath(bgTrack, juce::PathStrokeType(trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 2. Active Value Arc (glowing amber/cyan gradient)
        if (sliderPosProportional > 0.001f)
        {
            juce::Path activeTrack;
            activeTrack.addCentredArc(centreX, centreY, radius - trackWidth * 0.5f, radius - trackWidth * 0.5f,
                                     0.0f, rotaryStartAngle, angle, true);

            juce::Colour arcColor = accentAmber;
            if (slider.getName().containsIgnoreCase("bow") || slider.getName().containsIgnoreCase("decay") || slider.getName().containsIgnoreCase("reverb"))
                arcColor = accentCyan;

            g.setColour(arcColor);
            g.strokePath(activeTrack, juce::PathStrokeType(trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 3. Dial Center Circle with metallic gradient & drop shadow
        const float dialRadius = radius - 8.0f;
        if (dialRadius > 4.0f)
        {
            g.setColour(juce::Colour(0x66000000));
            g.fillEllipse(centreX - dialRadius + 1.0f, centreY - dialRadius + 2.0f, dialRadius * 2.0f, dialRadius * 2.0f);

            juce::ColourGradient dialGrad(juce::Colour(0xff2a303c), centreX - dialRadius, centreY - dialRadius,
                                          juce::Colour(0xff161920), centreX + dialRadius, centreY + dialRadius, false);
            g.setGradientFill(dialGrad);
            g.fillEllipse(centreX - dialRadius, centreY - dialRadius, dialRadius * 2.0f, dialRadius * 2.0f);

            // Rim
            g.setColour(borderSubtle);
            g.drawEllipse(centreX - dialRadius, centreY - dialRadius, dialRadius * 2.0f, dialRadius * 2.0f, 1.2f);

            // 4. Indicator Needle
            juce::Path p;
            const float pointerLength = dialRadius * 0.75f;
            p.startNewSubPath(centreX + std::sin(angle) * (dialRadius * 0.25f),
                              centreY - std::cos(angle) * (dialRadius * 0.25f));
            p.lineTo(centreX + std::sin(angle) * pointerLength,
                     centreY - std::cos(angle) * pointerLength);

            g.setColour(accentAmber);
            g.strokePath(p, juce::PathStrokeType(2.5f, juce::PathStrokeType::beveled, juce::PathStrokeType::rounded));
        }
    }

    void CustomLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                 const juce::Colour& backgroundColour,
                                                 bool shouldDrawButtonAsHighlighted,
                                                 bool shouldDrawButtonAsDown)
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const float cornerSize = 4.0f;

        juce::Colour baseColor = bgCard;
        if (button.getToggleState())
        {
            baseColor = accentAmber.withAlpha(0.25f);
        }
        else if (shouldDrawButtonAsDown)
        {
            baseColor = bgCard.brighter(0.2f);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            baseColor = bgCard.brighter(0.08f);
        }

        g.setColour(baseColor);
        g.fillRoundedRectangle(bounds, cornerSize);

        // Border
        juce::Colour borderCol = button.getToggleState() ? accentAmber : (shouldDrawButtonAsHighlighted ? accentAmber.withAlpha(0.6f) : borderSubtle);
        g.setColour(borderCol);
        g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
    }

    void CustomLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                         int buttonX, int buttonY, int buttonW, int buttonH,
                                         juce::ComboBox& box)
    {
        juce::ignoreUnused(isButtonDown, buttonX, buttonY, buttonW, buttonH);
        auto bounds = box.getLocalBounds().toFloat().reduced(0.5f);

        g.setColour(bgCard);
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(borderSubtle);
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        // Dropdown triangle arrow
        juce::Path arrow;
        const float arrowX = static_cast<float>(width) - 16.0f;
        const float arrowY = static_cast<float>(height) * 0.5f;
        arrow.startNewSubPath(arrowX - 4.0f, arrowY - 2.0f);
        arrow.lineTo(arrowX + 4.0f, arrowY - 2.0f);
        arrow.lineTo(arrowX, arrowY + 3.0f);
        arrow.closeSubPath();

        g.setColour(textDimmed);
        g.fillPath(arrow);
    }

    void CustomLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
    {
        g.setColour(bgCard);
        g.fillRect(0, 0, width, height);

        g.setColour(borderSubtle.brighter(0.2f));
        g.drawRect(0, 0, width, height, 1);
    }

    void CustomLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                              bool isSeparator, bool isActive, bool isHighlighted,
                                              bool isTicked, bool hasSubMenu, const juce::String& text,
                                              const juce::String& shortcutKeyText,
                                              const juce::Drawable* icon, const juce::Colour* textColour)
    {
        juce::ignoreUnused(hasSubMenu, shortcutKeyText, icon);

        if (isSeparator)
        {
            auto r = area.reduced(5, 0);
            g.setColour(borderSubtle);
            g.fillRect(r.removeFromTop(1));
            return;
        }

        if (isHighlighted && isActive)
        {
            g.setColour(accentAmber.withAlpha(0.2f));
            g.fillRect(area);
        }

        g.setColour(isHighlighted ? juce::Colours::white : (isActive ? textBright : textDimmed));
        g.setFont(juce::Font(13.0f));

        auto textBounds = area.reduced(10, 0);
        g.drawFittedText(text, textBounds, juce::Justification::centredLeft, 1);

        if (isTicked)
        {
            auto itemArea = area;
            auto tickArea = itemArea.removeFromRight(22).toFloat();
            const float cx = tickArea.getCentreX();
            const float cy = tickArea.getCentreY();
            juce::Path tick;
            tick.startNewSubPath(cx - 4.5f, cy);
            tick.lineTo(cx - 1.5f, cy + 3.5f);
            tick.lineTo(cx + 4.5f, cy - 3.5f);
            g.setColour(accentAmber);
            g.strokePath(tick, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
}
