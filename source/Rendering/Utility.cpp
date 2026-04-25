#include "Utility.h"

#include "webgpu/webgpu.h"

String Rendering::ToStr(const WGPUStringView& InStr)
{
    if (InStr.data)
        return String(InStr.data, InStr.length);
    return {};
}

WGPUStringView Rendering::ToStr(const String& InStr)
{
    // Keep strings around! Slight memory leak
    static Map<String, String*> persistence;
    String* ptr = persistence[InStr];
    if (!ptr) ptr = new String(InStr);
    return WGPUStringView(ptr->data(), ptr->length());
}

WGPUStringView Rendering::ToStr(const char* InStr)
{
    return WGPUStringView(InStr, WGPU_STRLEN);
}
