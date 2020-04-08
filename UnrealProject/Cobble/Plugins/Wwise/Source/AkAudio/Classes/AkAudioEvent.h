// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved
#pragma once

#include "AkAssetBase.h"
#include "Serialization/BulkData.h"

#include "AkAudioEvent.generated.h"

class UAkGroupValue;
class UAkMediaAsset;
class UAkAudioBank;
struct FStreamableHandle;

UCLASS(EditInlineNew)
class AKAUDIO_API UAkAssetDataSwitchContainerData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "AkAudioEvent")
	TSoftObjectPtr<UAkGroupValue> GroupValue;

	UPROPERTY(VisibleAnywhere, Category = "AkAudioEvent")
	TArray<TSoftObjectPtr<UAkMediaAsset>> MediaList;

	UPROPERTY(VisibleAnywhere, Category = "AkAudioEvent")
	TArray<UAkAssetDataSwitchContainerData*> Children;

	friend class UAkAssetDataSwitchContainer;

public:
#if WITH_EDITOR
	void GetMediaList(TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList) const;
#endif

private:
	void internalGetMediaList(const UAkAssetDataSwitchContainerData* data, TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList) const;

private:
	TSharedPtr<FStreamableHandle> streamHandle;
};

UCLASS()
class AKAUDIO_API UAkAssetDataSwitchContainer : public UAkAssetDataWithMedia
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "AkAudioEvent")
	TArray<UAkAssetDataSwitchContainerData*> SwitchContainers;

public:
	void BeginDestroy() override;

	AKRESULT Load() override;
	AKRESULT Unload() override;

#if WITH_EDITOR
	void GetMediaList(TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList) const override;
#endif

private:
	void loadSwitchContainer(const TArray<UAkAssetDataSwitchContainerData*>& switchContainers);
	void loadSwitchContainer(UAkAssetDataSwitchContainerData* switchContainer);

	void onLoadSwitchValue(const FSoftObjectPath& path);
	void loadSwitchValue(const FSoftObjectPath& path, const TArray<UAkAssetDataSwitchContainerData*>& switchContainers);
	void loadSwitchValue(const FSoftObjectPath& path, UAkAssetDataSwitchContainerData* switchContainer);

	void onUnloadSwitchValue(const FSoftObjectPath& path);
	void unloadSwitchValue(const FSoftObjectPath& path, const TArray<UAkAssetDataSwitchContainerData*>& switchContainers);
	void unloadSwitchValue(const FSoftObjectPath& path, UAkAssetDataSwitchContainerData* switchContainer);

	void loadSwitchContainerMedia(UAkAssetDataSwitchContainerData* switchContainer);

	void unloadSwitchContainerMedia(const TArray<UAkAssetDataSwitchContainerData*>& switchContainers);
	void unloadSwitchContainerMedia(UAkAssetDataSwitchContainerData* switchContainer);

private:
	FDelegateHandle onLoadSwitchHandle;
	FDelegateHandle onUnloadSwitchHandle;
};

UCLASS()
class AKAUDIO_API UAkAudioEventData : public UAkAssetDataSwitchContainer
{
	GENERATED_BODY()

public:
	/** Maximum attenuation radius for this event */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AkAudioEvent")
	float MaxAttenuationRadius;

	/** Whether this event is infinite (looping) or finite (duration parameters are valid) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AkAudioEvent")
	bool IsInfinite;

	/** Minimum duration */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AkAudioEvent")
	float MinimumDuration;

	/** Maximum duration */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AkAudioEvent")
	float MaximumDuration;

	// This map is used when the event is part of a AkAudioBank
	UPROPERTY(VisibleAnywhere, Category = "AkAudioEvent")
	TMap<FString, UAkAssetDataSwitchContainer*> LocalizedMedia;

public:
	UAkAssetDataSwitchContainer* FindOrAddLocalizedData(const FString& language);

#if WITH_EDITOR
	void GetMediaList(TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList) const override;
#endif

private:
	mutable FCriticalSection LocalizedMediaLock;
};

UCLASS(meta=(BlueprintSpawnableComponent))
class AKAUDIO_API UAkAudioEvent : public UAkAssetBase
{
	GENERATED_BODY()

public:
	/** Maximum attenuation radius for this event */
	UFUNCTION(BlueprintGetter, Category = "AkAudioEvent")
	float MaxAttenuationRadius() const;

	/** Whether this event is infinite (looping) or finite (duration parameters are valid) */
	UFUNCTION(BlueprintGetter, Category = "AkAudioEvent")
	bool IsInfinite() const;

	/** Minimum duration */
	UFUNCTION(BlueprintGetter, Category = "AkAudioEvent")
	float MinimumDuration() const;
	void SetMinimumDuration(float value);

	/** Maximum duration */
	UFUNCTION(BlueprintGetter, Category = "AkAudioEvent")
	float MaximumDuration() const;
	void SetMaximumDuration(float value);

	UPROPERTY(VisibleAnywhere, Category = "AkAudioEvent")
	TMap<FString, TSoftObjectPtr<UAkAssetPlatformData>> LocalizedPlatformAssetDataMap;

	UPROPERTY(EditAnywhere, Category = "AkAudioEvent")
	UAkAudioBank* RequiredBank = nullptr;

private:
	UPROPERTY(Transient)
	UAkAssetPlatformData* CurrentLocalizedPlatformData = nullptr;

public:
	void Load() override;
	void Unload() override;

	bool IsLocalized() const;

	bool SwitchLanguage(const FString& newAudioCulture, const SwitchLanguageCompletedFunction& Function = SwitchLanguageCompletedFunction());

#if WITH_EDITOR
	UAkAssetData* FindOrAddAssetData(const FString& platform, const FString& language) override;

	void Reset() override;
#endif

	friend class AkEventBasedIntegrationBehavior;

protected:
	UAkAssetData* createAssetData(UObject* parent) const override;
	UAkAssetData* getAssetData() const override;

private:
	void loadLocalizedData(const FString& audioCulture, const SwitchLanguageCompletedFunction& Function);
	void unloadLocalizedData();
	void onLocalizedDataLoaded();
	void superLoad();

private:
	TSharedPtr<FStreamableHandle> localizedStreamHandle;
};
