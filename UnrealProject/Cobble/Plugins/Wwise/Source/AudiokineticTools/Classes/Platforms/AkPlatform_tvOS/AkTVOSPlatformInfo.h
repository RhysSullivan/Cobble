#pragma once

#include "Platforms/AkPlatformInfo.h"
#include "AkTVOSPlatformInfo.generated.h"

UCLASS()
class UAkTVOSPlatformInfo : public UAkPlatformInfo
{
	GENERATED_BODY()

public:
	UAkTVOSPlatformInfo()
	{
		WwisePlatform = "tvOS";
		Architecture = "tvOS";
		bUsesStaticLibraries = true;
	}
};
