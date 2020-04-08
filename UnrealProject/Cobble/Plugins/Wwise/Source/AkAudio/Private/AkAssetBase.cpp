#include "AkAssetBase.h"

#include "Async/Async.h"
#include "AkMediaAsset.h"
#include "AkUnrealHelper.h"
#include "IntegrationBehavior/AkIntegrationBehavior.h"

#if WITH_EDITOR
#include "TargetPlatform/Public/Interfaces/ITargetPlatform.h"
#endif

void UAkAssetData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Data.Serialize(Ar, this);
}

AKRESULT UAkAssetData::Load()
{
	return AkIntegrationBehavior::Get()->AkAssetData_Load(this);
}

AKRESULT UAkAssetData::Unload()
{
	if (BankID == AK_INVALID_BANK_ID)
		return Data.GetBulkDataSize() == 0 ? AK_Success : AK_Fail;

	if (auto AudioDevice = FAkAudioDevice::Get())
	{
		AudioDevice->UnloadBankFromMemory(BankID, RawData);
	}

	BankID = AK_INVALID_BANK_ID;
	RawData = nullptr;
	return AK_Success;
}

#if WITH_EDITOR
void UAkAssetData::GetMediaList(TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList) const
{
}
#endif

AKRESULT UAkAssetDataWithMedia::Load()
{
	auto result = Super::Load();
	auto AudioDevice = FAkAudioDevice::Get();
	if (!AudioDevice)
		return result;

	if (result != AK_Success)
		return result;

	if (MediaList.Num() <= 0)
		return result;

	TArray<FSoftObjectPath> MediaToLoad;
	for (auto& media : MediaList)
		MediaToLoad.AddUnique(media.ToSoftObjectPath());

	mediaStreamHandle = AudioDevice->GetStreamableManager().RequestAsyncLoad(MediaToLoad);
	return result;
}

AKRESULT UAkAssetDataWithMedia::Unload()
{
	auto result = Super::Unload();
	if (result != AK_Success)
		return result;

	if (!mediaStreamHandle.IsValid())
		return result;

	mediaStreamHandle->ReleaseHandle();
	mediaStreamHandle.Reset();
	return result;
}

#if WITH_EDITOR
void UAkAssetDataWithMedia::GetMediaList(TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList) const
{
	Super::GetMediaList(mediaList);

	for (auto& media : MediaList)
	{
		mediaList.AddUnique(media);
	}
}
#endif

void UAkAssetPlatformData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITORONLY_DATA
	if (!Ar.IsFilterEditorOnly())
	{
		Ar << AssetDataPerPlatform;
		return;
	}

	if (Ar.IsSaving())
	{
		auto cookedAssetData = AssetDataPerPlatform.Find(Ar.CookingTarget()->IniPlatformName());
		CurrentAssetData = cookedAssetData ? *cookedAssetData : nullptr;
	}
#endif

	Ar << CurrentAssetData;
}

#if WITH_EDITOR
void UAkAssetPlatformData::GetMediaList(TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList) const
{
	for (auto& entry : AssetDataPerPlatform)
	{
		entry.Value->GetMediaList(mediaList);
	}
}
#endif

void UAkAssetBase::PostLoad()
{
	Super::PostLoad();

	Load();
}

void UAkAssetBase::BeginDestroy()
{
	Super::BeginDestroy();

	Unload();
}

void UAkAssetBase::Load()
{
	if (auto assetData = getAssetData())
	{
		assetData->Load();
	}
}

void UAkAssetBase::Unload()
{
	if (auto assetData = getAssetData())
	{
		assetData->Unload();
	}
}

UAkAssetData* UAkAssetBase::getAssetData() const
{
	if (!PlatformAssetData)
		return nullptr;

#if WITH_EDITORONLY_DATA
	if (auto assetData = PlatformAssetData->AssetDataPerPlatform.Find(FPlatformProperties::IniPlatformName()))
		return *assetData;

	return nullptr;
#else
	return PlatformAssetData->CurrentAssetData;
#endif
}

UAkAssetData* UAkAssetBase::createAssetData(UObject* parent) const
{
	return NewObject<UAkAssetData>(parent);
}

#if WITH_EDITOR
UAkAssetData* UAkAssetBase::FindOrAddAssetData(const FString& platform, const FString& language)
{
	FScopeLock autoLock(&assetDataLock);

	if (!PlatformAssetData)
	{
		PlatformAssetData = NewObject<UAkAssetPlatformData>(this);
	}

	return internalFindOrAddAssetData(PlatformAssetData, platform, this);
}

UAkAssetData* UAkAssetBase::internalFindOrAddAssetData(UAkAssetPlatformData* data, const FString& platform, UObject* parent)
{
	auto assetData = data->AssetDataPerPlatform.Find(platform);
	if (assetData)
		return *assetData;

	auto newAssetData = createAssetData(parent);
	data->AssetDataPerPlatform.Add(platform, newAssetData);
	return newAssetData;
}

void UAkAssetBase::GetMediaList(TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList) const
{
	if (PlatformAssetData)
	{
		PlatformAssetData->GetMediaList(mediaList);
	}
}

bool UAkAssetBase::NeedsRebuild() const
{
	TArray<TSoftObjectPtr<UAkMediaAsset>> mediaList;
	GetMediaList(mediaList);

	for (auto& media : mediaList)
	{
		if (media.ToSoftObjectPath().IsValid() && !media.IsValid())
		{
			return true;
		}
	}

	return false;
}

void UAkAssetBase::Reset()
{
	PlatformAssetData = nullptr;

	Super::Reset();
}
#endif
