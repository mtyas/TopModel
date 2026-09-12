#include "PresetManager.h"

namespace ModelKeys
{
    PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts, juce::UndoManager& undoManager)
        : state(apvts), undo(undoManager)
    {
    }

    void PresetManager::addListener(Listener* listener)
    {
        listeners.add(listener);
    }

    void PresetManager::removeListener(Listener* listener)
    {
        listeners.remove(listener);
    }

    void PresetManager::initialize()
    {
        refreshPresetList();
        if (!presets.empty())
        {
            selectPreset(0);
        }
    }

    juce::File PresetManager::getUserPresetDirectory()
    {
        auto baseDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
        auto dir = baseDir.getChildFile("TopModel").getChildFile("Presets");
        if (!dir.exists())
        {
            auto legacyDir = baseDir.getChildFile("ModelKeys").getChildFile("Presets");
            if (legacyDir.exists())
                return legacyDir;
            dir.createDirectory();
        }
        return dir;
    }

    juce::File PresetManager::getDeletedPresetsFile()
    {
        return getUserPresetDirectory().getChildFile("deleted_presets.txt");
    }

    void PresetManager::refreshPresetList()
    {
        // 1. Read set of erased factory preset names
        std::vector<std::string> deletedPresets;
        auto delFile = getDeletedPresetsFile();
        if (delFile.existsAsFile())
        {
            auto lines = juce::StringArray::fromLines(delFile.loadFileAsString());
            for (const auto& line : lines)
            {
                auto trimmed = line.trim().toStdString();
                if (!trimmed.empty())
                    deletedPresets.push_back(trimmed);
            }
        }

        // 2. Load factory presets, filtering out deleted ones
        presets.clear();
        auto allFactory = FactoryPresets::getAll();
        for (const auto& fp : allFactory)
        {
            if (std::find(deletedPresets.begin(), deletedPresets.end(), fp.name) == deletedPresets.end())
            {
                presets.push_back(fp);
            }
        }

        // 3. Load user presets from disk (overwriting factory presets with matching name)
        auto userDir = getUserPresetDirectory();
        auto presetFiles = userDir.findChildFiles(juce::File::findFiles, false, "*.xml");

        for (const auto& file : presetFiles)
        {
            auto xml = juce::parseXML(file);
            if (xml != nullptr && (xml->hasTagName("TopModelPreset") || xml->hasTagName("ModelKeysPreset")))
            {
                Preset p;
                p.name = xml->getStringAttribute("name", file.getFileNameWithoutExtension()).toStdString();
                p.category = xml->getStringAttribute("category", "User").toStdString();

                auto* paramsElem = xml->getChildByName("Parameters");
                if (paramsElem != nullptr)
                {
                    for (auto* child : paramsElem->getChildIterator())
                    {
                        if (child->hasTagName("Param"))
                        {
                            auto id = child->getStringAttribute("id").toStdString();
                            auto val = static_cast<float>(child->getDoubleAttribute("value", 0.0));
                            p.parameters[id] = val;
                        }
                    }
                }

                // Check if an existing preset has this name -> overwrite it!
                auto it = std::find_if(presets.begin(), presets.end(),
                                       [&](const Preset& ep) { return ep.name == p.name; });
                if (it != presets.end())
                {
                    *it = p; // Overwrite factory or earlier preset!
                }
                else
                {
                    presets.push_back(p);
                }
            }
        }

        // 4. Rebuild categories (preserve standard order: All, Bass, Lead, Keys, Plucks, Pads, Percussion, then custom)
        categories.clear();
        categories.push_back("All");
        const std::vector<std::string> standardOrder = { "Bass", "Lead", "Keys", "Plucks", "Pads", "Percussion" };
        for (const auto& stdCat : standardOrder)
        {
            bool hasCat = false;
            for (const auto& p : presets)
            {
                if (p.category == stdCat) { hasCat = true; break; }
            }
            if (hasCat)
                categories.push_back(stdCat);
        }

        for (const auto& p : presets)
        {
            if (std::find(categories.begin(), categories.end(), p.category) == categories.end())
            {
                categories.push_back(p.category);
            }
        }

        if (currentPresetIndex >= static_cast<int>(presets.size()))
            currentPresetIndex = static_cast<int>(presets.size()) - 1;
        if (currentPresetIndex < 0 && !presets.empty())
            currentPresetIndex = 0;

        listeners.call(&Listener::presetListUpdated);
    }

    const Preset* PresetManager::getCurrentPreset() const
    {
        if (currentPresetIndex >= 0 && currentPresetIndex < static_cast<int>(presets.size()))
            return &presets[currentPresetIndex];
        return nullptr;
    }

    void PresetManager::selectPreset(int index)
    {
        if (index >= 0 && index < static_cast<int>(presets.size()))
        {
            currentPresetIndex = index;
            applyPresetToAPVTS(presets[currentPresetIndex]);
            listeners.call(&Listener::presetChanged, presets[currentPresetIndex], currentPresetIndex);
        }
    }

    void PresetManager::selectPresetByName(const std::string& name)
    {
        for (size_t i = 0; i < presets.size(); ++i)
        {
            if (presets[i].name == name)
            {
                selectPreset(static_cast<int>(i));
                return;
            }
        }
    }

    void PresetManager::nextPreset(const std::string& categoryFilter)
    {
        if (presets.empty()) return;

        int nextIdx = (currentPresetIndex + 1) % static_cast<int>(presets.size());
        if (categoryFilter != "All")
        {
            int attempts = 0;
            while (presets[nextIdx].category != categoryFilter && attempts < static_cast<int>(presets.size()))
            {
                nextIdx = (nextIdx + 1) % static_cast<int>(presets.size());
                attempts++;
            }
        }
        selectPreset(nextIdx);
    }

    void PresetManager::prevPreset(const std::string& categoryFilter)
    {
        if (presets.empty()) return;

        int prevIdx = currentPresetIndex - 1;
        if (prevIdx < 0) prevIdx = static_cast<int>(presets.size()) - 1;

        if (categoryFilter != "All")
        {
            int attempts = 0;
            while (presets[prevIdx].category != categoryFilter && attempts < static_cast<int>(presets.size()))
            {
                prevIdx = prevIdx - 1;
                if (prevIdx < 0) prevIdx = static_cast<int>(presets.size()) - 1;
                attempts++;
            }
        }
        selectPreset(prevIdx);
    }

    void PresetManager::applyPresetToAPVTS(const Preset& preset)
    {
        undo.beginNewTransaction("Load Preset: " + preset.name);

        for (const auto& [paramId, val] : preset.parameters)
        {
            if (auto* param = state.getParameter(paramId))
            {
                auto range = state.getParameterRange(paramId);
                float normalized = range.convertTo0to1(val);
                param->setValueNotifyingHost(normalized);

                auto child = state.state.getChildWithProperty("id", juce::String(paramId));
                if (child.isValid())
                {
                    child.setProperty("value", val, &undo);
                }
            }
        }
    }

    bool PresetManager::saveUserPreset(const std::string& name, const std::string& category)
    {
        if (name.empty()) return false;

        auto userDir = getUserPresetDirectory();
        auto file = userDir.getChildFile(juce::File::createLegalFileName(name) + ".xml");

        auto xml = std::make_unique<juce::XmlElement>("TopModelPreset");
        xml->setAttribute("name", name);
        xml->setAttribute("category", category.empty() ? "User" : category);

        auto* paramsElem = xml->createNewChildElement("Parameters");

        for (auto* param : state.processor.getParameters())
        {
            if (auto* pWithId = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            {
                auto* child = paramsElem->createNewChildElement("Param");
                child->setAttribute("id", pWithId->paramID);
                auto range = state.getParameterRange(pWithId->paramID);
                child->setAttribute("value", range.convertFrom0to1(param->getValue()));
            }
        }

        if (xml->writeTo(file))
        {
            // If this preset was previously marked deleted, un-delete it!
            auto delFile = getDeletedPresetsFile();
            if (delFile.existsAsFile())
            {
                auto lines = juce::StringArray::fromLines(delFile.loadFileAsString());
                juce::StringArray remaining;
                for (const auto& line : lines)
                {
                    if (line.trim().toStdString() != name && !line.trim().isEmpty())
                        remaining.add(line.trim());
                }
                delFile.replaceWithText(remaining.joinIntoString("\n"));
            }

            refreshPresetList();
            selectPresetByName(name);
            return true;
        }

        return false;
    }

    bool PresetManager::deletePreset(int index)
    {
        if (index >= 0 && index < static_cast<int>(presets.size()))
        {
            return deletePresetByName(presets[index].name);
        }
        return false;
    }

    bool PresetManager::deletePresetByName(const std::string& name)
    {
        if (name.empty()) return false;

        bool foundOrDeleted = false;

        // 1. Delete user XML file if it exists
        auto userDir = getUserPresetDirectory();
        auto file = userDir.getChildFile(juce::File::createLegalFileName(name) + ".xml");
        if (file.existsAsFile())
        {
            file.deleteFile();
            foundOrDeleted = true;
        }

        // 2. If it is in the factory presets library, record in deleted_presets.txt
        auto factory = FactoryPresets::getAll();
        bool isFactory = false;
        for (const auto& fp : factory)
        {
            if (fp.name == name)
            {
                isFactory = true;
                break;
            }
        }

        if (isFactory)
        {
            auto delFile = getDeletedPresetsFile();
            auto lines = delFile.existsAsFile() ? juce::StringArray::fromLines(delFile.loadFileAsString()) : juce::StringArray();
            bool alreadyInList = false;
            for (const auto& l : lines)
            {
                if (l.trim().toStdString() == name) { alreadyInList = true; break; }
            }
            if (!alreadyInList)
            {
                lines.add(juce::String(name));
                delFile.replaceWithText(lines.joinIntoString("\n"));
            }
            foundOrDeleted = true;
        }

        if (foundOrDeleted)
        {
            int targetIndex = std::max(0, currentPresetIndex - 1);
            refreshPresetList();
            if (!presets.empty())
                selectPreset(std::clamp(targetIndex, 0, static_cast<int>(presets.size()) - 1));
            return true;
        }

        return false;
    }

    void PresetManager::restoreFactoryPresets()
    {
        auto delFile = getDeletedPresetsFile();
        if (delFile.existsAsFile())
            delFile.deleteFile();

        refreshPresetList();
        if (!presets.empty())
            selectPreset(0);
    }
}
