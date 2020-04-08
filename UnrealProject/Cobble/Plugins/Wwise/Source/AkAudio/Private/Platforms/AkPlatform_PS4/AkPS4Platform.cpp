// Copyright (c) 2006-2018 Audiokinetic Inc. / All Rights Reserved
#if PLATFORM_PS4

#include "Platforms/AkPlatform_PS4/AkPS4Platform.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"

FString FAkPS4Platform::GetWwisePluginDirectory()
{
    // Is it not possible to get an absolute path on PS4, so we build it ourselves...
    FString BaseDirectory = TEXT("/app0");
    if (IPluginManager::Get().FindPlugin("Wwise")->GetType() == EPluginType::Engine)
    {
        BaseDirectory /= TEXT("engine");
    }
    else
    {
        BaseDirectory /= FApp::GetProjectName();
    }

    return BaseDirectory / TEXT("Plugins") / TEXT("Wwise");
}

#endif