#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <string>
#include <mutex>

namespace ModelKeys
{
    class MidiLearnManager
    {
    public:
        MidiLearnManager(juce::AudioProcessorValueTreeState& state)
            : apvts(state)
        {
            loadMappings();
        }

        ~MidiLearnManager()
        {
            saveMappings();
        }

        void setLearnActive(bool active)
        {
            isLearnActive = active;
        }

        bool getLearnActive() const noexcept
        {
            return isLearnActive;
        }

        void setTargetParameter(const std::string& paramId)
        {
            std::lock_guard<std::mutex> lock(mutex);
            targetParamId = paramId;
        }

        std::string getTargetParameter() const
        {
            std::lock_guard<std::mutex> lock(mutex);
            return targetParamId;
        }

        int getCCForParameter(const std::string& paramId) const
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (int cc = 0; cc < 128; ++cc)
            {
                if (ccMap[cc] == paramId)
                    return cc;
            }
            return -1;
        }

        void assignCC(int cc, const std::string& paramId)
        {
            if (cc < 0 || cc >= 128) return;
            std::lock_guard<std::mutex> lock(mutex);
            // Clear any previous mapping for this param
            for (int c = 0; c < 128; ++c)
            {
                if (ccMap[c] == paramId)
                    ccMap[c].clear();
            }
            ccMap[cc] = paramId;
            saveMappings();
        }

        void clearCC(int cc)
        {
            if (cc >= 0 && cc < 128)
            {
                std::lock_guard<std::mutex> lock(mutex);
                ccMap[cc].clear();
                saveMappings();
            }
        }

        void clearParameter(const std::string& paramId)
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (int cc = 0; cc < 128; ++cc)
            {
                if (ccMap[cc] == paramId)
                    ccMap[cc].clear();
            }
            saveMappings();
        }

        // Called from processBlock with MIDI messages
        bool handleMidiMessage(const juce::MidiMessage& msg)
        {
            if (!msg.isController())
                return false;

            const int cc = msg.getControllerNumber();
            const float normVal = static_cast<float>(msg.getControllerValue()) / 127.0f;

            if (isLearnActive)
            {
                std::string target;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    target = targetParamId;
                }

                if (!target.empty())
                {
                    assignCC(cc, target);
                    isLearnActive = false;
                    return true;
                }
            }

            // Normal CC dispatch
            std::string mappedParam;
            {
                std::lock_guard<std::mutex> lock(mutex);
                mappedParam = ccMap[cc];
            }

            if (!mappedParam.empty())
            {
                if (auto* param = apvts.getParameter(mappedParam))
                {
                    param->setValueNotifyingHost(normVal);
                    return true;
                }
            }

            return false;
        }

    private:
        juce::File getConfigFile()
        {
            auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
            auto topModelDir = base.getChildFile("TopModel");
            if (topModelDir.exists())
                return topModelDir.getChildFile("midi_mappings.xml");
            auto legacyDir = base.getChildFile("ModelKeys");
            if (legacyDir.getChildFile("midi_mappings.xml").existsAsFile())
                return legacyDir.getChildFile("midi_mappings.xml");
            return topModelDir.getChildFile("midi_mappings.xml");
        }

        void saveMappings()
        {
            auto file = getConfigFile();
            if (!file.getParentDirectory().exists())
                file.getParentDirectory().createDirectory();

            auto xml = std::make_unique<juce::XmlElement>("TopModelMidiMappings");
            for (int cc = 0; cc < 128; ++cc)
            {
                if (!ccMap[cc].empty())
                {
                    auto* item = xml->createNewChildElement("Mapping");
                    item->setAttribute("cc", cc);
                    item->setAttribute("param", juce::String(ccMap[cc]));
                }
            }
            xml->writeTo(file);
        }

        void loadMappings()
        {
            auto file = getConfigFile();
            if (!file.existsAsFile())
                return;

            auto xml = juce::parseXML(file);
            if (xml != nullptr && (xml->hasTagName("TopModelMidiMappings") || xml->hasTagName("ModelKeysMidiMappings")))
            {
                std::lock_guard<std::mutex> lock(mutex);
                for (auto* child : xml->getChildIterator())
                {
                    if (child->hasTagName("Mapping"))
                    {
                        int cc = child->getIntAttribute("cc", -1);
                        auto param = child->getStringAttribute("param").toStdString();
                        if (cc >= 0 && cc < 128 && !param.empty())
                        {
                            ccMap[cc] = param;
                        }
                    }
                }
            }
        }

        juce::AudioProcessorValueTreeState& apvts;
        mutable std::mutex mutex;
        std::atomic<bool> isLearnActive { false };
        std::string targetParamId;
        std::array<std::string, 128> ccMap;
    };
}
