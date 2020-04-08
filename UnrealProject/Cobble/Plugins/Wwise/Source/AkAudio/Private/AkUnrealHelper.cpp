// Copyright (c) 2006-2018 Audiokinetic Inc. / All Rights Reserved
#include "AkUnrealHelper.h"

#include "AkSettings.h"
#include "AkUEFeatures.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Platforms/AkUEPlatform.h"

#if WITH_EDITOR
#include "HAL/FileManager.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Docking/SDockTab.h"
#endif

namespace AkUnrealHelper
{
	const TCHAR* MediaFolderName = TEXT("Media");
	const TCHAR* ExternalSourceFolderName = TEXT("ExternalSources");

	void TrimPath(FString& Path)
	{
		Path.TrimStartAndEndInline();
	}

	FString GetProjectDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	}

	FString GetWwisePluginDirectory()
	{
		return FAkPlatform::GetWwisePluginDirectory();
	}

	FString GetContentDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	}

	FString GetThirdPartyDirectory()
	{
		return FPaths::Combine(GetWwisePluginDirectory(), TEXT("ThirdParty"));
	}

	FString GetSoundBankDirectory()
	{
		if (const UAkSettings* AkSettings = GetDefault<UAkSettings>())
		{
			return FPaths::Combine(GetContentDirectory(), AkSettings->WwiseSoundDataFolder.Path);
		}
		else
		{
			return FPaths::Combine(GetContentDirectory(), UAkSettings::DefaultSoundDataFolder);
		}
	}

	FString GetExternalSourceDirectory()
	{
		return FPaths::Combine(GetSoundBankDirectory(), ExternalSourceFolderName);
	}

	FString GetBaseAssetPackagePath()
	{
		if (const UAkSettings* AkSettings = GetDefault<UAkSettings>())
		{
			return FPaths::Combine(TEXT("/Game/"), AkSettings->WwiseSoundDataFolder.Path);
		}

		return TEXT("/Game/WwiseAudio");
	}

	FString GetLocalizedAssetPackagePath()
	{
		return FPaths::Combine(GetBaseAssetPackagePath(), TEXT("Localized"));
	}

	FString GetExternalSourceAssetPackagePath()
	{
		return FPaths::Combine(GetBaseAssetPackagePath(), ExternalSourceFolderName);
	}

	FString GetWwiseProjectPath()
	{
		FString projectPath;

		if (auto* settings = GetDefault<UAkSettings>())
		{
			projectPath = settings->WwiseProjectPath.FilePath;

			if (FPaths::IsRelative(projectPath))
			{
				projectPath = FPaths::ConvertRelativePathToFull(GetProjectDirectory(), projectPath);
			}

#if PLATFORM_WINDOWS
			projectPath.ReplaceInline(TEXT("/"), TEXT("\\"));
#endif
		}

		return projectPath;
	}

	FString GetGeneratedSoundBanksFolder()
	{
		if (IsUsingEventBased())
		{
			return FPaths::Combine(FPaths::GetPath(GetWwiseProjectPath()), TEXT("GeneratedSoundBanks"));
		}
		
		return GetSoundBankDirectory();
	}

	bool IsUsingEventBased()
	{
		static struct UsingAssetManagementCache
		{
			UsingAssetManagementCache()
			{
				if (auto AkSettings = GetDefault<UAkSettings>())
				{
					UseEventBasedPackaging = AkSettings->UseEventBasedPackaging;
				}
			}

			bool UseEventBasedPackaging = false;
		} AssetManagementCache;

		return AssetManagementCache.UseEventBasedPackaging;
	}

#if WITH_EDITOR
	void SanitizePath(FString& Path, const FString& PreviousPath, const FText& DialogMessage)
	{
		AkUnrealHelper::TrimPath(Path);

		FText FailReason;
		if (!FPaths::ValidatePath(Path, &FailReason))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FailReason);
			Path = PreviousPath;
		}

		const FString TempPath = FPaths::IsRelative(Path) ? FPaths::ConvertRelativePathToFull(AkUnrealHelper::GetProjectDirectory(), Path) : Path;
		if (!FPaths::DirectoryExists(TempPath))
		{
			FMessageDialog::Open(EAppMsgType::Ok, DialogMessage);
			Path = PreviousPath;
		}
	}

	void SanitizeProjectPath(FString& Path, const FString& PreviousPath, const FText& DialogMessage, bool &bRequestRefresh)
	{
		AkUnrealHelper::TrimPath(Path);

		FString TempPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*Path);

		FText FailReason;
		if (!FPaths::ValidatePath(TempPath, &FailReason))
		{
			if (EAppReturnType::Ok == FMessageDialog::Open(EAppMsgType::Ok, FailReason))
			{
				Path = PreviousPath;
				return;
			}
		}

		auto ProjectDirectory = AkUnrealHelper::GetProjectDirectory();
		if (!FPaths::FileExists(TempPath))
		{
			// Path might be a valid one (relative to game) entered manually. Check that.
			TempPath = FPaths::ConvertRelativePathToFull(ProjectDirectory, Path);

			if (!FPaths::FileExists(TempPath))
			{
				if (EAppReturnType::Ok == FMessageDialog::Open(EAppMsgType::Ok, DialogMessage))
				{
					Path = PreviousPath;
					return;
				}
			}
		}

		// Make the path relative to the game dir
		FPaths::MakePathRelativeTo(TempPath, *ProjectDirectory);
		Path = TempPath;

		if (Path != PreviousPath)
		{
			TSharedRef<SDockTab> WaapiPickerTab = FGlobalTabmanager::Get()->InvokeTab(FName("WaapiPicker"));
			TSharedRef<SDockTab> WwisePickerTab = FGlobalTabmanager::Get()->InvokeTab(FName("WwisePicker"));
			bRequestRefresh = true;
		}
	}
#endif
}
