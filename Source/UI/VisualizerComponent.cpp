#include "VisualizerComponent.h"
#include <cmath>
#include <algorithm>

namespace ModelKeys
{
    static inline float getModeSpatialShape(ResonatorType type, int m, float x)
    {
        // x in [0.0, 1.0] along resonator body
        switch (type)
        {
            case ResonatorType::ClampedTine:
            case ResonatorType::ClampedReed:
            {
                // Clamped at x = 0, free vibrating tip at x = 1
                if (m == 0)
                    return 1.0f - std::cos(1.5707963f * x);
                const float k = (static_cast<float>(m) + 0.5f) * 3.14159265f;
                return std::sin(k * x) * (0.25f + 0.75f * x);
            }
            case ResonatorType::FreeBar:
            {
                // Free bar with acoustic nodes at x = 0.224 and 0.776
                const float n1 = 0.224f, n2 = 0.776f;
                if (m == 0)
                    return -1.6f * (x - n1) * (x - n2);
                return std::cos(static_cast<float>(m + 1) * 3.14159265f * x);
            }
            case ResonatorType::Membrane:
            case ResonatorType::SteelPan:
            {
                // Clamped circular boundary at edges, antinode at center
                const float r = std::abs(2.0f * x - 1.0f);
                if (m == 0)
                    return std::max(0.0f, 1.0f - r * r);
                return (1.0f - r * r) * std::sin(static_cast<float>(m + 1) * 3.14159265f * x);
            }
            case ResonatorType::Plate:
            {
                return std::sin(static_cast<float>(m + 1) * 3.14159265f * x) * std::cos(static_cast<float>(m) * 1.5707963f * x);
            }
            case ResonatorType::String:
            default:
            {
                // Fixed-fixed ideal string: sin((m+1)*pi*x)
                return std::sin(static_cast<float>(m + 1) * 3.14159265f * x);
            }
        }
    }

    VisualizerComponent::VisualizerComponent(PluginProcessor& p)
        : processor(p)
    {
        startTimerHz(60);
    }

    VisualizerComponent::~VisualizerComponent()
    {
        stopTimer();
    }

    void VisualizerComponent::resized()
    {
    }

    void VisualizerComponent::timerCallback()
    {
        animPhase += 0.12f;
        if (animPhase > 2.0f * 3.14159265f)
            animPhase -= 2.0f * 3.14159265f;

        bowDrawPhase += 0.08f;
        if (bowDrawPhase > 2.0f * 3.14159265f)
            bowDrawPhase -= 2.0f * 3.14159265f;

        // 1. Scan active voices for highest vibration energy
        const auto& voices = processor.getVoiceManager().getVoices();
        const Voice* activeVoice = nullptr;
        float maxEnergy = 0.0f;
        bool anyActive = false;

        for (const auto& v : voices)
        {
            if (v.isActive())
            {
                anyActive = true;
                const float e = v.getEnergy();
                if (e > maxEnergy)
                {
                    maxEnergy = e;
                    activeVoice = &v;
                }
            }
        }

        // Detect note-on trigger for hammer strike rebound animation
        if (anyActive && !lastVoiceActive)
            impactAnim = 1.0f;
        lastVoiceActive = anyActive;

        // Smoothly decay hammer rebound
        impactAnim *= 0.82f;

        // 2. Track modal amplitude envelopes with perceptual square-root scaling
        if (activeVoice != nullptr)
        {
            smoothedEnergy = smoothedEnergy * 0.85f + maxEnergy * 0.15f;
            const auto& modes = activeVoice->getResonator().getModes();
            for (size_t m = 0; m < 8; ++m)
            {
                if (modes[m].active)
                {
                    // Exact modal phasor magnitude: sqrt(x^2 + y^2)
                    const float amp1 = std::sqrt(modes[m].x * modes[m].x + modes[m].y * modes[m].y);
                    const float amp2 = std::sqrt(modes[m].x2 * modes[m].x2 + modes[m].y2 * modes[m].y2);
                    const float totalAmp = amp1 + 0.7f * amp2;

                    // Perceptual gamma curve so decaying vibration remains clearly animated across note decay
                    const float targetVisAmp = std::clamp(std::pow(totalAmp * 45.0f, 0.52f), 0.0f, 1.25f);
                    if (targetVisAmp > smoothedAmps[m])
                        smoothedAmps[m] = targetVisAmp; // Fast attack
                    else
                        smoothedAmps[m] = smoothedAmps[m] * 0.92f + targetVisAmp * 0.08f; // Smooth musical decay
                }
                else
                {
                    smoothedAmps[m] *= 0.85f;
                }
            }
        }
        else
        {
            smoothedEnergy *= 0.80f;
            for (size_t m = 0; m < 8; ++m)
                smoothedAmps[m] *= 0.86f;
        }

        repaint();
    }

    void VisualizerComponent::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);

        // 1. Dark container background with subtle radial vignette
        g.setColour(CustomLookAndFeel::bgCard.darker(0.35f));
        g.fillRoundedRectangle(bounds, 6.0f);

        g.setColour(CustomLookAndFeel::borderSubtle);
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

        const float midY = bounds.getCentreY();
        const float spanX = bounds.getWidth() - 70.0f;
        const float startX = bounds.getX() + 35.0f;
        const float endX = startX + spanX;

        // Grid lines (studio oscilloscope reference marks)
        g.setColour(juce::Colour(0x0affffff));
        g.drawLine(startX - 10.0f, midY, endX + 10.0f, midY, 1.0f);
        for (float x = startX; x <= endX + 1.0f; x += spanX * 0.125f)
        {
            g.drawLine(x, bounds.getY() + 10.0f, x, bounds.getBottom() - 10.0f, 0.6f);
        }

        auto& apvts = processor.getAPVTS();
        const auto excType = static_cast<ExciterType>(static_cast<int>(*apvts.getRawParameterValue("exciterType")));
        const auto resType = static_cast<ResonatorType>(static_cast<int>(*apvts.getRawParameterValue("resonatorType")));
        const float contactPos = *apvts.getRawParameterValue("contactPos");
        const float pickupPos = *apvts.getRawParameterValue("pickupPos");

        // 2. Compute Standing Wave Paths (Outline & Translucent Underglow Fill)
        juce::Path wavePath;
        juce::Path fillPath;
        const int numPoints = 140;
        const float maxAmp = bounds.getHeight() * 0.36f;

        wavePath.startNewSubPath(startX, midY);
        fillPath.startNewSubPath(startX, midY);

        float waveDispAtPickup = 0.0f;

        for (int i = 0; i <= numPoints; ++i)
        {
            const float normX = static_cast<float>(i) / static_cast<float>(numPoints);
            const float px = startX + normX * spanX;
            float disp = 0.0f;

            for (int m = 0; m < 8; ++m)
            {
                if (smoothedAmps[m] > 0.001f)
                {
                    const float spatial = getModeSpatialShape(resType, m, normX);
                    const float timeOsc = std::sin(animPhase * (1.0f + 0.60f * static_cast<float>(m)) + static_cast<float>(m) * 1.35f);
                    disp += smoothedAmps[m] * spatial * timeOsc * 0.44f;
                }
            }

            disp = std::clamp(disp, -1.0f, 1.0f);
            const float py = midY - disp * maxAmp;

            if (std::abs(normX - pickupPos) < (0.5f / static_cast<float>(numPoints)))
                waveDispAtPickup = disp;

            if (i == 0)
            {
                wavePath.startNewSubPath(px, py);
                fillPath.startNewSubPath(px, py);
            }
            else
            {
                wavePath.lineTo(px, py);
                fillPath.lineTo(px, py);
            }
        }

        fillPath.lineTo(endX, midY);
        fillPath.lineTo(startX, midY);
        fillPath.closeSubPath();

        // 3. Render Standing Wave Glow & Core
        // Translucent gradient area fill
        juce::ColourGradient fillGrad(CustomLookAndFeel::accentCyan.withAlpha(0.12f), startX, midY - maxAmp * 0.5f,
                                      CustomLookAndFeel::accentCyan.withAlpha(0.01f), startX, midY + maxAmp * 0.5f, false);
        g.setGradientFill(fillGrad);
        g.fillPath(fillPath);

        // Luminous outer wave bloom
        g.setColour(CustomLookAndFeel::accentCyan.withAlpha(0.25f));
        g.strokePath(wavePath, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Crisp inner wave line
        g.setColour(CustomLookAndFeel::accentCyan.brighter(0.3f));
        g.strokePath(wavePath, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 4. Draw Resonator Mechanical Mounts
        if (resType == ResonatorType::ClampedTine || resType == ResonatorType::ClampedReed)
        {
            // Clamped Left Tonebar bracket
            g.setColour(CustomLookAndFeel::borderSubtle.brighter(0.6f));
            g.fillRect(startX - 8.0f, midY - 20.0f, 8.0f, 40.0f);
            g.setColour(CustomLookAndFeel::accentAmber);
            g.drawRect(startX - 8.0f, midY - 20.0f, 8.0f, 40.0f, 1.0f);
        }
        else
        {
            // Left & Right bridge frets
            g.setColour(CustomLookAndFeel::borderSubtle.brighter(0.4f));
            g.fillRect(startX - 5.0f, midY - 16.0f, 5.0f, 32.0f);
            g.fillRect(endX, midY - 16.0f, 5.0f, 32.0f);
        }

        // 5. Draw Contact Exciter (with Spring/Strike Rebound & Bow Motion)
        const float contactPixelX = startX + contactPos * spanX;
        float exciterOffset = 18.0f;

        if (excType == ExciterType::Strike)
        {
            // Hammer strikes down on note trigger, then springs back up
            exciterOffset = 18.0f - impactAnim * 14.0f;
        }
        else if (excType == ExciterType::Bow)
        {
            // Bow rests directly on string during active sustain
            exciterOffset = lastVoiceActive ? 5.0f : 16.0f;
        }
        else if (excType == ExciterType::Pluck)
        {
            // Plectrum deflection
            exciterOffset = 16.0f - impactAnim * 10.0f;
        }
        else // Noise
        {
            exciterOffset = 14.0f;
        }

        const float contactPixelY = midY - exciterOffset;
        const juce::Colour exciterCol = CustomLookAndFeel::accentAmber;
        g.setColour(exciterCol);

        if (excType == ExciterType::Strike)
        {
            // Piano / Marimba hammer head
            g.fillRoundedRectangle(contactPixelX - 7.0f, contactPixelY - 11.0f, 14.0f, 10.0f, 3.0f);
            g.drawLine(contactPixelX, contactPixelY - 11.0f, contactPixelX, contactPixelY - 26.0f, 2.0f);
            if (impactAnim > 0.3f)
            {
                // Impact flash spark
                g.setColour(juce::Colours::white.withAlpha(impactAnim * 0.7f));
                g.fillEllipse(contactPixelX - 4.0f, midY - 4.0f, 8.0f, 8.0f);
            }
        }
        else if (excType == ExciterType::Pluck)
        {
            // Plectrum pick triangle
            juce::Path pick;
            pick.startNewSubPath(contactPixelX, contactPixelY);
            pick.lineTo(contactPixelX - 7.0f, contactPixelY - 14.0f);
            pick.lineTo(contactPixelX + 7.0f, contactPixelY - 14.0f);
            pick.closeSubPath();
            g.fillPath(pick);
        }
        else if (excType == ExciterType::Bow)
        {
            // Bow hair cross-section with horizontal draw motion
            const float bowShift = lastVoiceActive ? std::sin(bowDrawPhase * 2.0f) * 6.0f : 0.0f;
            g.fillRoundedRectangle(contactPixelX - 3.0f + bowShift, contactPixelY - 14.0f, 6.0f, 14.0f, 2.0f);
            // Lateral bow stick-slip arrows
            g.drawLine(contactPixelX - 10.0f + bowShift, contactPixelY - 7.0f, contactPixelX + 10.0f + bowShift, contactPixelY - 7.0f, 1.5f);
            if (lastVoiceActive)
            {
                // Rosin friction shimmer spark
                g.setColour(CustomLookAndFeel::accentAmber.withAlpha(0.6f));
                g.fillEllipse(contactPixelX - 3.0f, midY - 3.0f, 6.0f, 6.0f);
            }
        }
        else // Noise particles
        {
            for (int k = -2; k <= 2; ++k)
            {
                const float pDisp = lastVoiceActive ? std::sin(animPhase * 3.0f + k * 1.5f) * 3.0f : 0.0f;
                g.fillEllipse(contactPixelX + k * 5.0f - 2.0f, contactPixelY - 8.0f + pDisp, 4.0f, 4.0f);
            }
        }

        // Contact label
        g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
        g.setColour(exciterCol);
        g.drawText("EXCITER (" + juce::String(static_cast<int>(contactPos * 100.0f)) + "%)",
                   static_cast<int>(contactPixelX) - 50, static_cast<int>(bounds.getY()) + 4, 100, 14, juce::Justification::centred, false);

        // 6. Draw Pickup Transducer Probe (with Dynamic Magnetic Flux Glow)
        const float pickupPixelX = startX + pickupPos * spanX;
        const float pickupPixelY = midY + 16.0f;

        const juce::Colour pickupCol = CustomLookAndFeel::accentOrange;
        g.setColour(pickupCol);

        // Sensor Coil / Magnet Core
        g.fillRoundedRectangle(pickupPixelX - 8.0f, pickupPixelY, 16.0f, 8.0f, 2.0f);
        g.drawLine(pickupPixelX, pickupPixelY + 8.0f, pickupPixelX, pickupPixelY + 22.0f, 2.0f);

        // Pulsing magnetic flux lines responding to passing wave displacement
        const float fluxScale = 1.0f + std::abs(waveDispAtPickup) * 0.6f;
        const float fluxAlpha = 0.25f + std::abs(waveDispAtPickup) * 0.45f;
        g.setColour(pickupCol.withAlpha(std::clamp(fluxAlpha, 0.1f, 0.85f)));
        g.drawEllipse(pickupPixelX - 12.0f * fluxScale, pickupPixelY - 8.0f, 24.0f * fluxScale, 14.0f, 1.2f);

        // Pickup label
        g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
        g.setColour(pickupCol);
        g.drawText("PICKUP (" + juce::String(static_cast<int>(pickupPos * 100.0f)) + "%)",
                   static_cast<int>(pickupPixelX) - 50, static_cast<int>(bounds.getBottom()) - 18, 100, 14, juce::Justification::centred, false);

        // 7. Geometry Mode Badge in top-left
        juce::String geoName = "STRING";
        if (resType == ResonatorType::ClampedTine) geoName = "CLAMPED TINE (RHODES)";
        else if (resType == ResonatorType::FreeBar) geoName = "FREE BAR (MARIMBA)";
        else if (resType == ResonatorType::Membrane) geoName = "CIRCULAR MEMBRANE";
        else if (resType == ResonatorType::Plate) geoName = "METALLIC PLATE";
        else if (resType == ResonatorType::ClampedReed) geoName = "CLAMPED REED (WURLI)";
        else if (resType == ResonatorType::SteelPan) geoName = "STEELPAN SHELL";

        const int activeVoices = processor.getVoiceManager().getActiveVoiceCount();
        juce::String statusText = "RESONATOR: " + geoName;
        if (activeVoices > 0)
            statusText += "  [" + juce::String(activeVoices) + " VOICES ACTIVE]";

        auto badgeRect = juce::Rectangle<float>(bounds.getX() + 8.0f, bounds.getY() + 6.0f, 220.0f, 18.0f);
        g.setColour(juce::Colour(0x22ffffff));
        g.fillRoundedRectangle(badgeRect, 3.0f);
        g.setColour(CustomLookAndFeel::textDimmed);
        g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
        g.drawFittedText(statusText, badgeRect.toNearestInt(), juce::Justification::centred, 1);
    }

    void VisualizerComponent::mouseDown(const juce::MouseEvent& e)
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        const float spanX = bounds.getWidth() - 70.0f;
        const float startX = bounds.getX() + 35.0f;

        auto& apvts = processor.getAPVTS();
        const float cPos = *apvts.getRawParameterValue("contactPos");
        const float pPos = *apvts.getRawParameterValue("pickupPos");

        const float cPx = startX + cPos * spanX;
        const float pPx = startX + pPos * spanX;

        draggingContact = (std::abs(e.position.x - cPx) < 22.0f && e.position.y < bounds.getCentreY());
        draggingPickup = (std::abs(e.position.x - pPx) < 22.0f && e.position.y >= bounds.getCentreY());
    }

    void VisualizerComponent::mouseDrag(const juce::MouseEvent& e)
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        const float spanX = bounds.getWidth() - 70.0f;
        const float startX = bounds.getX() + 35.0f;

        const float norm = std::clamp((e.position.x - startX) / spanX, 0.02f, 0.98f);
        auto& apvts = processor.getAPVTS();

        if (draggingContact)
        {
            if (auto* param = apvts.getParameter("contactPos"))
            {
                auto range = apvts.getParameterRange("contactPos");
                param->setValueNotifyingHost(range.convertTo0to1(norm));
            }
        }
        else if (draggingPickup)
        {
            if (auto* param = apvts.getParameter("pickupPos"))
            {
                auto range = apvts.getParameterRange("pickupPos");
                param->setValueNotifyingHost(range.convertTo0to1(norm));
            }
        }
    }
}

