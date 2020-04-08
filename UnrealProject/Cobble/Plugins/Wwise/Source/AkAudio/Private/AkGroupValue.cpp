#include "AkGroupValue.h"

#include "AkAudioDevice.h"

void UAkGroupValue::PostLoad()
{
	Super::PostLoad();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		GetPathName(nullptr, packagePath);

		if (auto* audioDevice = FAkAudioDevice::Get())
		{
			audioDevice->OnLoadSwitchValue.Broadcast(packagePath);
		}
	}
}

void UAkGroupValue::BeginDestroy()
{
	Super::BeginDestroy();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		if (auto* audioDevice = FAkAudioDevice::Get())
		{
			audioDevice->OnUnloadSwitchValue.Broadcast(packagePath);

			if (GEngine)
			{
				GEngine->ForceGarbageCollection();
			}
		}
	}
}
