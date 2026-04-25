#include "Shader.h"
#include "Context/Context.h"

bool Rendering::ShaderResource::Load()
{
    // Create shader
    CHECK_RETURN_LOG(!Utility::File::Exists(id.Str()), "Shader file does not exist: " + id.Str(), false)
    
    LOG("Loading shader: " + id.Str());
    String file = LoadShaderFile(id.Str(), includes);
    CHECK_RETURN_LOG(file.empty(), "Failed to read shader file: " + id.Str(), false)
    
    WGPUShaderModuleDescriptor shaderDesc;
    shaderDesc.label = ToStr("Default shader");
    WGPUShaderSourceWGSL shaderCodeSource;
    shaderCodeSource.chain.next = nullptr;
    shaderCodeSource.chain.sType = WGPUSType_ShaderSourceWGSL;
    shaderDesc.nextInChain = &shaderCodeSource.chain;
    shaderCodeSource.code = ToStr(file);
    auto& c = Context::Get();
    shader = c.CreateShader(shaderDesc);
    return c.CheckDeviceValidation();
}

bool Rendering::ShaderResource::Unload()
{
    if (shader)
    {
        wgpuShaderModuleRelease(shader);
        shader = {};
    }
    includes.clear();
    return true;
}

bool Rendering::ShaderResource::Accept(const String &InPath)
{
    auto name = Utility::File::Name(InPath);
    return name.starts_with("SH_") &&
        (InPath.ends_with(".wgsl") ||
        !name.contains("."));
}

Utility::Timepoint Rendering::ShaderResource::GetEditTime() const
{
    Utility::Timepoint max = Utility::Timepoint::min(); 
    for (String path : includes)
        max = Utility::Math::Max(max, Utility::File::GetWriteTime(path));
    return  max;
}

String Rendering::ShaderResource::LoadShaderFile(const String& InPath, Set<String>& InIncludes)
{
    InIncludes.clear();
    String shader = Utility::File::Read(InPath);
    shader = ProcessDefines(shader);
    shader = ProcessIncludes(shader, InPath, InIncludes);

    // Fix null terminations
    std::erase(shader, '\0');
    
    return shader;
}

String Rendering::ShaderResource::ProcessIncludes(const String& InShaderCode, const String& InPath, Set<String>& InIncludes)
{
    InIncludes.insert(InPath);
    
    String processedShader;
    size_t searchOffset = 0;
    const String searchPattern = "#include \"";
    while (true)
    {
        const size_t findOffset = InShaderCode.find(searchPattern, searchOffset);
        processedShader += InShaderCode.substr(searchOffset, findOffset - searchOffset); 
        if (findOffset == std::string::npos)
            break;

        const size_t includeEnd = InShaderCode.find_first_of('\"', findOffset + searchPattern.length());
        const size_t includeStart = findOffset + searchPattern.length();
        const String includePath = InShaderCode.substr(includeStart, includeEnd - includeStart);

        if (Utility::File::Exists(includePath))
        {
            if (!InIncludes.contains(includePath))
            {
                const String includeContent = Utility::File::Read(includePath);
                const String processedInclude = ProcessIncludes(includeContent, includePath, InIncludes);
                processedShader += processedInclude; 
            }
        }
        else
        {
            LOG("Tried to include non-existing file: " + includePath + " in " + InPath);
        }
        
        searchOffset = includeEnd + 1;
    }
    return processedShader;
}

String Rendering::ShaderResource::ProcessDefines(const String& InShaderCode)
{
    String result = InShaderCode;
    return result;
    
    auto search = [&](String& InCode, const String& InSearchStart, const String& InSearchEnd, const std::function<String(const String& InContent)>& InReplaceFunc) -> bool
    {
        bool found = false;
        String processedShader;
        size_t searchOffset = 0;
        while (true)
        {
            const size_t findOffset = InCode.find(InSearchStart, searchOffset);
            processedShader += InCode.substr(searchOffset, findOffset - searchOffset); 
            if (findOffset == std::string::npos)
                break;
            
            const size_t findEnd = InCode.find_first_of(InSearchEnd, findOffset + InSearchStart.length());
            const size_t findStart = findOffset + InSearchStart.length();
            const String findStr = InCode.substr(findStart, findEnd - findStart);
            processedShader += InReplaceFunc(findStr);
            searchOffset = findEnd + InSearchEnd.length();
            found = true;
        }
        InCode = processedShader;
        return found; 
    };
    
    // Find defines
    Set<String> defineFinds;
    auto foundDefine = [&](const String& InFindStr) -> String
    {
        const auto start = InFindStr.find_first_not_of(' ');
        const auto end = InFindStr.find_last_not_of(' ');
        defineFinds.insert(InFindStr.substr(start, end - start));
        return "";
    };

    // ifdefs
    auto foundIfdef = [&](const String& InFindStr) -> String
    {
        String str = InFindStr;
        const bool negated = InFindStr.starts_with("ndef");
        if (negated)
            str = str.substr(4);

        const auto defEnd = str.find_first_of('\n');
        const auto defStart = str.find_first_not_of(' ');
        const String def = str.substr(defStart, defEnd - defStart);
        
        if (defineFinds.contains(def) != negated)
            return str.substr(defEnd);
        return ""; 
    };
    
    while (true)
    {
        bool changed = false;
        changed |= search(result, "#if", "#endif", foundIfdef); // Resolve ifdefs
        changed |= search(result, "#define ", "\n", foundDefine); // Resolve defines
        if (!changed)
            break;
    }

    return result;
}