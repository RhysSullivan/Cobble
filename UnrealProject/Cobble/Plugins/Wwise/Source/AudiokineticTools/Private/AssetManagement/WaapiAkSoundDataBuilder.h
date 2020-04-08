#pragma once

#include "AkSoundDataBuilder.h"

#include "Async/Future.h"
#include "Containers/UnrealString.h"

class FJsonObject;
class UAkAudioEvent;
class UAkAuxBus;
class UAkInitBank;

class WaapiAkSoundDataBuilder : public AkSoundDataBuilder
{
public:
	WaapiAkSoundDataBuilder(const InitParameters& InitParameter);
	~WaapiAkSoundDataBuilder();

	void Init() override;

	void DoWork() override;

private:
	void onSoundBankGenerated(uint64_t id, TSharedPtr<FJsonObject> responseJson);

	bool parseBankData(FByteBulkData& bulkBankData, TSharedPtr<FJsonObject> responseJson);

private:
	uint64 _generatedSubscriptionId = 0;
	FDelegateHandle _connectionLostHandle;

	FCriticalSection parseTasksLock;
	FGraphEventArray allParseTask;
};
