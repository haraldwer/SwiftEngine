#pragma once
#include "Context/ContextConfig.h"
#include "Window/WindowConfig.h"

namespace Rendering
{
    struct Config : BaseConfig<Config>
    {
        PROPERTY(WindowConfig, Window);
        PROPERTY(ContextConfig, Context);
        
        String Name() const override { return "Rendering"; }
    };
}
