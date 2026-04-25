#pragma once

#include "Core/Property/PropertyOwner.h"

namespace Rendering
{
    struct WindowConfig : PropertyOwner<WindowConfig>
    {
        PROPERTY_D(String, Title, "Swiftblade");
        PROPERTY_D(Vec2I, Size, Vec2I(1600, 900));
        PROPERTY_D(bool, Fullscreen, false); // Borderless fullscreen
        PROPERTY_D(Vec2I, Position, Vec2I(0, 0)); // 0,0 == centered
        PROPERTY_D(bool, Borderless, false);
        PROPERTY_D(String, Icon, "");
        
    };
}
