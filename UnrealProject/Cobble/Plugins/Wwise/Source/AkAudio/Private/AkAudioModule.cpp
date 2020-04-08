// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

#include "AkAudioModule.h"
#include "AkAudioDevice.h"
#include "AkAudioStyle.h"
#include "AkWaapiClient.h"
#include "Misc/CoreDelegates.h"

#if WITH_EDITOR
#include "AkUnrealHelper.h"
#include "AssetRegistry/Public/AssetRegistryModule.h"
#endif

IMPLEMENT_MODULE( FAkAudioModule, AkAudio )
FAkAudioModule* FAkAudioModule::AkAudioModuleIntance = nullptr;

void FAkAudioModule::StartupModule()
{
	if (AkAudioModuleIntance)
		return;

	AkAudioModuleIntance = this;

#if WITH_EDITOR
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		
	auto initBankPath = FString::Printf(TEXT("%s/InitBank"), *AkUnrealHelper::GetBaseAssetPackagePath());
	FString initBankFileName;
	TArray<FString> paths;
	if (FPackageName::TryConvertLongPackageNameToFilename(initBankPath, initBankFileName, FPackageName::GetAssetPackageExtension()))
	{
		paths.Add(initBankFileName);
	}
		
	AssetRegistryModule.Get().ScanFilesSynchronous(paths, false);
#endif

	AkAudioDevice = new FAkAudioDevice;
	if (!AkAudioDevice)
		return;

	FAkWaapiClient::Initialize();
	if (!AkAudioDevice->Init())
	{
		delete AkAudioDevice;
		AkAudioDevice = nullptr;
		return;
	}

	OnTick = FTickerDelegate::CreateRaw(AkAudioDevice, &FAkAudioDevice::Update);
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(OnTick);

	FAkWaapiClient::Initialize();
	FAkAudioStyle::Initialize();
}

void FAkAudioModule::ShutdownModule()
{
	FAkAudioStyle::Shutdown();

	FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);

	if (AkAudioDevice) 
	{
		AkAudioDevice->Teardown();
		delete AkAudioDevice;
		AkAudioDevice = nullptr;
	}

	FAkWaapiClient::DeleteInstance();

	AkAudioModuleIntance = nullptr;
}

FAkAudioDevice* FAkAudioModule::GetAkAudioDevice()
{ 
	return AkAudioDevice;
}
