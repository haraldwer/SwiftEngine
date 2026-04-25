#include "Manager.h"

#include "Config.h"
#include "ImGuiContext.h"
#include "Commands/Command.h"
#include "Commands/CommandList.h"

void Rendering::Manager::Init()
{
    RN_PROFILE();
    
    Config config;
    config.LoadConfig();
    
    context.BeginInit(config.Context);
    window.Open(config.Window);
    context.EndInit(window);
    
    ImGuiContext::Init(window, context);
    pipelineCache.Init();
    viewport.Init(window.GetConfig().Size);
    modelDefaults.Init();
}

void Rendering::Manager::Deinit()
{
    RN_PROFILE();
    
    Config config;
    config.Window = window.GetConfig();
    config.Context = context.GetConfig();
    config.SaveConfig();
    
    modelDefaults.Deinit();
    pipelineCache.Deinit();
    ImGuiContext::Deinit();
    window.Close();
    context.Deinit();
    buffers.Clear();
}

int Rendering::Manager::Frame(bool& InRun)
{
    RN_PROFILE();
    
    ImGuiContext::EndFrame();

    RenderTarget& windowTarget = window.BeginFrame();
    viewport.Resize(window.GetConfig().Size);
    auto& targets = viewport.GetTargets();
    
    list.Begin("Scene");
    sceneRenderer.Render(list, viewport);
    list.End();
    
    // Post processing...
    list.Begin("Postprocessing");
    
    list.End();
    
    list.Begin("Viewport");
    buffers.GetGroup(0).Set(0, targets.msaaFrame);
    Command blitCommand("Blit");
    blitCommand.targets = { &windowTarget };
    blitCommand.material = blit;
    blitCommand.buffers = &buffers;
    list.Add(blitCommand);
    
    list.Add(ImGuiContext::Command(windowTarget));
    list.End(); 
    
    // Present and wait
    list.Submit();
    window.Present(InRun);
    
    ImGuiContext::BeginFrame(window.GetConfig().Size);
    
    return pacer.Pace();
}
