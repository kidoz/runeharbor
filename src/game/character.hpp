// SPDX-License-Identifier: MIT
#pragma once

#include <string>

namespace runeharbor::game
{

enum class CharacterClass
{
    Knight,
    Thief,
    Monk,
    // ... other classes
};

struct CharacterStats
{
    int might = 15;
    int intellect = 15;
    int personality = 15;
    int endurance = 15;
    int speed = 15;
    int accuracy = 15;
    int luck = 15;
};

class Character
{
public:
    std::string name;
    CharacterClass char_class;
    CharacterStats stats;
    // ... other attributes like skills, inventory, etc.
};

} // namespace runeharbor::game
