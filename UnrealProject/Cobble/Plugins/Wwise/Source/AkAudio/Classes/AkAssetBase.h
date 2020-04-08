#pragma once

#include "AkAudioDevice.h"
#include "AkAudioType.h"
#include "Serialization/BulkData.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/Object.h"

#include "AkAssetBase.generated.h"

class UAkMediaAsset;

struct FStreamableHandle;

UCLASS()
class AKAUDIO_API UAkAssetData : public UObject
{
	GENERATED_BODY()
public:
	FByteBulkData Data;

public:
	void Serialize(FArchive& Ar) override;

	virtual AKRESULT Load();
	virtual AKRESULT Unload();

#if WITH_EDITOR
	virtual void GetMediaList(TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList) const;
#endif

	friend class AkEventBasedIntegrationBehavior;

protected:
	AkBankID BankID = AK_INVALID_BANK_ID;
	const void* RawData = nullptr;

#if WITH_EDITOR
	TArray<uint8> EditorRawData;
#endif
};

UCLASS() 
class AKAUDIO_API UAkAssetDataWithMedia : public UAkAssetData
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "AkAssetBase")
	TArray<TSoftObjectPtr<UAkMediaAsset>> MediaList;

	AKRESULT Load() override;
	AKRESULT Unload() override;

#if WITH_EDITOR
	void GetMediaList(TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList) const override;
#endif

protected:
	TSharedPtr<FStreamableHandle> mediaStreamHandle;
};

UCLASS()
class AKAUDIO_API UAkAssetPlatformData : public UObject
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "AkAssetBase")
	TMap<FString, UAkAssetData*> AssetDataPerPlatform;
#endif

	UPROPERTY()
	UAkAssetData* CurrentAssetData;

public:
	void Serialize(FArchive& Ar) override;

#if WITH_EDITOR
	void GetMediaList(TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList) const;
#endif
};

UCLASS()
class AKAUDIO_API UAkAssetBase : public UAkAudioType
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "AkAssetBase")
	UAkAssetPlatformData* PlatformAssetData = nullptr;

public:
	void PostLoad() override;
	void BeginDestroy() override;

	virtual void Load();
	virtual void Unload();

#if WITH_EDITOR
	template<typename T>
	T* FindOrAddAssetDataTyped(const FString& platform, const FString& language)
	{
		return Cast<T>(FindOrAddAssetData(platform, language));
	}

	virtual UAkAssetData* FindOrAddAssetData(const FString& platform, const FString& language);
	virtual void GetMediaList(TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList) const;

	bool NeedsRebuild() const;

	void Reset() override;
#endif

protected:
	virtual UAkAssetData* createAssetData(UObject* parent) const;
	virtual UAkAssetData* getAssetData() const;

	UAkAssetData* internalFindOrAddAssetData(UAkAssetPlatformData* data, const FString & platform, UObject* parent);

private:
#if WITH_EDITOR
	FCriticalSection assetDataLock;
#endif
};

using SwitchLanguageCompletedFunction = TFunction<void(bool)>;