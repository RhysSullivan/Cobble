#include "AkInitBank.h"

UAkAssetData* UAkInitBank::createAssetData(UObject* parent) const
{
	return NewObject<UAkInitBankAssetData>(parent);
}

#if WITH_EDITOR
void UAkInitBank::Reset()
{
	AvailableAudioCultures.Empty();

	Super::Reset();
}
#endif
