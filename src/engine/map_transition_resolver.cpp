// SPDX-License-Identifier: MIT
#include "map_transition_resolver.hpp"

#include <string>

#include <cctype>

#include "../game/game_world.hpp"

namespace runeharbor::engine
{
namespace
{
std::string toLowerCopy(std::string_view text)
{
    std::string lowered(text);
    for (char& c : lowered)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lowered;
}

std::string trimCopy(std::string_view text)
{
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])))
    {
        begin++;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])))
    {
        end--;
    }
    return std::string(text.substr(begin, end - begin));
}

std::string basenameLowerCopy(std::string_view mapName)
{
    std::string lowered = toLowerCopy(trimCopy(mapName));
    for (char& c : lowered)
    {
        if (c == '\\')
        {
            c = '/';
        }
    }
    if (const size_t slash = lowered.find_last_of('/'); slash != std::string::npos)
    {
        lowered = lowered.substr(slash + 1);
    }
    if (lowered == "pcout01")
    {
        return "out01.odm";
    }
    return lowered;
}

std::string ensureMapExtension(std::string baseName)
{
    if (baseName.size() >= 4)
    {
        const std::string ext = baseName.substr(baseName.size() - 4);
        if (ext == ".blv" || ext == ".odm")
        {
            return baseName;
        }
    }

    if (baseName.rfind("out", 0) == 0 || baseName.rfind("i", 0) == 0)
    {
        baseName += ".odm";
    }
    else
    {
        baseName += ".blv";
    }
    return baseName;
}

bool mapNamesMatch(std::string_view a, std::string_view b)
{
    const std::string baseA = basenameLowerCopy(a);
    const std::string baseB = basenameLowerCopy(b);
    if (baseA.empty() || baseB.empty())
    {
        return false;
    }
    if (baseA == baseB)
    {
        return true;
    }

    return ensureMapExtension(baseA) == ensureMapExtension(baseB);
}
} // namespace

const game::MapTransition* resolveInteractionTransition(const game::GameWorld* world,
                                                        std::string_view normalizedTargetMap)
{
    if (!world || normalizedTargetMap.empty())
    {
        return nullptr;
    }

    const auto interaction = world->lastEventInteraction();
    if (interaction.eventId <= 0)
    {
        return nullptr;
    }

    const game::MapTransition* transition = world->getTransition(interaction.eventId);
    if (!transition || transition->targetMap.empty())
    {
        return nullptr;
    }

    if (!mapNamesMatch(normalizedTargetMap, transition->targetMap))
    {
        return nullptr;
    }

    return transition;
}

int resolveSpawnIndexFromDirection(int direction)
{
    if (direction >= 0 && direction <= 4)
    {
        return direction;
    }

    switch (direction)
    {
    case 'N':
    case 'n':
        return 1;
    case 'S':
    case 's':
        return 2;
    case 'E':
    case 'e':
        return 3;
    case 'W':
    case 'w':
        return 4;
    default:
        break;
    }
    return 0;
}

std::optional<float> directionToEntryYaw(int direction)
{
    // Face into the map when arriving from an edge transition. Values are in
    // MM7 turn-units (0..2047 = full circle), matching how Party stores yaw and
    // how the camera/HUD/minimap consume it. (Previously returned degrees, which
    // was misread as turn-units and produced a wrong facing on arrival.)
    switch (resolveSpawnIndexFromDirection(direction))
    {
    case 1: // North edge -> face South (half circle)
        return 1024.0f;
    case 2: // South edge -> face North (zero)
        return 0.0f;
    case 3: // East edge -> face West (three-quarter circle)
        return 1536.0f;
    case 4: // West edge -> face East (quarter circle)
        return 512.0f;
    default:
        return std::nullopt;
    }
}

} // namespace runeharbor::engine
