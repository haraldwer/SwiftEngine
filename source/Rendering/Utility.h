#pragma once
#include "webgpu/webgpu.h"

namespace Rendering
{
    String ToStr(const WGPUStringView& InStr);
    WGPUStringView ToStr(const String& InStr);
    WGPUStringView ToStr(const char* InStr);
}
