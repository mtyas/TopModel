#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../CustomLookAndFeel.h"
#include "../Controls/CustomRotarySlider.h"
#include "../../PluginProcessor.h"

namespace ModelKeys
{
    class HeaderBar : public juce::Component,
                      public PresetManager::Listener,
                      private juce::Timer
    {
    public:
        HeaderBar(PluginProcessor& p)
            : processor(p),
              masterSlider("masterGain", "MASTER", p.getMidiLearnManager())
        {
            // Title
            addAndMakeVisible(masterSlider);
            masterAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.getAPVTS(), "masterGain", masterSlider);

            // Preset Controls
            addAndMakeVisible(categoryBox);
            categoryBox.onChange = [this]() { onCategoryChanged(); };

            addAndMakeVisible(presetBox);
            presetBox.onChange = [this]() { onPresetChanged(); };

            addAndMakeVisible(prevBtn);
            prevBtn.setButtonText("<");
            prevBtn.onClick = [this]()
            {
                processor.getPresetManager().prevPreset(categoryBox.getText().toStdString());
            };

            addAndMakeVisible(nextBtn);
            nextBtn.setButtonText(">");
            nextBtn.onClick = [this]()
            {
                processor.getPresetManager().nextPreset(categoryBox.getText().toStdString());
            };

            addAndMakeVisible(saveBtn);
            saveBtn.setButtonText("SAVE");
            saveBtn.onClick = [this]() { promptSavePreset(); };

            addAndMakeVisible(eraseBtn);
            eraseBtn.setButtonText("DEL");
            eraseBtn.onClick = [this]() { promptDeletePreset(); };

            // Undo / Redo
            addAndMakeVisible(undoBtn);
            undoBtn.setButtonText("UNDO");
            undoBtn.onClick = [this]() { processor.getUndoManager().undo(); };

            addAndMakeVisible(redoBtn);
            redoBtn.setButtonText("REDO");
            redoBtn.onClick = [this]() { processor.getUndoManager().redo(); };

            // MIDI Learn Toggle
            addAndMakeVisible(learnBtn);
            learnBtn.setButtonText("MIDI LEARN");
            learnBtn.setClickingTogglesState(true);
            learnBtn.onClick = [this]()
            {
                processor.getMidiLearnManager().setLearnActive(learnBtn.getToggleState());
            };

            // Polyphony Selector
            addAndMakeVisible(polyBox);
            polyBox.addItem("Mono", 1);
            polyBox.addItem("2 Voices", 2);
            polyBox.addItem("4 Voices", 4);
            polyBox.addItem("8 Voices", 8);
            polyBox.addItem("12 Voices", 12);
            polyBox.addItem("16 Voices", 16);
            polyBox.setSelectedId(16, juce::dontSendNotification);
            polyBox.onChange = [this]()
            {
                if (auto* param = processor.getAPVTS().getParameter("polyphony"))
                {
                    auto range = processor.getAPVTS().getParameterRange("polyphony");
                    param->setValueNotifyingHost(range.convertTo0to1(static_cast<float>(polyBox.getSelectedId())));
                }
            };

            processor.getPresetManager().addListener(this);
            updatePresetBoxes();

            startTimerHz(25);
        }

        ~HeaderBar() override
        {
            processor.getPresetManager().removeListener(this);
            stopTimer();
        }

        void presetChanged(const Preset& newPreset, int presetIndex) override
        {
            juce::ignoreUnused(newPreset, presetIndex);
            updatePresetSelection();
        }

        void presetListUpdated() override
        {
            updatePresetBoxes();
        }

        void timerCallback() override
        {
            // Sync Undo/Redo button states
            undoBtn.setEnabled(processor.getUndoManager().canUndo());
            redoBtn.setEnabled(processor.getUndoManager().canRedo());

            // Sync Learn button state
            if (learnBtn.getToggleState() != processor.getMidiLearnManager().getLearnActive())
                learnBtn.setToggleState(processor.getMidiLearnManager().getLearnActive(), juce::dontSendNotification);

            // Sync polyphony if changed via host automation
            const int poly = static_cast<int>(*processor.getAPVTS().getRawParameterValue("polyphony"));
            if (polyBox.getSelectedId() != poly)
                polyBox.setSelectedId(poly, juce::dontSendNotification);

            repaint();
        }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();

            // Background banner
            g.setColour(CustomLookAndFeel::bgPanel);
            g.fillRect(bounds);

            g.setColour(CustomLookAndFeel::borderSubtle);
            g.drawLine(0.0f, bounds.getBottom(), bounds.getRight(), bounds.getBottom(), 1.0f);

            // Glowing TopModel Brand Emblem
            g.setFont(juce::FontOptions(22.0f).withStyle("Bold"));
            g.setColour(CustomLookAndFeel::accentAmber);
            g.drawText("TOPMODEL", 18, 12, 150, 24, juce::Justification::centredLeft, false);

            g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
            g.setColour(CustomLookAndFeel::accentCyan);
            g.drawText("PHYSICAL MODELING SYNTHESIZER | MTYAS", 19, 36, 320, 12, juce::Justification::centredLeft, false);

            // Draw Master Peak Meters next to Master Slider
            auto meterArea = juce::Rectangle<float>(bounds.getRight() - 24.0f, 14.0f, 8.0f, bounds.getHeight() - 28.0f);
            g.setColour(CustomLookAndFeel::bgDark);
            g.fillRoundedRectangle(meterArea, 2.0f);

            const float pL = processor.getMeterLevelL();
            const float pR = processor.getMeterLevelR();
            const float maxP = std::clamp(std::max(pL, pR), 0.0f, 1.0f);
            const float fillH = meterArea.getHeight() * maxP;

            auto fillRect = meterArea.removeFromBottom(fillH);
            g.setColour(maxP > 0.95f ? juce::Colours::red : (maxP > 0.75f ? CustomLookAndFeel::accentAmber : CustomLookAndFeel::accentCyan));
            g.fillRoundedRectangle(fillRect, 2.0f);

            // Polyphony label
            g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
            g.setColour(CustomLookAndFeel::textDimmed);
            g.drawText("VOICES", polyBox.getX(), polyBox.getY() - 14, polyBox.getWidth(), 12, juce::Justification::centred, false);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(8, 4);

            // Left: Title branding takes 225px
            area.removeFromLeft(225);

            // Master volume on far right
            auto rightArea = area.removeFromRight(85);
            masterSlider.setBounds(rightArea.removeFromLeft(56).reduced(2));

            // Polyphony box
            auto polyArea = area.removeFromRight(95);
            polyBox.setBounds(polyArea.withSizeKeepingCentre(88, 26));

            // Undo / Redo buttons
            auto undoArea = area.removeFromRight(105);
            undoBtn.setBounds(undoArea.removeFromLeft(48).withSizeKeepingCentre(44, 26));
            redoBtn.setBounds(undoArea.removeFromLeft(48).withSizeKeepingCentre(44, 26));

            // MIDI Learn toggle button
            auto learnArea = area.removeFromRight(95);
            learnBtn.setBounds(learnArea.withSizeKeepingCentre(88, 26));

            // Generous spacing before preset controls
            area.removeFromLeft(12);
            area.removeFromRight(12);

            // Preset Bar in center - well spaced out and fully readable
            auto presetArea = area.reduced(4, 6);
            categoryBox.setBounds(presetArea.removeFromLeft(125).withSizeKeepingCentre(120, 26));
            presetArea.removeFromLeft(6);
            prevBtn.setBounds(presetArea.removeFromLeft(26).withSizeKeepingCentre(22, 26));
            presetArea.removeFromLeft(4);
            presetBox.setBounds(presetArea.removeFromLeft(230).withSizeKeepingCentre(224, 26));
            presetArea.removeFromLeft(4);
            nextBtn.setBounds(presetArea.removeFromLeft(26).withSizeKeepingCentre(22, 26));
            presetArea.removeFromLeft(8);
            saveBtn.setBounds(presetArea.removeFromLeft(52).withSizeKeepingCentre(48, 26));
            presetArea.removeFromLeft(6);
            eraseBtn.setBounds(presetArea.removeFromLeft(52).withSizeKeepingCentre(48, 26));
        }

    private:
        void updatePresetBoxes()
        {
            auto& pm = processor.getPresetManager();
            categoryBox.clear(juce::dontSendNotification);
            const auto& categories = pm.getCategories();
            for (int i = 0; i < static_cast<int>(categories.size()); ++i)
                categoryBox.addItem(categories[i], i + 1);
            categoryBox.setSelectedId(1, juce::dontSendNotification);

            refreshPresetBoxForCurrentCategory();
        }

        void refreshPresetBoxForCurrentCategory()
        {
            auto& pm = processor.getPresetManager();
            presetBox.clear(juce::dontSendNotification);

            const auto cat = categoryBox.getText().toStdString();
            const auto& all = pm.getAllPresets();
            int matchedSelectId = 1;

            for (int i = 0; i < static_cast<int>(all.size()); ++i)
            {
                if (cat == "All" || all[i].category == cat)
                {
                    presetBox.addItem(all[i].name, i + 1);
                    if (i == pm.getCurrentPresetIndex())
                        matchedSelectId = i + 1;
                }
            }

            presetBox.setSelectedId(matchedSelectId, juce::dontSendNotification);
        }

        void updatePresetSelection()
        {
            auto& pm = processor.getPresetManager();
            const auto* p = pm.getCurrentPreset();
            if (p != nullptr)
            {
                // Update category box if needed
                for (int i = 0; i < categoryBox.getNumItems(); ++i)
                {
                    if (categoryBox.getItemText(i) == juce::String(p->category))
                    {
                        categoryBox.setSelectedItemIndex(i, juce::dontSendNotification);
                        break;
                    }
                }
                refreshPresetBoxForCurrentCategory();
            }
        }

        void onCategoryChanged()
        {
            refreshPresetBoxForCurrentCategory();
            if (presetBox.getNumItems() > 0)
            {
                int presetIdx = presetBox.getSelectedId() - 1;
                if (presetIdx >= 0)
                    processor.getPresetManager().selectPreset(presetIdx);
            }
        }

        void onPresetChanged()
        {
            int presetIdx = presetBox.getSelectedId() - 1;
            if (presetIdx >= 0)
                processor.getPresetManager().selectPreset(presetIdx);
        }

        void promptSavePreset()
        {
            auto* currentPreset = processor.getPresetManager().getCurrentPreset();
            juce::String defaultName = currentPreset != nullptr ? juce::String(currentPreset->name) : "My Custom Patch";
            juce::String defaultCat = currentPreset != nullptr ? juce::String(currentPreset->category) : "Keys";

            auto* win = new juce::AlertWindow("Save Preset", "Save current sound as a preset:", juce::AlertWindow::QuestionIcon);
            win->addTextEditor("name", defaultName, "Preset Name:");

            juce::StringArray catList;
            for (const auto& c : processor.getPresetManager().getCategories())
            {
                if (c != "All")
                    catList.add(juce::String(c));
            }
            if (!catList.contains("User"))
                catList.add("User");

            win->addComboBox("category", catList, "Category:");
            if (auto* cb = win->getComboBoxComponent("category"))
            {
                int selectIdx = catList.indexOf(defaultCat);
                cb->setSelectedItemIndex(selectIdx >= 0 ? selectIdx : 0, juce::dontSendNotification);
            }

            win->addTextEditor("customCat", "", "Or New Category (optional):");

            win->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
            win->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

            win->enterModalState(true, juce::ModalCallbackFunction::create(
                [this, win](int result)
                {
                    if (result == 1)
                    {
                        auto name = win->getTextEditorContents("name").trim().toStdString();
                        auto chosenCat = win->getComboBoxComponent("category") != nullptr
                            ? win->getComboBoxComponent("category")->getText().trim().toStdString()
                            : std::string("User");
                        auto customCat = win->getTextEditorContents("customCat").trim().toStdString();
                        std::string finalCat = !customCat.empty() ? customCat : chosenCat;
                        if (finalCat.empty() || finalCat == "All")
                            finalCat = "User";

                        if (!name.empty())
                        {
                            processor.getPresetManager().saveUserPreset(name, finalCat);
                        }
                    }
                    delete win;
                }));
        }

        void promptDeletePreset()
        {
            auto* currentPreset = processor.getPresetManager().getCurrentPreset();
            if (currentPreset == nullptr) return;

            auto presetName = currentPreset->name;
            auto* win = new juce::AlertWindow("Delete Preset",
                                              "Are you sure you want to erase '" + juce::String(presetName) + "'?",
                                              juce::AlertWindow::WarningIcon);
            win->addButton("Delete", 1, juce::KeyPress(juce::KeyPress::returnKey));
            win->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

            win->enterModalState(true, juce::ModalCallbackFunction::create(
                [this, win, presetName](int result)
                {
                    if (result == 1)
                    {
                        processor.getPresetManager().deletePresetByName(presetName);
                    }
                    delete win;
                }));
        }

        PluginProcessor& processor;

        juce::ComboBox categoryBox;
        juce::ComboBox presetBox;
        juce::TextButton prevBtn;
        juce::TextButton nextBtn;
        juce::TextButton saveBtn;
        juce::TextButton eraseBtn;

        juce::TextButton undoBtn;
        juce::TextButton redoBtn;
        juce::TextButton learnBtn;

        juce::ComboBox polyBox;
        CustomRotarySlider masterSlider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttach;
    };
}
