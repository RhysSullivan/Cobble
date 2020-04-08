#include "AkMediaAsset.h"

#include "AkAudioDevice.h"
#include "HAL/PlatformProperties.h"

#if WITH_EDITOR
#include "Interfaces/ITargetPlatform.h"
#endif


FAkMediaDataChunk::FAkMediaDataChunk() { }

FAkMediaDataChunk::FAkMediaDataChunk(IFileHandle* FileHandle, int64 BytesToRead, uint32 BulkDataFlags, bool IsPrefetch)
	: IsPrefetch(IsPrefetch)
{
	Data.SetBulkDataFlags(BulkDataFlags);
	Data.Lock(EBulkDataLockFlags::LOCK_READ_WRITE);
	FileHandle->Read(reinterpret_cast<uint8*>(Data.Realloc(BytesToRead)), BytesToRead);
	Data.Unlock();
}

void FAkMediaDataChunk::Serialize(FArchive& Ar, UObject* Owner)
{
	Ar << IsPrefetch;
	Data.Serialize(Ar, Owner);
}

void UAkMediaAssetData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	int32 numChunks = DataChunks.Num();
	Ar << numChunks;

	if (Ar.IsLoading())
	{
		DataChunks.Empty();
		for (int32 i = 0; i < numChunks; ++i)
		{
			DataChunks.Add(new FAkMediaDataChunk());
		}
	}

	for (int32 i = 0; i < numChunks; ++i)
	{
		DataChunks[i].Serialize(Ar, this);
	}
}

void UAkMediaAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITORONLY_DATA
	if (Ar.IsFilterEditorOnly())
	{
		if (Ar.IsSaving())
		{
			auto currentMediaData = MediaAssetDataPerPlatform.Find(Ar.CookingTarget()->IniPlatformName());
			CurrentMediaAssetData = currentMediaData ? *currentMediaData : nullptr;
		}

		Ar << CurrentMediaAssetData;
	}
	else
	{
		Ar << MediaAssetDataPerPlatform;
	}
#else
	Ar << CurrentMediaAssetData;
#endif
}

void UAkMediaAsset::PostLoad()
{
	Super::PostLoad();

	loadMedia();
}

void UAkMediaAsset::BeginDestroy()
{
	Super::BeginDestroy();

	unloadMedia();
}

#if WITH_EDITOR
#include "Async/Async.h"
void UAkMediaAsset::Reset()
{
	MediaAssetDataPerPlatform.Empty();
	MediaName.Empty();
	CurrentMediaAssetData = nullptr;
	AsyncTask(ENamedThreads::GameThread, [this] {
		MarkPackageDirty();
		});
}

UAkMediaAssetData* UAkMediaAsset::FindOrAddMediaAssetData(const FString& platform)
{
	auto platformData = MediaAssetDataPerPlatform.Find(platform);
	if (platformData)
	{
		return *platformData;
	}

	auto newPlatformData = NewObject<UAkMediaAssetData>(this);
	MediaAssetDataPerPlatform.Add(platform, newPlatformData);
	return newPlatformData;
}
#endif

void UAkMediaAsset::Load()
{
	loadMedia();
}

void UAkMediaAsset::Unload()
{
	unloadMedia();
}

FAkMediaDataChunk const* UAkMediaAsset::GetStreamedChunk() const
{
	auto mediaData = getMediaAssetData();
	if (!mediaData || mediaData->DataChunks.Num() <= 0)
	{
		return nullptr;
	}

	if (!mediaData->DataChunks[0].IsPrefetch)
	{
		return &mediaData->DataChunks[0];
	}

	if (mediaData->DataChunks.Num() >= 2)
	{
		return &mediaData->DataChunks[1];
	}

	return nullptr;
}

void UAkMediaAsset::loadMedia()
{
	auto assetData = getMediaAssetData();
	if (!assetData || assetData->DataChunks.Num() <= 0)
	{
		return;
	}

	auto& DataChunk = assetData->DataChunks[0];
	if (assetData->IsStreamed && !DataChunk.IsPrefetch)
	{
		return;
	}

#if !WITH_EDITOR
	if (DataChunk.Data.GetBulkDataSize() <= 0 || !DataChunk.Data.IsAvailableForUse())
	{
		return;
	}
#endif

	auto audioDevice = FAkAudioDevice::Get();
	if (!audioDevice)
	{
		return;
	}

#if WITH_EDITOR
	const void* bulkMediaData = DataChunk.Data.LockReadOnly();
#else
	RawMediaData = DataChunk.Data.Lock(LOCK_READ_ONLY);
#endif

	auto dataBulkSize = DataChunk.Data.GetBulkDataSize();

#if WITH_EDITOR
	EditorMediaData.Reset(dataBulkSize);
	RawMediaData = EditorMediaData.GetData();
	FMemory::Memcpy(RawMediaData, bulkMediaData, dataBulkSize);
	DataChunk.Data.Unlock();
#endif

	AkSourceSettings sourceSettings
	{
		Id, reinterpret_cast<AkUInt8*>(RawMediaData), static_cast<AkUInt32>(dataBulkSize)
	};

#if AK_SUPPORT_DEVICE_MEMORY
	if (assetData->UseDeviceMemory)
	{
		MediaDataDeviceMemory = (AkUInt8*)AKPLATFORM::AllocDevice(dataBulkSize, 0);
		if (MediaDataDeviceMemory)
		{
			FMemory::Memcpy(MediaDataDeviceMemory, RawMediaData, dataBulkSize);
			sourceSettings.pMediaMemory = MediaDataDeviceMemory;
		}
		else
		{
			UE_LOG(LogAkAudio, Error, TEXT("Allocating device memory failed!"))
		}
	}
#endif

	if (audioDevice->SetMedia(&sourceSettings, 1) != AK_Success)
	{
		UE_LOG(LogAkAudio, Log, TEXT("SetMedia failed for ID: %u"), Id);
	}

#if !WITH_EDITOR
	DataChunk.Data.Unlock();
#endif
}

void UAkMediaAsset::unloadMedia()
{
	auto assetData = getMediaAssetData();
	auto mediaData = RawMediaData;
#if AK_SUPPORT_DEVICE_MEMORY
	if (MediaDataDeviceMemory)
	{
		mediaData = MediaDataDeviceMemory;
	}
#endif
	if (assetData && mediaData && assetData->DataChunks.Num() > 0)
	{
		if (auto audioDevice = FAkAudioDevice::Get())
		{
			AkSourceSettings sourceSettings
			{
				Id, reinterpret_cast<AkUInt8*>(mediaData), static_cast<AkUInt32>(assetData->DataChunks[0].Data.GetBulkDataSize())
			};
			audioDevice->UnsetMedia(&sourceSettings, 1);
		}
		RawMediaData = nullptr;

#if AK_SUPPORT_DEVICE_MEMORY
		if (MediaDataDeviceMemory)
		{
			AKPLATFORM::FreeDevice(MediaDataDeviceMemory, assetData->DataChunks[0].Data.GetBulkDataSize(), 0, true);
			MediaDataDeviceMemory = nullptr;
		}
#endif
	}
}

UAkMediaAssetData* UAkMediaAsset::getMediaAssetData() const
{
#if! WITH_EDITORONLY_DATA
	return CurrentMediaAssetData;
#else
	const FString runningPlatformName(FPlatformProperties::IniPlatformName());
	if (auto platformMediaData = MediaAssetDataPerPlatform.Find(runningPlatformName))
	{
		return *platformMediaData;
	}

	return nullptr;
#endif
}

TTuple<void*, int64> UAkExternalMediaAsset::GetExternalSourceData() const
{
	auto* mediaData = getMediaAssetData();

	if (mediaData && mediaData->DataChunks.Num() > 0)
	{
		auto result = MakeTuple(mediaData->DataChunks[0].Data.Lock(LOCK_READ_ONLY), mediaData->DataChunks[0].Data.GetBulkDataSize());
		mediaData->DataChunks[0].Data.Unlock();
		return result;
	}

	return {};
}