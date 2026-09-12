#include "PluginEditor.h"

namespace ModelKeys
{
    PluginEditor::PluginEditor(PluginProcessor& p)
        : AudioProcessorEditor(&p),
          processor(p),
          headerBar(p),
          visualizer(p),
          exciterPanel(p),
          resonatorPanel(p),
          bodyPickupPanel(p),
          fxPanel(p),
          keyboardComponent(p.getKeyboardState())
    {
        juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);

        addAndMakeVisible(headerBar);
        addAndMakeVisible(visualizer);
        addAndMakeVisible(exciterPanel);
        addAndMakeVisible(resonatorPanel);
        addAndMakeVisible(bodyPickupPanel);
        addAndMakeVisible(fxPanel);
        addAndMakeVisible(keyboardComponent);

        setSize(1220, 760);
        setResizable(true, true);
        setResizeLimits(1040, 680, 1800, 1200);
    }

    PluginEditor::~PluginEditor()
    {
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    }

    void PluginEditor::paint(juce::Graphics& g)
    {
        g.fillAll(CustomLookAndFeel::bgDark);
    }

    void PluginEditor::resized()
    {
        auto bounds = getLocalBounds();

        // 1. Header Bar at top
        headerBar.setBounds(bounds.removeFromTop(62));

        // 2. Keyboard at bottom
        keyboardComponent.setBounds(bounds.removeFromBottom(72));

        // 3. Middle content area
        auto contentArea = bounds.reduced(8, 4);

        // Visualizer at the top of content
        visualizer.setBounds(contentArea.removeFromTop(150));
        contentArea.removeFromTop(6);

        // Split remaining height equally into 2 rows
        const int rowHeight = contentArea.getHeight() / 2;

        // Row 1: Exciter & Resonator
        auto row1 = contentArea.removeFromTop(rowHeight).reduced(0, 3);
        const int halfW = row1.getWidth() / 2;
        exciterPanel.setBounds(row1.removeFromLeft(halfW).reduced(2, 0));
        resonatorPanel.setBounds(row1.reduced(2, 0));

        // Row 2: Body/Pickup & FX
        auto row2 = contentArea.reduced(0, 3);
        bodyPickupPanel.setBounds(row2.removeFromLeft(halfW).reduced(2, 0));
        fxPanel.setBounds(row2.reduced(2, 0));
    }

    bool PluginEditor::keyPressed(const juce::KeyPress& key)
    {
        // Undo: Ctrl+Z
        if (key.getModifiers().isCommandDown() && !key.getModifiers().isShiftDown() && key.getKeyCode() == 'Z')
        {
            processor.getUndoManager().undo();
            return true;
        }

        // Redo: Ctrl+Y or Ctrl+Shift+Z
        if ((key.getModifiers().isCommandDown() && key.getKeyCode() == 'Y') ||
            (key.getModifiers().isCommandDown() && key.getModifiers().isShiftDown() && key.getKeyCode() == 'Z'))
        {
            processor.getUndoManager().redo();
            return true;
        }

        return false;
    }
}
