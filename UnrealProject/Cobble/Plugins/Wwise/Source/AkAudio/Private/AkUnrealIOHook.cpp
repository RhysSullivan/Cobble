// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved
#include "AkUnrealIOHook.h"

#include "AkAudioDevice.h"
#include "AkMediaAsset.h"
#include "AkUnrealHelper.h"
#include "AssetRegistry/Public/AssetRegistryModule.h"
#include "Async/AsyncFileHandle.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "Async/Async.h"
#endif

struct AkFileCustomParam
{
	virtual ~AkFileCustomParam() {}
	virtual AKRESULT DoWork(AkAsyncIOTransferInfo& io_transferInfo) = 0;

	static void SetupAkFileDesc(AkFileDesc& fileDesc, AkFileCustomParam* customParam)
	{
		fileDesc.pCustomParam = customParam;
		fileDesc.uCustomParamSize = sizeof(AkFileCustomParam);
	}

	static AkFileCustomParam* GetFileCustomParam(const AkFileDesc& fileDesc)
	{
		if (fileDesc.uCustomParamSize == sizeof(AkFileCustomParam))
		{
			return reinterpret_cast<AkFileCustomParam*>(fileDesc.pCustomParam);
		}

		return nullptr;
	}
};

struct AkReadAssetCustomParam : public AkFileCustomParam
{
public:
	AkReadAssetCustomParam(IAsyncReadFileHandle* handle, int64 bulkDataOffset)
	: IORequestHandle(handle), BulkDataOffset(bulkDataOffset)
	{}

	virtual ~AkReadAssetCustomParam()
	{
		for (auto& ioRequest : IORequests)
		{
			if (ioRequest)
			{
				ioRequest->WaitCompletion();
				delete ioRequest;
			}
		}

		delete IORequestHandle;
	}

	AKRESULT DoWork(AkAsyncIOTransferInfo& io_transferInfo) override
	{
		auto AkTransferInfo = io_transferInfo;

		for (auto& ioRequest : IORequests)
		{
			if (ioRequest && ioRequest->PollCompletion())
			{
				ioRequest->WaitCompletion();
				delete ioRequest;
				ioRequest = nullptr;
			}
		}

		IORequests.Remove(nullptr);

		FAsyncFileCallBack AsyncFileCallBack = [AkTransferInfo](bool bWasCancelled, IAsyncReadRequest* Req) mutable
		{
			if (AkTransferInfo.pCallback)
			{
				AkTransferInfo.pCallback(&AkTransferInfo, AK_Success);
			}
		};
		auto PendingReadRequest = IORequestHandle->ReadRequest(BulkDataOffset + AkTransferInfo.uFilePosition, AkTransferInfo.uRequestedSize, AIOP_High, &AsyncFileCallBack, (uint8*)AkTransferInfo.pBuffer);
		if (PendingReadRequest)
		{
			IORequests.Add(PendingReadRequest);
		}

		return AK_Success;
	}

public:
	IAsyncReadFileHandle* IORequestHandle = nullptr;
	TArray<IAsyncReadRequest*, TInlineAllocator<8>> IORequests;
	int64 BulkDataOffset = 0;
};

struct AkEditorReadCustomParam : AkFileCustomParam
{
public:
	AkEditorReadCustomParam(const FByteBulkData& BulkData)
	{
		RawData = BulkData.LockReadOnly();
		BulkData.Unlock();
	}

	~AkEditorReadCustomParam()
	{
		RawData = nullptr;
	}

	AKRESULT DoWork(AkAsyncIOTransferInfo& io_transferInfo) override
	{
		if (io_transferInfo.pCallback)
		{
			FMemory::Memcpy(io_transferInfo.pBuffer, reinterpret_cast<const uint8*>(RawData) + io_transferInfo.uFilePosition, io_transferInfo.uRequestedSize);
			io_transferInfo.pCallback(&io_transferInfo, AK_Success);
		}

		return AK_Success;
	}

public:
	const void* RawData = nullptr;
};

struct AkWriteFileCustomParam : AkFileCustomParam
{
private:
	IFileHandle* FileHandle;

public:
	AkWriteFileCustomParam(IFileHandle* handle)
	: FileHandle(handle)
	{}

	virtual ~AkWriteFileCustomParam()
	{
		if (FileHandle)
		{
			delete FileHandle;
		}
	}

	virtual AKRESULT DoWork(AkAsyncIOTransferInfo& io_transferInfo) override
	{
		FileHandle->Seek(io_transferInfo.uFilePosition);

		if (FileHandle->Write((uint8*)io_transferInfo.pBuffer, io_transferInfo.uBufferSize))
		{
			if (io_transferInfo.pCallback)
			{
				io_transferInfo.pCallback(&io_transferInfo, AK_Success);
			}

			return AK_Success;
		}

		return AK_Fail;
	}
};

FAkUnrealIOHook::FAkUnrealIOHook()
{
	assetRegistryModule = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	localizedPackagePath = AkUnrealHelper::GetLocalizedAssetPackagePath();
	mediaPackagePath = FPaths::Combine(AkUnrealHelper::GetBaseAssetPackagePath(), AkUnrealHelper::MediaFolderName);
	externalSourcePackagePath = AkUnrealHelper::GetExternalSourceAssetPackagePath();
}

FAkUnrealIOHook::~FAkUnrealIOHook()
{
	if (AK::StreamMgr::GetFileLocationResolver() == this)
	{
		AK::StreamMgr::SetFileLocationResolver(nullptr);
	}

	AK::StreamMgr::DestroyDevice(m_deviceID);
}

bool FAkUnrealIOHook::Init(const AkDeviceSettings& in_deviceSettings)
{
	if (in_deviceSettings.uSchedulerTypeFlags != AK_SCHEDULER_DEFERRED_LINED_UP)
	{
		AKASSERT(!"CAkDefaultIOHookDeferred I/O hook only works with AK_SCHEDULER_DEFERRED_LINED_UP devices");
		return false;
	}

	// If the Stream Manager's File Location Resolver was not set yet, set this object as the 
	// File Location Resolver (this I/O hook is also able to resolve file location).
	if (!AK::StreamMgr::GetFileLocationResolver())
	{
		AK::StreamMgr::SetFileLocationResolver(this);
	}

	// Create a device in the Stream Manager, specifying this as the hook.
	m_deviceID = AK::StreamMgr::CreateDevice(in_deviceSettings, this);
	return m_deviceID != AK_INVALID_DEVICE_ID;
}

AKRESULT FAkUnrealIOHook::Open(
	const AkOSChar*			in_pszFileName,
	AkOpenMode				in_eOpenMode,
	AkFileSystemFlags*		in_pFlags,
	bool&					io_bSyncOpen,
	AkFileDesc&			out_fileDesc
)
{
	cleanFileDescriptor(out_fileDesc);

	if (in_eOpenMode == AK_OpenModeRead && in_pFlags->uCompanyID == AKCOMPANYID_AUDIOKINETIC_EXTERNAL)
	{
		auto assetName = FPaths::GetBaseFilename(in_pszFileName);
		auto assetPath = FPaths::Combine(externalSourcePackagePath, FString::Format(TEXT("{0}.{0}"), { *assetName }));
		if (assetPath.Len() == 0)
		{
			return AK_Fail;
		}

		auto externalSourceAsset = GetExternalSourceAsset(assetPath);
		if (!externalSourceAsset)
		{
			return AK_Fail;
		}
		auto result = doAssetOpen(externalSourceAsset, out_fileDesc);
		externalSourceAsset->RemoveFromRoot();
		return result;
	}

	if (in_eOpenMode == AK_OpenModeWrite || in_eOpenMode == AK_OpenModeWriteOvrwr)
	{
		auto fullPath = FPaths::Combine(FAkAudioDevice::Get()->GetBasePath(), FString(in_pszFileName));
		if (auto FileHandle = FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*fullPath))
		{
			io_bSyncOpen = true;
			AkFileCustomParam::SetupAkFileDesc(out_fileDesc, new AkWriteFileCustomParam(FileHandle));
			return AK_Success;
		}
	}

	return AK_NotImplemented;
}

AKRESULT FAkUnrealIOHook::Open(
	AkFileID				in_fileID,          // File ID.
	AkOpenMode				in_eOpenMode,       // Open mode.
	AkFileSystemFlags*		in_pFlags,			// Special flags. Can pass NULL.
	bool&					io_bSyncOpen,		// If true, the file must be opened synchronously. Otherwise it is left at the File Location Resolver's discretion. Return false if Open needs to be deferred.
	AkFileDesc&			out_fileDesc        // Returned file descriptor.
)
{
	cleanFileDescriptor(out_fileDesc);

	if (in_eOpenMode != AK_OpenModeRead)
	{
		return AK_NotImplemented;
	}

	FString assetPath;

	if (in_pFlags->bIsLanguageSpecific)
	{
		if (auto* audioDevice = FAkAudioDevice::Get())
		{
			assetPath = FPaths::Combine(localizedPackagePath, audioDevice->GetCurrentAudioCulture(), AkUnrealHelper::MediaFolderName, FString::Printf(TEXT("%u.%u"), in_fileID, in_fileID));

			if (!assetRegistryModule->Get().GetAssetByObjectPath(FName(*assetPath)).IsValid())
			{
				assetPath = FPaths::Combine(localizedPackagePath, audioDevice->GetDefaultLanguage(), AkUnrealHelper::MediaFolderName, FString::Printf(TEXT("%u.%u"), in_fileID, in_fileID));
				if (!assetRegistryModule->Get().GetAssetByObjectPath(FName(*assetPath)).IsValid())
				{
					return AK_Fail;
				}
			}
		}
	}
	else
	{
		assetPath = FPaths::Combine(mediaPackagePath, FString::Printf(TEXT("%u.%u"), in_fileID, in_fileID));
	}

	if (assetPath.Len() == 0)
	{
		return AK_Fail;
	}

	return doAssetOpen(GetAsset(assetPath), out_fileDesc);
}

UAkMediaAsset* FAkUnrealIOHook::GetAsset(const FString &assetPath)
{
	auto assetData = assetRegistryModule->Get().GetAssetByObjectPath(FName(*assetPath));
	return !assetData.IsValid() || !assetData.IsAssetLoaded() ? nullptr : Cast<UAkMediaAsset>(assetData.GetAsset());
}

UAkMediaAsset* FAkUnrealIOHook::GetExternalSourceAsset(const FString &assetPath)
{
	auto assetData = assetRegistryModule->Get().GetAssetByObjectPath(FName(*assetPath));
	if (!assetData.IsValid())
	{
		return nullptr;
	}

	if (assetData.IsAssetLoaded())
	{
		return Cast<UAkMediaAsset>(assetData.GetAsset());
	}

	return nullptr;
}

AKRESULT FAkUnrealIOHook::doAssetOpen(UAkMediaAsset* mediaAsset, AkFileDesc& out_fileDesc)
{
	if (!mediaAsset)
	{
		return AK_Fail;
	}

	auto streamedChunk = mediaAsset->GetStreamedChunk();
	if (!streamedChunk)
	{
		return AK_Fail;
	}

	out_fileDesc.iFileSize = streamedChunk->Data.GetBulkDataSize();

#if WITH_EDITOR
	if (streamedChunk->Data.GetBulkDataOffsetInFile() == -1 || streamedChunk->Data.GetFilename().Len() == 0)
	{
		auto customParam = new AkEditorReadCustomParam(streamedChunk->Data);
		AkFileCustomParam::SetupAkFileDesc(out_fileDesc, customParam);
		return AK_Success;
	}
#endif

	auto ioRequestHandle = FPlatformFileManager::Get().GetPlatformFile().OpenAsyncRead(*streamedChunk->Data.GetFilename());
	if (!ioRequestHandle)
	{
		return AK_Fail;
	}

	auto customParam = new AkReadAssetCustomParam(ioRequestHandle, streamedChunk->Data.GetBulkDataOffsetInFile());
	AkFileCustomParam::SetupAkFileDesc(out_fileDesc, customParam);
	return AK_Success;
}

AKRESULT FAkUnrealIOHook::Read(
	AkFileDesc &			in_fileDesc,
	const AkIoHeuristics &	in_heuristics,
	AkAsyncIOTransferInfo & io_transferInfo
)
{
	auto fileCustomParam = AkFileCustomParam::GetFileCustomParam(in_fileDesc);
	if (fileCustomParam)
	{
		return fileCustomParam->DoWork(io_transferInfo);
	}
	
	return AK_Fail;
}

AKRESULT FAkUnrealIOHook::Write(
	AkFileDesc &			in_fileDesc,
	const AkIoHeuristics&	in_heuristics,
	AkAsyncIOTransferInfo& io_transferInfo
)
{
	auto fileCustomParam = AkFileCustomParam::GetFileCustomParam(in_fileDesc);
	if (fileCustomParam)
	{
		return fileCustomParam->DoWork(io_transferInfo);
	}

	return AK_Fail;
}

void FAkUnrealIOHook::Cancel(
	AkFileDesc &			in_fileDesc,
	AkAsyncIOTransferInfo & io_transferInfo,
	bool & io_bCancelAllTransfersForThisFile
)
{
}

AKRESULT FAkUnrealIOHook::Close(AkFileDesc& in_fileDesc)
{
	auto fileCustomParam = AkFileCustomParam::GetFileCustomParam(in_fileDesc);
	if (fileCustomParam)
	{
		delete fileCustomParam;
	}

	AkFileCustomParam::SetupAkFileDesc(in_fileDesc, nullptr);
	return AK_Success;
}

// Returns the block size for the file or its storage device. 
AkUInt32 FAkUnrealIOHook::GetBlockSize(AkFileDesc& in_fileDesc)
{
	return 1;
}

// Returns a description for the streaming device above this low-level hook.
void FAkUnrealIOHook::GetDeviceDesc(AkDeviceDesc& 
#if !defined(AK_OPTIMIZED)
	out_deviceDesc
#endif
)
{
#if !defined(AK_OPTIMIZED)
	// Deferred scheduler.
	out_deviceDesc.deviceID = m_deviceID;
	out_deviceDesc.bCanRead = true;
	out_deviceDesc.bCanWrite = true;
	AK_CHAR_TO_UTF16(out_deviceDesc.szDeviceName, "UnrealIODevice", AK_MONITOR_DEVICENAME_MAXLENGTH);
	out_deviceDesc.uStringSize = (AkUInt32)AKPLATFORM::AkUtf16StrLen(out_deviceDesc.szDeviceName) + 1;
#endif
}

// Returns custom profiling data: 1 if file opens are asynchronous, 0 otherwise.
AkUInt32 FAkUnrealIOHook::GetDeviceData()
{
	return 1;
}

void FAkUnrealIOHook::cleanFileDescriptor(AkFileDesc& out_fileDesc)
{
	out_fileDesc.uSector = 0;
	out_fileDesc.deviceID = m_deviceID;

	auto fileCustomParam = AkFileCustomParam::GetFileCustomParam(out_fileDesc);
	if (fileCustomParam)
	{
		delete fileCustomParam;
	}

	AkFileCustomParam::SetupAkFileDesc(out_fileDesc, nullptr);

	out_fileDesc.iFileSize = 0;
}
