#include "WaapiAkSoundDataBuilder.h"

#include "AkAssetDatabase.h"
#include "AkAudioBank.h"
#include "AkAudioBankGenerationHelpers.h"
#include "AkAudioDevice.h"
#include "AkAudioEvent.h"
#include "AkAuxBus.h"
#include "AkInitBank.h"
#include "AkUnrealHelper.h"
#include "AkWaapiClient.h"
#include "AkWaapiUtils.h"
#include "AssetRegistry/Public/AssetRegistryModule.h"
#include "AssetTools/Public/AssetToolsModule.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Base64.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DECLARE_CYCLE_STAT(TEXT("AkSoundData - Waapi List Event"), STAT_WaapiListEvent, STATGROUP_AkSoundDataSource);
DECLARE_CYCLE_STAT(TEXT("AkSoundData - Waapi List AuxBus"), STAT_WaapiListAuxBus, STATGROUP_AkSoundDataSource);
DECLARE_CYCLE_STAT(TEXT("AkSoundData - Waapi Call"), STAT_WaapiCall, STATGROUP_AkSoundDataSource);
DECLARE_CYCLE_STAT(TEXT("AkSoundData - Waapi Parse JSON response"), STAT_WaapiParseJSONResponse, STATGROUP_AkSoundDataSource);
DECLARE_CYCLE_STAT(TEXT("AkSoundData - Waapi DoWork"), STAT_WaapiDoWork, STATGROUP_AkSoundDataSource);

DECLARE_CYCLE_STAT(TEXT("AkSoundData - Waapi Gather Platform Data"), STAT_WaapiGatherPlatformData, STATGROUP_AkSoundDataSource);
DECLARE_CYCLE_STAT(TEXT("AkSoundData - Waapi Process JSON"), STAT_WaapiProcessJSON, STATGROUP_AkSoundDataSource);
DECLARE_CYCLE_STAT(TEXT("AkSoundData - Waapi Bank data"), STAT_WaapiBankData, STATGROUP_AkSoundDataSource);

WaapiAkSoundDataBuilder::WaapiAkSoundDataBuilder(const InitParameters& InitParameter)
: AkSoundDataBuilder(InitParameter)
{
}

WaapiAkSoundDataBuilder::~WaapiAkSoundDataBuilder()
{
	TSharedPtr<FJsonObject> result;
	FAkWaapiClient::Get()->Unsubscribe(_generatedSubscriptionId, result);
}

void WaapiAkSoundDataBuilder::Init()
{
	AkSoundDataBuilder::Init();
	
	TSharedPtr<FJsonObject> result;
	TSharedRef<FJsonObject> generatedOptions = MakeShareable(new FJsonObject());
	generatedOptions->SetBoolField(WwiseWaapiHelper::INFO_FILE, true);
	generatedOptions->SetBoolField(WwiseWaapiHelper::BANK_DATA, true);
	generatedOptions->SetBoolField(WwiseWaapiHelper::PLUGININFO_OPTIONS, true);
	auto soundBankGeneratedCallback = WampEventCallback::CreateRaw(this, &WaapiAkSoundDataBuilder::onSoundBankGenerated);
	FAkWaapiClient::Get()->Subscribe(ak::wwise::core::soundbank::generated, generatedOptions, soundBankGeneratedCallback, _generatedSubscriptionId, result);
}

void WaapiAkSoundDataBuilder::DoWork()
{
	if (!_generatedSubscriptionId)
	{
		return;
	}

	createNotificationItem();

	TSharedRef<FJsonObject> args = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> options = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> result;

	if (FAkWaapiClient::Get()->Call(ak::wwise::core::remote::getConnectionStatus, args, options, result, -1))
	{
		bool isConnected = false;
		if (result->TryGetBoolField(WwiseWaapiHelper::IS_CONNECTED, isConnected) && isConnected)
		{
			notifyProfilingInProgress();
			return;
		}
	}

	AutoSetIsBuilding autoSetIsBuilding;

	auto start = FPlatformTime::Cycles64();

	SCOPE_CYCLE_COUNTER(STAT_WaapiDoWork);

	TArray<TSharedPtr<FJsonValue>> platformJsonArray;
	for (auto& platform : initParameters.Platforms)
	{
		platformJsonArray.Add(MakeShared<FJsonValueString>(platform));
	}
	args->SetArrayField(WwiseWaapiHelper::PLATFORMS, platformJsonArray);

	if (initParameters.Languages.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> languageJsonArray;
		for (auto& language : initParameters.Languages)
		{
			languageJsonArray.Add(MakeShared<FJsonValueString>(language.ID.ToString(EGuidFormats::DigitsWithHyphensInBraces)));
		}
		args->SetArrayField(WwiseWaapiHelper::LANGUAGES, languageJsonArray);
	}

	TArray<TSharedPtr<FJsonValue>> soundBanks;
	
	{
		SCOPE_CYCLE_COUNTER(STAT_WaapiListEvent);

		TArray<TSharedPtr<FJsonValue>> eventInclusionArray
		{
			MakeShared<FJsonValueString>(WwiseWaapiHelper::EVENT),
			MakeShared<FJsonValueString>(WwiseWaapiHelper::STRUCTURE)
		};

		AudioBankInfoMap audioBankInfoMap;
		fillAudioBankInfoMap(audioBankInfoMap, FillAudioBankInfoKind::GUID);
		
		{
			FScopeLock autoEventLock(&AkAssetDatabase::Get().EventMap.CriticalSection);

			for (auto& eventEntry : AkAssetDatabase::Get().EventMap.TypeMap)
			{
				if (!eventEntry.Value->RequiredBank && eventEntry.Value->ID.IsValid())
				{
					if (!initParameters.SkipLanguages || (initParameters.SkipLanguages && eventEntry.Value->LocalizedPlatformAssetDataMap.Num() == 0))
					{
						TSharedPtr<FJsonObject> soundBankEntry = MakeShareable(new FJsonObject());
						soundBankEntry->SetStringField(WwiseWaapiHelper::NAME, guidToBankName(eventEntry.Key));
						soundBankEntry->SetArrayField(WwiseWaapiHelper::EVENTS, { MakeShared<FJsonValueString>(eventEntry.Value->ID.ToString(EGuidFormats::DigitsWithHyphensInBraces)) });
						soundBankEntry->SetArrayField(WwiseWaapiHelper::INCLUSIONS, eventInclusionArray);
						soundBankEntry->SetBoolField(WwiseWaapiHelper::REBUILD, eventEntry.Value->NeedsRebuild());
						soundBanks.Add(MakeShared<FJsonValueObject>(soundBankEntry));
					}
				}
			}
		}

		for (auto& audioBankEntry : audioBankInfoMap)
		{
			TSharedPtr<FJsonObject> soundBankEntry = MakeShareable(new FJsonObject());
			soundBankEntry->SetStringField(WwiseWaapiHelper::NAME, audioBankEntry.Key);

			TArray<TSharedPtr<FJsonValue>> eventArray;
			for (auto& eventName : audioBankEntry.Value.Events)
			{
				eventArray.Add(MakeShared<FJsonValueString>(eventName));
			}
			soundBankEntry->SetArrayField(WwiseWaapiHelper::EVENTS, eventArray);

			TArray<TSharedPtr<FJsonValue>> auxBusArray;
			for (auto& auxBusName : audioBankEntry.Value.AuxBusses)
			{
				auxBusArray.Add(MakeShared<FJsonValueString>(auxBusName));
			}
			soundBankEntry->SetArrayField(WwiseWaapiHelper::AUX_BUSSES, auxBusArray);

			soundBankEntry->SetArrayField(WwiseWaapiHelper::INCLUSIONS, eventInclusionArray);
			soundBankEntry->SetBoolField(WwiseWaapiHelper::REBUILD, audioBankEntry.Value.NeedsRebuild);
			soundBanks.Add(MakeShared<FJsonValueObject>(soundBankEntry));
		}
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_WaapiListAuxBus);
	
		TArray<TSharedPtr<FJsonValue>> auxBusInclusionArray
		{
			MakeShared<FJsonValueString>(WwiseWaapiHelper::STRUCTURE)
		};

		FScopeLock autoAuxBusLock(&AkAssetDatabase::Get().AuxBusMap.CriticalSection);

		for (auto& auxBusEntry : AkAssetDatabase::Get().AuxBusMap.TypeMap)
		{
			if (!auxBusEntry.Value->RequiredBank && auxBusEntry.Value->ID.IsValid())
			{
				TSharedPtr<FJsonObject> soundBankEntry = MakeShareable(new FJsonObject());
				soundBankEntry->SetStringField(WwiseWaapiHelper::NAME, guidToBankName(auxBusEntry.Key));
				soundBankEntry->SetArrayField(WwiseWaapiHelper::AUX_BUSSES, { MakeShared<FJsonValueString>(auxBusEntry.Value->ID.ToString(EGuidFormats::DigitsWithHyphensInBraces)) });
				soundBankEntry->SetArrayField(WwiseWaapiHelper::INCLUSIONS, auxBusInclusionArray);
				soundBankEntry->SetBoolField(WwiseWaapiHelper::REBUILD, auxBusEntry.Value->NeedsRebuild());
				soundBanks.Add(MakeShared<FJsonValueObject>(soundBankEntry));
			}
		}
	}

	args->SetArrayField(WwiseWaapiHelper::SOUNDBANKS, soundBanks);
	args->SetBoolField(WwiseWaapiHelper::SKIP_LANGUAGES, initParameters.SkipLanguages);

	auto succeeded = true;

	{
		SCOPE_CYCLE_COUNTER(STAT_WaapiCall);
		succeeded = FAkWaapiClient::Get()->Call(ak::wwise::core::soundbank::generate, args, options, result, -1);
	}

	FTaskGraphInterface::Get().WaitUntilTasksComplete(allParseTask);

	dispatchAndWaitMediaCookTasks();

	auto end = FPlatformTime::Cycles64();
	if (succeeded)
	{
		notifyGenerationSucceeded();
	}
	else
	{
		auto message = result->GetStringField(WwiseWaapiHelper::MESSSAGE);
		UE_LOG(LogAkAudio, Error, TEXT("WAAPI Sound Data generation failed: %s"), *message);
		notifyGenerationFailed();
	}

	UE_LOG(LogAkSoundData, Display, TEXT("WAAPI Sound Data Builder task took %f seconds."), FPlatformTime::ToSeconds64(end - start));

	if (!IsRunningCommandlet())
	{
		AsyncTask(ENamedThreads::Type::GameThread, []
		{
			if (auto audioDevice = FAkAudioDevice::Get())
			{
				audioDevice->ReloadAllSoundData();
			}
		});
	}
}

void WaapiAkSoundDataBuilder::onSoundBankGenerated(uint64_t id, TSharedPtr<FJsonObject> responseJson)
{
	SCOPE_CYCLE_COUNTER(STAT_WaapiParseJSONResponse);

	auto& akAssetDatabase = AkAssetDatabase::Get();

	FString soundBankName;
	FString platformName;
	FString language;

	if (responseJson->HasField(WwiseWaapiHelper::SOUNDBANK))
	{
		TSharedPtr<FJsonObject> soundBankJson = responseJson->GetObjectField(WwiseWaapiHelper::SOUNDBANK);
		soundBankName = soundBankJson->GetStringField(WwiseWaapiHelper::NAME);
	}

	if (responseJson->HasField(WwiseWaapiHelper::PLATFORM))
	{
		TSharedPtr<FJsonObject> platformJson = responseJson->GetObjectField(WwiseWaapiHelper::PLATFORM);

		platformName = platformJson->GetStringField(WwiseWaapiHelper::NAME);
	}

	if (responseJson->HasField(WwiseWaapiHelper::LANGUAGE))
	{
		TSharedPtr<FJsonObject> languageJson = responseJson->GetObjectField(WwiseWaapiHelper::LANGUAGE);

		language = languageJson->GetStringField(WwiseWaapiHelper::NAME);

		{
			FScopeLock autoLock(&akAssetDatabase.InitBankLock);
			akAssetDatabase.InitBank->AvailableAudioCultures.AddUnique(language);
		}
	}

	if (responseJson->HasField(WwiseWaapiHelper::BANK_INFO))
	{
		auto infoJsonArray = responseJson->GetArrayField(WwiseWaapiHelper::BANK_INFO);
		if (infoJsonArray.Num() == 0)
			return;

		auto bankGuid = bankNameToGuid(soundBankName);

		auto infoJson = infoJsonArray[0]->AsObject();
		auto akAudioEventIt = akAssetDatabase.EventMap.Find(bankGuid);
		auto akAuxBusIt = akAssetDatabase.AuxBusMap.Find(bankGuid);
		auto akAudioBankIt = akAssetDatabase.BankMap.Find(bankGuid);

		auto sharedThis = SharedThis(this);

		if (akAudioEventIt)
		{
			struct EventRequiredData
			{
				UAkAudioEvent* akAudioEvent = nullptr;
				UAkAssetData* eventPlatformData = nullptr;
			};

			TSharedPtr<EventRequiredData> requiredData = MakeShared<EventRequiredData>();
			requiredData->akAudioEvent = *akAudioEventIt;

			auto fetchPlatformDataTask = FFunctionGraphTask::CreateAndDispatchWhenReady([requiredData, language, platformName]
			{
				if (requiredData->akAudioEvent)
				{
					requiredData->eventPlatformData = requiredData->akAudioEvent->FindOrAddAssetData(platformName, language);
				}
			}, GET_STATID(STAT_WaapiGatherPlatformData), nullptr, ENamedThreads::GameThread);

			auto parseTask = FFunctionGraphTask::CreateAndDispatchWhenReady([sharedThis, requiredData, platformName, language, responseJson, infoJson]
			{
				if (requiredData->akAudioEvent && requiredData->eventPlatformData)
				{
					bool changed = false;

					changed |= sharedThis->parseBankData(requiredData->eventPlatformData->Data, responseJson);
					changed |= sharedThis->parseSoundBankInfo(requiredData->akAudioEvent, requiredData->eventPlatformData, platformName, language, infoJson, false);

					if (changed)
					{
						sharedThis->markAssetDirty(requiredData->akAudioEvent);
						sharedThis->markAssetDirty(requiredData->eventPlatformData);
					}
				}
			}, GET_STATID(STAT_WaapiProcessJSON), fetchPlatformDataTask);

			{
				FScopeLock autoLock(&parseTasksLock);
				allParseTask.Add(parseTask);
			}
		}
		else if (akAuxBusIt)
		{
			auto parseTask = FFunctionGraphTask::CreateAndDispatchWhenReady([sharedThis, akAuxBus = *akAuxBusIt, platformName, responseJson, infoJson] {
				if (akAuxBus)
				{
					UAkAssetData* auxBusPlatformData = akAuxBus->FindOrAddAssetData(platformName, FString());

					if (auxBusPlatformData)
					{
						bool changed = false;

						changed |= sharedThis->parseBankData(auxBusPlatformData->Data, responseJson);
						changed |= sharedThis->parseSoundBankInfo(akAuxBus, auxBusPlatformData, platformName, FString(), infoJson, false);

						if (changed)
						{
							sharedThis->markAssetDirty(akAuxBus);
						}
					}
				}
			}, GET_STATID(STAT_WaapiProcessJSON));

			{
				FScopeLock autoLock(&parseTasksLock);
				allParseTask.Add(parseTask);
			}
		}
		else if (akAudioBankIt)
		{
			struct AudioBankRequiredData
			{
				UAkAudioBank* akAudioBank = nullptr;
				UAkAssetData* audioBankPlatformData = nullptr;
			};

			TSharedPtr<AudioBankRequiredData> requiredData = MakeShared<AudioBankRequiredData>();
			requiredData->akAudioBank = *akAudioBankIt;

			auto fetchPlatformDataTask = FFunctionGraphTask::CreateAndDispatchWhenReady([requiredData, language, platformName]
			{
				if (requiredData->akAudioBank)
				{
					requiredData->audioBankPlatformData = requiredData->akAudioBank->FindOrAddAssetData(platformName, language);
				}
			}, GET_STATID(STAT_WaapiGatherPlatformData), nullptr, ENamedThreads::GameThread);

			auto parseTask = FFunctionGraphTask::CreateAndDispatchWhenReady([sharedThis, requiredData, platformName, language, responseJson, infoJson]
			{
				if (requiredData->akAudioBank && requiredData->audioBankPlatformData)
				{
					bool changed = false;

					changed |= sharedThis->parseBankData(requiredData->audioBankPlatformData->Data, responseJson);
					changed |= sharedThis->parseSoundBankInfo(requiredData->akAudioBank, requiredData->audioBankPlatformData, platformName, language, infoJson, false);

					if (changed)
					{
						sharedThis->markAssetDirty(requiredData->akAudioBank);
						sharedThis->markAssetDirty(requiredData->audioBankPlatformData);
					}
				}
			}, GET_STATID(STAT_WaapiProcessJSON), fetchPlatformDataTask);

			{
				FScopeLock autoLock(&parseTasksLock);
				allParseTask.Add(parseTask);
			}
		}
		else if (soundBankName == AkInitBankName)
		{
			auto parseTask = FFunctionGraphTask::CreateAndDispatchWhenReady([sharedThis, platformName, responseJson, infoJson, InitBank = akAssetDatabase.InitBank] {
				UAkAssetData* initBankPlatformData = InitBank->FindOrAddAssetData(platformName, FString());

				if (initBankPlatformData)
				{
					bool changed = false;

					changed |= sharedThis->parseBankData(initBankPlatformData->Data, responseJson);
					changed |= sharedThis->parseSoundBankInfo(InitBank, initBankPlatformData, platformName, FString(), infoJson, true);

					if (responseJson->HasField(WwiseWaapiHelper::PLUGININFO_RESPONSE))
					{
						changed |= sharedThis->parsePluginInfo(InitBank, platformName, responseJson->GetObjectField(WwiseWaapiHelper::PLUGININFO_RESPONSE));
					}

					if (changed)
					{
						sharedThis->markAssetDirty(InitBank);
					}
				}
			}, GET_STATID(STAT_WaapiProcessJSON));

			{
				FScopeLock autoLock(&parseTasksLock);
				allParseTask.Add(parseTask);
			}
		}
	}
}

bool WaapiAkSoundDataBuilder::parseBankData(FByteBulkData& bulkBankData, TSharedPtr<FJsonObject> responseJson)
{
	if (responseJson->HasField(WwiseWaapiHelper::BANK_DATA))
	{
		SCOPE_CYCLE_COUNTER(STAT_WaapiBankData);

		auto& bankDataJson = responseJson->GetObjectField(WwiseWaapiHelper::BANK_DATA);

		int32 dataSize = bankDataJson->GetIntegerField(WwiseWaapiHelper::SIZE);

		FString stringData = bankDataJson->GetStringField(WwiseWaapiHelper::DATA);

		if (bulkBankData.IsLocked())
		{
			bulkBankData.Unlock();
		}

		bulkBankData.Lock(EBulkDataLockFlags::LOCK_READ_WRITE);
		void* rawBankData = bulkBankData.Realloc(dataSize);
		FBase64::Decode(*stringData, stringData.Len(), reinterpret_cast<uint8*>(rawBankData));
		bulkBankData.Unlock();

		return true;
	}

	return false;
}
