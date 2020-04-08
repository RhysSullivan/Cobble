#include "AkSoundDataBuilder.h"

#include "AkAssetDatabase.h"
#include "AkAudioBank.h"
#include "AkAudioBankGenerationHelpers.h"
#include "AkAudioDevice.h"
#include "AkAudioEvent.h"
#include "AkAudioStyle.h"
#include "AkAuxBus.h"
#include "AkGroupValue.h"
#include "AkInitBank.h"
#include "AkMediaAsset.h"
#include "AkRtpc.h"
#include "AkSettings.h"
#include "AkSwitchValue.h"
#include "AkTrigger.h"
#include "AkUnrealHelper.h"
#include "AssetRegistry/Public/AssetRegistryModule.h"
#include "AssetTools/Public/AssetToolsModule.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "Editor.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StaticPluginWriter.h"
#include "ToolBehavior/AkToolBehavior.h"
#include "UnrealEd/Public/FileHelpers.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "AkAudio"

DECLARE_CYCLE_STAT(TEXT("AkSoundData - Gather Assets"), STAT_GatherAssets, STATGROUP_AkSoundDataSource);
DECLARE_CYCLE_STAT(TEXT("AkSoundData - Gather Media Assets"), STAT_GatherMediaAssets, STATGROUP_AkSoundDataSource);
DECLARE_CYCLE_STAT(TEXT("AkSoundData - Cook Media Asset"), STAT_CookMediaAsset, STATGROUP_AkSoundDataSource);
DECLARE_CYCLE_STAT(TEXT("AkSoundData - Fetch Event Platform Data For Event Group"), STAT_EventPlatformDataEventGroup, STATGROUP_AkSoundDataSource);
DECLARE_CYCLE_STAT(TEXT("AkSoundData - Parse Event Info For Event Group"), STAT_ParseEventInfoEventGroup, STATGROUP_AkSoundDataSource);

namespace AkSoundDataBuilder_Helpers
{
	constexpr auto SoundBankNamePrefix = TEXT("SB_");
}

AutoSetIsBuilding::AutoSetIsBuilding()
{
	if (FAkAudioDevice::Get())
	{
		FAkAudioDevice::Get()->IsBuildingData = true;
	}
}

AutoSetIsBuilding::~AutoSetIsBuilding()
{
	if (FAkAudioDevice::Get())
	{
		FAkAudioDevice::Get()->IsBuildingData = false;
	}
}

AkSoundDataBuilder::AkSoundDataBuilder(const InitParameters& InitParameter)
: initParameters(InitParameter)
{
	platformFile = &FPlatformFileManager::Get().GetPlatformFile();
}

AkSoundDataBuilder::~AkSoundDataBuilder()
{
}

void AkSoundDataBuilder::Init()
{
	assetRegistryModule = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	assetToolsModule = &FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	basePackagePath = AkUnrealHelper::GetBaseAssetPackagePath();
	localizedPackagePath = AkUnrealHelper::GetLocalizedAssetPackagePath();

	if (auto akSettings = GetDefault<UAkSettings>())
	{
		splitSwitchContainerMedia = akSettings->SplitSwitchContainerMedia;
	}

	wwiseProjectInfo.Parse();

	cacheDirectory = wwiseProjectInfo.CacheDirectory();
	defaultLanguage = wwiseProjectInfo.DefaultLanguage();

	if (!AkAssetDatabase::Get().IsInited())
	{
		AkAssetDatabase::Get().Init();
	}
}

void AkSoundDataBuilder::createNotificationItem()
{
	if (!IsRunningCommandlet())
	{
		AsyncTask(ENamedThreads::Type::GameThread, [sharedThis = SharedThis(this)]
		{
			FNotificationInfo Info(LOCTEXT("GeneratingSoundData", "Generating Wwise sound data..."));

			Info.Image = FAkAudioStyle::GetBrush(TEXT("AudiokineticTools.AkPickerTabIcon"));
			Info.bFireAndForget = false;
			Info.FadeOutDuration = 0.0f;
			Info.ExpireDuration = 0.0f;
			Info.Hyperlink = FSimpleDelegate::CreateLambda([]() { FGlobalTabmanager::Get()->InvokeTab(FName("OutputLog")); });
			Info.HyperlinkText = LOCTEXT("ShowOutputLogHyperlink", "Show Output Log");
			sharedThis->notificationItem = FSlateNotificationManager::Get().AddNotification(Info);
		});
	}
}

void AkSoundDataBuilder::notifyGenerationFailed()
{
	if (!IsRunningCommandlet())
	{
		AsyncTask(ENamedThreads::Type::GameThread, [sharedThis = SharedThis(this)]
		{
			if (sharedThis->notificationItem)
			{
				sharedThis->notificationItem->SetText(LOCTEXT("SoundDataGenerationFailed", "Generating Wwise sound data failed!"));

				sharedThis->notificationItem->SetCompletionState(SNotificationItem::CS_Fail);
				sharedThis->notificationItem->SetExpireDuration(3.5f);
				sharedThis->notificationItem->SetFadeOutDuration(0.5f);
				sharedThis->notificationItem->ExpireAndFadeout();
			}

			GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileFailed_Cue.CompileFailed_Cue"));
		});
	}
}

void AkSoundDataBuilder::notifyProfilingInProgress()
{
	if (!IsRunningCommandlet())
	{
		AsyncTask(ENamedThreads::Type::GameThread, [sharedThis = SharedThis(this)]
		{
			if (sharedThis->notificationItem)
			{
				sharedThis->notificationItem->SetText(LOCTEXT("SoundDataGenerationProfiling", "Cannot generate sound data while Authoring is profiling."));

				sharedThis->notificationItem->SetCompletionState(SNotificationItem::CS_Fail);
				sharedThis->notificationItem->SetExpireDuration(3.5f);
				sharedThis->notificationItem->SetFadeOutDuration(0.5f);
				sharedThis->notificationItem->ExpireAndFadeout();
			}

			GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileFailed_Cue.CompileFailed_Cue"));
		});
	}
}

void AkSoundDataBuilder::notifyCookingData()
{
	if (!IsRunningCommandlet())
	{
		AsyncTask(ENamedThreads::Type::GameThread, [sharedThis = SharedThis(this)]
		{
			if (sharedThis->notificationItem)
			{
				sharedThis->notificationItem->SetCompletionState(SNotificationItem::CS_Pending);
				sharedThis->notificationItem->SetText(LOCTEXT("CookingSoundData", "Cooking Wwise sound data into the assets..."));
			}
		});
	}
}

void AkSoundDataBuilder::notifyGenerationSucceeded()
{
	if (!IsRunningCommandlet())
	{
		AsyncTask(ENamedThreads::Type::GameThread, [sharedThis = SharedThis(this)]
		{
			if (sharedThis->notificationItem)
			{
				sharedThis->notificationItem->SetText(LOCTEXT("CookingSoundDataCompleted", "Cooking Wwise sound data completed!"));

				sharedThis->notificationItem->SetCompletionState(SNotificationItem::CS_Success);
				sharedThis->notificationItem->SetExpireDuration(3.5f);
				sharedThis->notificationItem->SetFadeOutDuration(0.5f);
				sharedThis->notificationItem->ExpireAndFadeout();
			}

			GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileSuccess_Cue.CompileSuccess_Cue"));
		});
	}
}

bool AkSoundDataBuilder::processMediaEntry(MediaToCookMap& mediaToCookMap, const FString& platform, TArray<TSoftObjectPtr<UAkMediaAsset>>& medias, const TSharedPtr<FJsonObject>& mediaData, bool isStreamed, bool isInitBank)
{
	FString stringId = mediaData->GetStringField("Id");

	uint32 id = FCString::Atoi(*stringId);

	FString language = mediaData->GetStringField("Language");

	FString cachePath = mediaData->GetStringField("Path");

	FString mediaName = FPaths::GetBaseFilename(mediaData->GetStringField("ShortName"));

	FSoftObjectPath assetPath;

	bool useDeviceMemory = false;
	FString stringUseDeviceMemory;
	if (mediaData->TryGetStringField(TEXT("UseDeviceMemory"), stringUseDeviceMemory))
	{
		useDeviceMemory = (stringUseDeviceMemory == TEXT("true"));
	}

	bool usingReferenceLanguageAsStandIn = false;
	if (mediaData->TryGetBoolField(TEXT("UsingReferenceLanguageAsStandIn"), usingReferenceLanguageAsStandIn) && usingReferenceLanguageAsStandIn)
	{
		language = defaultLanguage;
	}

	bool changed = false;

	if (language != TEXT("SFX"))
	{
		assetPath = FPaths::Combine(localizedPackagePath, language, AkUnrealHelper::MediaFolderName, stringId + TEXT(".") + stringId);
	}
	else
	{
		language.Empty();

		assetPath = FPaths::Combine(basePackagePath, AkUnrealHelper::MediaFolderName, stringId + TEXT(".") + stringId);
	}

	if (isInitBank)
	{
		FScopeLock autoLock(&mediaLock);
		if (mediaIdToAssetPath.Contains(id))
		{
			return false;
		}
	}

	if (!medias.ContainsByPredicate([&assetPath](const TSoftObjectPtr<UAkMediaAsset>& item) { return item.GetUniqueID() == assetPath; }))
	{
		medias.Emplace(assetPath);

		changed = true;

		{
			FScopeLock autoLock(&mediaLock);

			if (!mediaIdToAssetPath.Contains(id))
			{
				mediaIdToAssetPath.Add(id, assetPath);
			}
		}
	}

	{
		FScopeLock autoLock(&mediaLock);

		auto* mediaAssetToCookIt = mediaToCookMap.Find(id);

		if (mediaAssetToCookIt)
		{
			MediaAssetPerPlatformData& parsedPlatformData = mediaAssetToCookIt->Get()->ParsedPerPlatformData.FindOrAdd(platform);

			FString stringPrefetchSize;
			if (mediaData->TryGetStringField("PrefetchSize", stringPrefetchSize))
			{
				uint32 parsedPrefetchSize = static_cast<uint32>(FCString::Atoi(*stringPrefetchSize));
				
				parsedPlatformData.PrefetchSize = FMath::Max(parsedPlatformData.PrefetchSize, parsedPrefetchSize);
			}

			parsedPlatformData.UseDeviceMemory = useDeviceMemory;
			parsedPlatformData.IsStreamed = isStreamed ? true : parsedPlatformData.IsStreamed;
		}
		else
		{
			auto& newMediaToCookEntry = mediaToCookMap.Emplace(id);
			newMediaToCookEntry = MakeShared<MediaAssetToCook>();
			newMediaToCookEntry->Id = id;
			newMediaToCookEntry->AssetPath = assetPath;
			newMediaToCookEntry->Language = language;
			newMediaToCookEntry->CachePath = cachePath;
			newMediaToCookEntry->MediaName = mediaName;

			MediaAssetPerPlatformData& parsedPlatformData = newMediaToCookEntry->ParsedPerPlatformData.FindOrAdd(platform);
			parsedPlatformData.IsStreamed = isStreamed;
			parsedPlatformData.PrefetchSize = 0;
			parsedPlatformData.UseDeviceMemory = useDeviceMemory;
		}
	}

	return changed;
}

void AkSoundDataBuilder::parseGameSyncs(const TSharedPtr<FJsonObject>& soundBankJson)
{
	if (!AkUnrealHelper::IsUsingEventBased())
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* stateGroups = nullptr;
	if (soundBankJson->TryGetArrayField("StateGroups", stateGroups))
	{
		for (auto& stateGroupValueJson : *stateGroups)
		{
			auto& stateGroupJson = stateGroupValueJson->AsObject();

			FString stringGroupId = stateGroupJson->GetStringField("Id");
			uint32 groupId = static_cast<uint32>(FCString::Atoi64(*stringGroupId));

			auto& statesJson = stateGroupJson->GetArrayField("States");

			for (auto& stateValueJson : statesJson)
			{
				parseGroupValue(stateValueJson->AsObject(), groupId);
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* switchGroups = nullptr;
	if (soundBankJson->TryGetArrayField("SwitchGroups", switchGroups))
	{
		for (auto& switchGroupValueJson : *switchGroups)
		{
			auto& switchGroupJson = switchGroupValueJson->AsObject();

			FString stringGroupId = switchGroupJson->GetStringField("Id");
			uint32 groupId = static_cast<uint32>(FCString::Atoi64(*stringGroupId));

			auto& switchesJson = switchGroupJson->GetArrayField("Switches");

			for (auto& switchValueJson : switchesJson)
			{
				parseGroupValue(switchValueJson->AsObject(), groupId);
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* gameParameters = nullptr;
	if (soundBankJson->TryGetArrayField("GameParameters", gameParameters))
	{
		for (auto& gameParameterJsonValue : *gameParameters)
		{
			parseGameParameter(gameParameterJsonValue->AsObject());
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* triggers = nullptr;
	if (soundBankJson->TryGetArrayField("IncludedTriggers", triggers))
	{
		for (auto& triggerJsonValue : *triggers)
		{
			parseTrigger(triggerJsonValue->AsObject());
		}
	}
}

void AkSoundDataBuilder::parseGroupValue(const TSharedPtr<FJsonObject>& groupValueObject, uint32 groupId)
{
	FString stringGuid = groupValueObject->GetStringField("GUID");

	FGuid guid;
	FGuid::ParseExact(stringGuid, EGuidFormats::DigitsWithHyphensInBraces, guid);

	if (auto groupValueIt = AkAssetDatabase::Get().GroupValueMap.Find(guid))
	{
		auto groupValue = *groupValueIt;

		FString stringId = groupValueObject->GetStringField("Id");
		uint32 valueId = static_cast<uint32>(FCString::Atoi64(*stringId));

		bool changed = false;

		if (groupValue->GroupShortID != groupId)
		{
			groupValue->GroupShortID = groupId;
			changed = true;
		}

		if (groupValue->ShortID != valueId)
		{
			groupValue->ShortID = valueId;
			changed = true;
		}

		if (changed)
		{
			markAssetDirty(groupValue);
		}
	}
}

void AkSoundDataBuilder::parseGameParameter(const TSharedPtr<FJsonObject>& gameParameterObject)
{
	FString stringGuid = gameParameterObject->GetStringField("GUID");

	FGuid guid;
	FGuid::ParseExact(stringGuid, EGuidFormats::DigitsWithHyphensInBraces, guid);

	if (auto rtpcIt = AkAssetDatabase::Get().RtpcMap.Find(guid))
	{
		auto rtpc = *rtpcIt;

		FString stringId = gameParameterObject->GetStringField("Id");
		uint32 valueId = static_cast<uint32>(FCString::Atoi64(*stringId));

		bool changed = false;

		if (rtpc->ShortID != valueId)
		{
			rtpc->ShortID = valueId;
			changed = true;
		}

		if (changed)
		{
			markAssetDirty(rtpc);
		}
	}
}

void AkSoundDataBuilder::parseTrigger(const TSharedPtr<FJsonObject>& triggerObject)
{
	FString stringGuid = triggerObject->GetStringField("GUID");

	FGuid guid;
	FGuid::ParseExact(stringGuid, EGuidFormats::DigitsWithHyphensInBraces, guid);

	if (auto triggerIt = AkAssetDatabase::Get().TriggerMap.Find(guid))
	{
		auto trigger = *triggerIt;

		FString stringId = triggerObject->GetStringField("Id");
		uint32 valueId = static_cast<uint32>(FCString::Atoi64(*stringId));

		bool changed = false;

		if (trigger->ShortID != valueId)
		{
			trigger->ShortID = valueId;
			changed = true;
		}

		if (changed)
		{
			markAssetDirty(trigger);
		}
	}
}

bool AkSoundDataBuilder::parseMedia(const TSharedPtr<FJsonObject>& soundBankJson, MediaToCookMap& mediaToCookMap, TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList, const FString& platform, bool isInitBank)
{
	if (!AkUnrealHelper::IsUsingEventBased())
	{
		return false;
	}

	bool changed = false;

	const TArray<TSharedPtr<FJsonValue>>* streamedFiles = nullptr;
	if (soundBankJson->TryGetArrayField("ReferencedStreamedFiles", streamedFiles))
	{
		for (auto& streamJsonValue : *streamedFiles)
		{
			changed |= processMediaEntry(mediaToCookMap, platform, mediaList, streamJsonValue->AsObject(), true, isInitBank);
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* excludedMemoryFiles = nullptr;
	if (soundBankJson->TryGetArrayField("ExcludedMemoryFiles", excludedMemoryFiles))
	{
		for (auto& mediaJsonValue : *excludedMemoryFiles)
		{
			changed |= processMediaEntry(mediaToCookMap, platform, mediaList, mediaJsonValue->AsObject(), false, isInitBank);
		}
	}

	return changed;
}

bool AkSoundDataBuilder::parseAssetInfo(UAkAudioEvent* akEvent, UAkAssetData* platformData, const FString& platform, const FString& language, const TSharedPtr<FJsonObject>& soundBankData, MediaToCookMap& mediaToCookMap)
{
	bool changed = false;

	if (auto* eventPlatformData = Cast<UAkAudioEventData>(platformData))
	{
		const TArray<TSharedPtr<FJsonValue>>* eventsArray = nullptr;
		if (soundBankData->TryGetArrayField("IncludedEvents", eventsArray))
		{
			for(auto& eventJsonValue : *eventsArray)
			{
				auto& eventJson = eventJsonValue->AsObject();
				FString eventStringId = eventJson->GetStringField("GUID");

				FGuid eventId;
				FGuid::ParseExact(eventStringId, EGuidFormats::DigitsWithHyphensInBraces, eventId);

				if (eventId == akEvent->ID)
				{
					changed |= parseEventInfo(akEvent, eventPlatformData, eventJson);
					changed |= parseMedia(eventJson, mediaToCookMap, eventPlatformData->MediaList, platform, false);

					eventPlatformData->SwitchContainers.Empty();

					if (splitSwitchContainerMedia)
					{
						const TArray<TSharedPtr<FJsonValue>>* switchContainers = nullptr;
						if (eventJson->TryGetArrayField("SwitchContainers", switchContainers))
						{
							for (auto& switchContainerValueJson : *switchContainers)
							{
								auto& switchContainerJson = switchContainerValueJson->AsObject();

								UAkAssetDataSwitchContainerData* switchContainerEntry = NewObject<UAkAssetDataSwitchContainerData>(eventPlatformData);
								parseSwitchContainer(switchContainerJson, switchContainerEntry, eventPlatformData->MediaList, eventPlatformData);
								eventPlatformData->SwitchContainers.Add(switchContainerEntry);
							}
						}

						changed = true;
					}

					break;
				}
			}
		}
	}

	return changed;
}

bool AkSoundDataBuilder::parseEventInfo(UAkAudioEvent* akEvent, UAkAudioEventData* eventPlatformData, const TSharedPtr<FJsonObject>& eventJson)
{
	bool changed = false;

	FString stringId;
	if (eventJson->TryGetStringField("Id", stringId))
	{
		uint32 id = static_cast<uint32>(FCString::Atoi64(*stringId));
		if (akEvent->ShortID != id)
		{
			akEvent->ShortID = id;
			changed = true;
		}
	}

	FString ValueString;
	if (eventJson->TryGetStringField("MaxAttenuation", ValueString))
	{
		const float EventRadius = FCString::Atof(*ValueString);
		if (eventPlatformData->MaxAttenuationRadius != EventRadius)
		{
			eventPlatformData->MaxAttenuationRadius = EventRadius;
			changed = true;
		}
	}
	else
	{
		if (eventPlatformData->MaxAttenuationRadius != 0)
		{
			// No attenuation info in json file, set to 0.
			eventPlatformData->MaxAttenuationRadius = 0;
			changed = true;
		}
	}

	// if we can't find "DurationType", then we assume infinite
	const bool IsInfinite = !eventJson->TryGetStringField("DurationType", ValueString) || (ValueString == "Infinite") || (ValueString == "Unknown");
	if (eventPlatformData->IsInfinite != IsInfinite)
	{
		eventPlatformData->IsInfinite = IsInfinite;
		changed = true;
	}

	if (!IsInfinite)
	{
		if (eventJson->TryGetStringField("DurationMin", ValueString))
		{
			const float DurationMin = FCString::Atof(*ValueString);
			if (eventPlatformData->MinimumDuration != DurationMin)
			{
				eventPlatformData->MinimumDuration = DurationMin;
				changed = true;
			}
		}

		if (eventJson->TryGetStringField("DurationMax", ValueString))
		{
			const float DurationMax = FCString::Atof(*ValueString);
			if (eventPlatformData->MaximumDuration != DurationMax)
			{
				eventPlatformData->MaximumDuration = DurationMax;
				changed = true;
			}
		}
	}

	return changed;
}

void AkSoundDataBuilder::parseSwitchContainer(const TSharedPtr<FJsonObject>& switchContainerJson, UAkAssetDataSwitchContainerData* switchContainerEntry, TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList, UObject* parent)
{
	FString stringSwitchValue = switchContainerJson->GetStringField("SwitchValue");
	FGuid switchValueGuid;
	FGuid::ParseExact(stringSwitchValue, EGuidFormats::DigitsWithHyphensInBraces, switchValueGuid);

	if (auto groupValueIt = AkAssetDatabase::Get().GroupValueMap.Find(switchValueGuid))
	{
		switchContainerEntry->GroupValue = *groupValueIt;
	}

	const TArray<TSharedPtr<FJsonValue>>* jsonMediaList = nullptr;
	if (switchContainerJson->TryGetArrayField("Media", jsonMediaList))
	{
		for (auto& mediaJsonValue : *jsonMediaList)
		{
			auto& mediaJsonObject = mediaJsonValue->AsObject();

			FString stringId = mediaJsonObject->GetStringField("Id");
			uint32 mediaFileId = static_cast<uint32>(FCString::Atoi64(*stringId));

			FSoftObjectPath* mediaAssetPath = nullptr;

			{
				FScopeLock autoLock(&mediaLock);
				mediaAssetPath = mediaIdToAssetPath.Find(mediaFileId);
			}

			if (mediaAssetPath)
			{
				switchContainerEntry->MediaList.Emplace(*mediaAssetPath);

				mediaList.RemoveAll([mediaAssetPath](const TSoftObjectPtr<UAkMediaAsset>& item) {
					return item.GetUniqueID() == *mediaAssetPath;
				});
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* children = nullptr;
	if (switchContainerJson->TryGetArrayField("Children", children))
	{
		for (auto& childJsonValue : *children)
		{
			auto& childJsonObject = childJsonValue->AsObject();

			UAkAssetDataSwitchContainerData* childEntry = NewObject<UAkAssetDataSwitchContainerData>(parent);
			parseSwitchContainer(childJsonObject, childEntry, mediaList, parent);
			switchContainerEntry->Children.Add(childEntry);
		}
	}
}

bool AkSoundDataBuilder::parseAssetInfo(UAkAuxBus* akAuxBus, UAkAssetData* auxBusPlatformData, const FString& platform, const FString& language, const TSharedPtr<FJsonObject>& soundBankData, MediaToCookMap& mediaToCookMap)
{
	bool changed = false;

	const TArray<TSharedPtr<FJsonValue>>* auxBusses = nullptr;
	if (soundBankData->TryGetArrayField("IncludedAuxBusses", auxBusses))
	{
		for (auto& auxBusJsonValue : *auxBusses)
		{
			auto& auxBusJson = auxBusJsonValue->AsObject();
			FString auxBusStringId = auxBusJson->GetStringField("GUID");

			FGuid auxBusId;
			FGuid::ParseExact(auxBusStringId, EGuidFormats::DigitsWithHyphensInBraces, auxBusId);

			if (auxBusId == akAuxBus->ID)
			{
				FString stringId;
				if (auxBusJson->TryGetStringField("Id", stringId))
				{
					uint32 id = static_cast<uint32>(FCString::Atoi64(*stringId));
					if (akAuxBus->ShortID != id)
					{
						akAuxBus->ShortID = id;
						changed = true;
					}
				}

				break;
			}
		}
	}

	return changed;
}

bool AkSoundDataBuilder::GetAuxBusFromBankInfo(const TSharedPtr<FJsonObject>& soundBankData)
{
	auto& akAssetDatabase = AkAssetDatabase::Get();
	bool bChanged = false;
	const TArray<TSharedPtr<FJsonValue>>* auxBusses = nullptr;
	if (soundBankData->TryGetArrayField("IncludedAuxBusses", auxBusses))
	{
		for (auto& auxBusJsonValue : *auxBusses)
		{
			auto& auxBusJson = auxBusJsonValue->AsObject();
			FString auxBusStringId;
			FGuid auxBusId;

			if (auxBusJson->TryGetStringField("GUID", auxBusStringId) && !auxBusStringId.IsEmpty())
			{
				FGuid::ParseExact(auxBusStringId, EGuidFormats::DigitsWithHyphensInBraces, auxBusId);
			}

			UAkAuxBus* auxBusByName = nullptr;
			UAkAuxBus* const* akAuxBusIt = nullptr;

			akAuxBusIt = akAssetDatabase.AuxBusMap.Find(auxBusId);
			if (!akAuxBusIt)
			{
				auto foundAuxBusses = akAssetDatabase.AuxBusMap.FindByName(auxBusJson->GetStringField("Name"));
				if (foundAuxBusses.Num() > 0)
				{
					auxBusByName = foundAuxBusses[0];
					akAuxBusIt = &auxBusByName;
				}
			}

			if (akAuxBusIt)
			{
				auto akAuxBus = *akAuxBusIt;

				FString stringId;
				if (auxBusJson->TryGetStringField("Id", stringId))
				{
					uint32 id = static_cast<uint32>(FCString::Atoi64(*stringId));
					if (akAuxBus->ShortID != id)
					{
						akAuxBus->ShortID = id;
						markAssetDirty(akAuxBus);
						bChanged = true;
					}
				}
			}
		}
	}

	return bChanged;

}

bool AkSoundDataBuilder::parseAssetInfo(UAkInitBank* akInitBank, UAkAssetData* platformData, const FString& platform, const FString& language, const TSharedPtr<FJsonObject>& soundBankData, MediaToCookMap& mediaToCookMap)
{
	akInitBank->DefaultLanguage = defaultLanguage;
	return GetAuxBusFromBankInfo(soundBankData);
}

bool AkSoundDataBuilder::parseAssetInfo(UAkAudioBank* akAudioBank, UAkAssetData* platformData, const FString& platform, const FString& language, const TSharedPtr<FJsonObject>& soundBankData, MediaToCookMap& mediaToCookMap)
{
	auto& akAssetDatabase = AkAssetDatabase::Get();

	const TArray<TSharedPtr<FJsonValue>>* eventsArray = nullptr;
	if (soundBankData->TryGetArrayField("IncludedEvents", eventsArray))
	{
		bool isUsingNewAssetManagement = AkUnrealHelper::IsUsingEventBased();

		for (auto& eventJsonValue : *eventsArray)
		{
			auto& eventJson = eventJsonValue->AsObject();

			FString eventStringId;
			FGuid eventId;
			if (eventJson->TryGetStringField("GUID", eventStringId) && !eventStringId.IsEmpty())
			{ 
				FGuid::ParseExact(eventStringId, EGuidFormats::DigitsWithHyphensInBraces, eventId);
			}

			UAkAudioEvent* eventByName = nullptr;
			UAkAudioEvent* const* eventIt = nullptr;

			eventIt = akAssetDatabase.EventMap.Find(eventId);
			if (!eventIt)
			{
				auto foundEvents = akAssetDatabase.EventMap.FindByName(eventJson->GetStringField("Name"));
				if (foundEvents.Num() > 0)
				{
					eventByName = foundEvents[0];
					eventIt = &eventByName;
				}
			}

			if (eventIt)
			{
				auto sharedThis = SharedThis(this);

				struct EventRequiredData
				{
					UAkAudioEvent* akAudioEvent = nullptr;
					UAkAssetData* eventPlatformData = nullptr;
				};

				TSharedPtr<EventRequiredData> requiredData = MakeShared<EventRequiredData>();
				requiredData->akAudioEvent = *eventIt;

				auto fetchPlatformDataTask = FFunctionGraphTask::CreateAndDispatchWhenReady([requiredData, platform]()
				{
					if (requiredData->akAudioEvent)
					{
						requiredData->eventPlatformData = requiredData->akAudioEvent->FindOrAddAssetData(platform, FString());
					}
				}, GET_STATID(STAT_EventPlatformDataEventGroup), nullptr, ENamedThreads::GameThread);

				FFunctionGraphTask::CreateAndDispatchWhenReady([sharedThis, requiredData, eventJson, platform, language, isUsingNewAssetManagement]
				{
					if (!requiredData->akAudioEvent || !requiredData->eventPlatformData)
						return;

					bool changed = false;

					MediaToCookMap& mediaToCookMap = sharedThis->getMediaToCookMap(platform);
					changed |= sharedThis->parseEventInfo(requiredData->akAudioEvent, Cast<UAkAudioEventData>(requiredData->eventPlatformData), eventJson);

					if (!isUsingNewAssetManagement)
					{
						return;
					}

					if (auto* eventPlatformData = Cast<UAkAudioEventData>(requiredData->eventPlatformData))
					{
						if (language.Len() > 0)
						{
							auto localizedMediaInfo = eventPlatformData->FindOrAddLocalizedData(language);

							changed |= sharedThis->parseMedia(eventJson, mediaToCookMap, localizedMediaInfo->MediaList, platform, false);

							localizedMediaInfo->SwitchContainers.Empty();

							if (sharedThis->splitSwitchContainerMedia)
							{
								const TArray<TSharedPtr<FJsonValue>>* switchContainers = nullptr;
								if (eventJson->TryGetArrayField("SwitchContainers", switchContainers))
								{
									for (auto& switchContainerValueJson : *switchContainers)
									{
										auto& switchContainerJson = switchContainerValueJson->AsObject();

										UAkAssetDataSwitchContainerData* switchContainerEntry = NewObject<UAkAssetDataSwitchContainerData>(localizedMediaInfo);
										sharedThis->parseSwitchContainer(switchContainerJson, switchContainerEntry, localizedMediaInfo->MediaList, localizedMediaInfo);
										localizedMediaInfo->SwitchContainers.Add(switchContainerEntry);
									}
								}

								changed = true;
							}
						}
						else
						{
							changed |= sharedThis->parseMedia(eventJson, mediaToCookMap, eventPlatformData->MediaList, platform, false);

							eventPlatformData->SwitchContainers.Empty();

							if (sharedThis->splitSwitchContainerMedia)
							{
								const TArray<TSharedPtr<FJsonValue>>* switchContainers = nullptr;
								if (eventJson->TryGetArrayField("SwitchContainers", switchContainers))
								{
									for (auto& switchContainerValueJson : *switchContainers)
									{
										auto& switchContainerJson = switchContainerValueJson->AsObject();

										UAkAssetDataSwitchContainerData* switchContainerEntry = NewObject<UAkAssetDataSwitchContainerData>(eventPlatformData);
										sharedThis->parseSwitchContainer(switchContainerJson, switchContainerEntry, eventPlatformData->MediaList, eventPlatformData);
										eventPlatformData->SwitchContainers.Add(switchContainerEntry);
									}
								}

								changed = true;
							}
						}
					}

					if (changed)
					{
						sharedThis->markAssetDirty(requiredData->akAudioEvent);
						sharedThis->markAssetDirty(requiredData->eventPlatformData);
					}
				}, GET_STATID(STAT_ParseEventInfoEventGroup), fetchPlatformDataTask);
			}
		}
	}

	GetAuxBusFromBankInfo(soundBankData);

	return false;
}

bool AkSoundDataBuilder::parsePluginInfo(UAkInitBank* InitBank, const FString& Platform, const TSharedPtr<FJsonObject>& PluginInfoJson)
{
	auto& pluginsArrayJson = PluginInfoJson->GetArrayField(TEXT("Plugins"));

	auto platformData = InitBank->FindOrAddAssetDataTyped<UAkInitBankAssetData>(Platform, FString());
	if (!platformData)
	{
		return false;
	}

	platformData->PluginInfos.Empty();

	for (auto& pluginJsonValue : pluginsArrayJson)
	{
		auto& pluginJson = pluginJsonValue->AsObject();

		auto pluginDLL = pluginJson->GetStringField(TEXT("DLL"));
		if (pluginDLL == TEXT("DefaultSink"))
		{
			continue;
		}

		auto pluginName = pluginJson->GetStringField(TEXT("Name"));
		auto pluginStringID = pluginJson->GetStringField(TEXT("ID"));

		auto pluginID = static_cast<uint32>(FCString::Atoi64(*pluginStringID));

		platformData->PluginInfos.Emplace(pluginName, pluginID, pluginDLL);
	}

	if (platformData->PluginInfos.Num() > 0)
	{
		StaticPluginWriter::OutputPluginInformation(InitBank, Platform);
	}

	return true;
}

void AkSoundDataBuilder::cookMediaAsset(const MediaAssetToCook& mediaToCook, const FString& platform)
{
	if (!mediaToCook.Instance || !mediaToCook.CurrentPlatformData)
	{
		return;
	}

	bool changed = false;

	UAkMediaAsset* mediaAsset = mediaToCook.Instance;

	if (mediaAsset->Id != mediaToCook.Id)
	{
		mediaAsset->Id = mediaToCook.Id;
		changed = true;
	}

	if (mediaAsset->MediaName != mediaToCook.MediaName)
	{
		mediaAsset->MediaName = mediaToCook.MediaName;
		changed = true;
	}

	auto mediaFilePath = FPaths::Combine(cacheDirectory, platform, mediaToCook.CachePath);
	auto modificationTime = platformFile->GetTimeStamp(*mediaFilePath).ToUnixTimestamp();
	auto platformData = mediaToCook.CurrentPlatformData;
	const MediaAssetPerPlatformData* parsedPlatformData = mediaToCook.ParsedPerPlatformData.Find(platform);
	if (!parsedPlatformData)
	{
		return;
	}

	bool needToReadFile = false;
	if (platformData->IsStreamed != parsedPlatformData->IsStreamed)
	{
		platformData->IsStreamed = parsedPlatformData->IsStreamed;
		needToReadFile = true;
	}
	else if (platformData->DataChunks.Num() <= 0 || platformData->LastWriteTime != modificationTime)
	{
		needToReadFile = true;
	}
	else if (platformData->IsStreamed && platformData->DataChunks[0].IsPrefetch && platformData->DataChunks[0].Data.GetBulkDataSize() != parsedPlatformData->PrefetchSize)
	{
		needToReadFile = true;
	}

	if (platformData->UseDeviceMemory != parsedPlatformData->UseDeviceMemory)
	{
		platformData->UseDeviceMemory = parsedPlatformData->UseDeviceMemory;
	}

	if (needToReadFile)
	{
		TUniquePtr<IFileHandle> fileReader(platformFile->OpenRead(*mediaFilePath));
		if (fileReader)
		{
			{
				FScopeLock autoLock(&platformData->DataLock);
				platformData->DataChunks.Empty();
			}
			auto fileSize = fileReader->Size();

			auto bulkDataLoadingType = platformData->UseDeviceMemory ? BULKDATA_Force_NOT_InlinePayload : BULKDATA_ForceInlinePayload;

			if (parsedPlatformData->IsStreamed && parsedPlatformData->PrefetchSize > 0)
			{
				auto prefetchChunk = new FAkMediaDataChunk(fileReader.Get(), parsedPlatformData->PrefetchSize, bulkDataLoadingType, true);
				{
					FScopeLock autoLock(&platformData->DataLock);
					platformData->DataChunks.Add(prefetchChunk);
				}
			}

			fileReader->Seek(0);
			auto dataChunk = new FAkMediaDataChunk(fileReader.Get(), fileSize, parsedPlatformData->IsStreamed ? BULKDATA_Force_NOT_InlinePayload : bulkDataLoadingType);
			{
				FScopeLock autoLock(&platformData->DataLock);
				platformData->DataChunks.Add(dataChunk);
			}

			platformData->LastWriteTime = modificationTime;

			changed = true;
		}
	}

	if (changed)
	{
		markAssetDirty(mediaAsset);
	}
}

void AkSoundDataBuilder::dispatchAndWaitMediaCookTasks()
{
	for (auto& entry : mediaToCookPerPlatform)
	{
		dispatchMediaCookTask(entry.Value, entry.Key);
	}

	FTaskGraphInterface::Get().WaitUntilTasksComplete(allMediaTasks);
}

void AkSoundDataBuilder::dispatchMediaCookTask(const MediaToCookMap& mediaMap, const FString& platform)
{
	for (auto& entry : mediaMap)
	{
		auto& mediaToCook = entry.Value;

		auto sharedThis = SharedThis(this);

		auto fetchMediaTask = FFunctionGraphTask::CreateAndDispatchWhenReady([sharedThis, mediaToCook, platform] {
			FString assetPackagePath;
			UClass* mediaAssetClass = UAkMediaAsset::StaticClass();

			if (mediaToCook->Language.Len() > 0)
			{
				mediaAssetClass = UAkLocalizedMediaAsset::StaticClass();
				assetPackagePath = FPaths::Combine(sharedThis->localizedPackagePath, mediaToCook->Language, AkUnrealHelper::MediaFolderName);
			}
			else
			{
				assetPackagePath = FPaths::Combine(sharedThis->basePackagePath, AkUnrealHelper::MediaFolderName);
			}

			auto assetData = sharedThis->assetRegistryModule->Get().GetAssetByObjectPath(mediaToCook->AssetPath.GetAssetPathName());

			if (assetData.IsValid())
			{
				mediaToCook->Instance = Cast<UAkMediaAsset>(assetData.GetAsset());
			}
			else
			{
				mediaToCook->Instance = Cast<UAkMediaAsset>(sharedThis->assetToolsModule->Get().CreateAsset(mediaToCook->AssetPath.GetAssetName(), assetPackagePath, mediaAssetClass, nullptr));
			}

			if (mediaToCook->Instance)
			{
				mediaToCook->CurrentPlatformData = mediaToCook->Instance->FindOrAddMediaAssetData(platform);
			}
		}, GET_STATID(STAT_GatherMediaAssets), nullptr, ENamedThreads::GameThread);

		auto cookTask = FFunctionGraphTask::CreateAndDispatchWhenReady([sharedThis, mediaToCook, platform] {
			sharedThis->cookMediaAsset(*mediaToCook.Get(), platform);
		}, GET_STATID(STAT_CookMediaAsset), fetchMediaTask);

		{
			FScopeLock autoLock(&mediaLock);
			allMediaTasks.Add(cookTask);
		}
	}
}

void AkSoundDataBuilder::markAssetDirty(UObject* obj)
{
	AsyncTask(ENamedThreads::GameThread, [obj] {
		if (!obj->GetOutermost()->IsDirty())
		{
			obj->GetOutermost()->MarkPackageDirty();
		}
	});
}

void AkSoundDataBuilder::fillAudioBankInfoMap(AudioBankInfoMap& audioBankInfoMap, FillAudioBankInfoKind infoKind)
{
	auto& akAssetDatabase = AkAssetDatabase::Get();

	{
		FScopeLock autoLock(&akAssetDatabase.EventMap.CriticalSection);

		for (auto& eventEntry : akAssetDatabase.EventMap.TypeMap)
		{
			if (eventEntry.Value->RequiredBank)
			{
				if (!initParameters.SkipLanguages || (initParameters.SkipLanguages && eventEntry.Value->RequiredBank->LocalizedPlatformAssetDataMap.Num() == 0))
				{
					FString audioBankName;
					if (!AkToolBehavior::Get()->AkSoundDataBuilder_GetBankName(this, eventEntry.Value->RequiredBank, initParameters.BanksToGenerate, audioBankName))
					{
						continue;
					}

					auto& infoEntry = audioBankInfoMap.FindOrAdd(audioBankName);
					infoEntry.NeedsRebuild = infoEntry.NeedsRebuild || eventEntry.Value->NeedsRebuild();

					auto& eventSet = infoEntry.Events;
					switch (infoKind)
					{
						case FillAudioBankInfoKind::GUID:
							eventSet.Add(eventEntry.Value->ID.ToString(EGuidFormats::DigitsWithHyphensInBraces));
							break;
						case FillAudioBankInfoKind::AssetName:
							eventSet.Add(eventEntry.Value->GetName());
							break;
						default:
							break;
					}
				}
			}
		}
	}

	{
		FScopeLock autoLock(&akAssetDatabase.AuxBusMap.CriticalSection);

		for (auto& auxBusEntry : akAssetDatabase.AuxBusMap.TypeMap)
		{
			if (auxBusEntry.Value->RequiredBank)
			{
				FString audioBankName;

				if (!AkToolBehavior::Get()->AkSoundDataBuilder_GetBankName(this, auxBusEntry.Value->RequiredBank, initParameters.BanksToGenerate, audioBankName))
				{
					continue;
				}

				auto& infoEntry = audioBankInfoMap.FindOrAdd(audioBankName);
				infoEntry.NeedsRebuild = infoEntry.NeedsRebuild || auxBusEntry.Value->NeedsRebuild();

				auto& auxBusSet = infoEntry.AuxBusses;
				switch (infoKind)
				{
					case FillAudioBankInfoKind::GUID:
						auxBusSet.Add(auxBusEntry.Value->ID.ToString(EGuidFormats::DigitsWithHyphensInBraces));
						break;
					case FillAudioBankInfoKind::AssetName:
						auxBusSet.Add(auxBusEntry.Value->GetName());
						break;
					default:
						break;
				}
			}
		}
	}
}

FString AkSoundDataBuilder::guidToBankName(const FGuid& guid)
{
	return FString(AkSoundDataBuilder_Helpers::SoundBankNamePrefix) + guid.ToString(EGuidFormats::Digits);
}

FGuid AkSoundDataBuilder::bankNameToGuid(const FString& string)
{
	FString copy = string;
	copy.RemoveFromStart(AkSoundDataBuilder_Helpers::SoundBankNamePrefix);

	FGuid result;
	FGuid::ParseExact(copy, EGuidFormats::Digits, result);

	return result;
}

#undef LOCTEXT_NAMESPACE
