#pragma once

#include <string>
#include <vector>
#include <map>

namespace ModelKeys
{
    struct Preset
    {
        std::string name;
        std::string category;
        std::map<std::string, float> parameters;
    };

    class FactoryPresets
    {
    public:
        static std::vector<Preset> getAll();
    };
}
