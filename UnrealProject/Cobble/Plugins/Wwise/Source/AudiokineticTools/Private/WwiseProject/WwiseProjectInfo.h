#pragma once

#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "FastXml.h"

struct FWwiseLanguageInfo
{
	FGuid ID;
	FString Name;
};

class WwiseProjectInfo : public IFastXmlCallback
{
public:
	virtual ~WwiseProjectInfo() {}

	void Parse();

	FString const& CacheDirectory() const { return cacheDirectory; }

	TSet<FString> const& SupportedPlatforms() const { return supportedPlatforms; }
	TArray<FWwiseLanguageInfo> const& SupportedLanguages() const { return supportedLanguages; }
	FString DefaultLanguage() const { return defaultLanguage; }

	bool ProcessAttribute(const TCHAR* AttributeName, const TCHAR* AttributeValue) override;
	bool ProcessClose(const TCHAR* Element) override;
	bool ProcessComment(const TCHAR* Comment) override;
	bool ProcessElement(const TCHAR* ElementName, const TCHAR* ElementData, int32 XmlFileLineNumber) override;
	bool ProcessXmlDeclaration(const TCHAR* ElementData, int32 XmlFileLineNumber) override;

private:
	FString GetProjectPath() const;

private:
	TSet<FString> supportedPlatforms;
	TArray<FWwiseLanguageInfo> supportedLanguages;
	FString defaultLanguage;
	FString cacheDirectory;

	bool insidePlatformElement = false;
	bool insideLanguageElement = false;
	bool insideMiscSettingEntryElement = false;
	bool insideCacheSettings = false;
	bool insidePropertyElement = false;
	bool insideDefaultLanguage = false;

	FWwiseLanguageInfo currentLanguageInfo;
};