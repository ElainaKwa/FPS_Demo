#include "CrosshairSettingsSubsystem.h"
#include "BaseCrosshairWidget.h"
#include "Crosshair_CrossDot.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Misc/ConfigCacheIni.h"

const FName UCrosshairSettingsSubsystem::DefaultTypeName = TEXT("CrossDot");

void UCrosshairSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Read widget class path directly from DefaultGame.ini (bypasses GConfig layering)
	if (FallbackCrosshairClassPath.IsEmpty())
	{
		FConfigFile ConfigFile;
		ConfigFile.Read(*(FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini")));
		ConfigFile.GetString(TEXT("/Script/MyFps_Demo.CrosshairSettingsSubsystem"),
			TEXT("FallbackCrosshairClassPath"), FallbackCrosshairClassPath);
	}

	if (!FallbackCrosshairClassPath.IsEmpty())
	{
		FallbackCrosshairClass = LoadClass<UBaseCrosshairWidget>(nullptr, *FallbackCrosshairClassPath);
	}

	if (!FallbackCrosshairClass)
	{
		FallbackCrosshairClass = UCrosshair_CrossDot::StaticClass();
		UE_LOG(LogTemp, Warning, TEXT("CrosshairSettings: Config path failed or empty, using C++ fallback [UCrosshair_CrossDot]"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("CrosshairSettings: using Config class [%s]"), *FallbackCrosshairClass->GetName());
	}

	SetupDefaultRegistry();

	if (CurrentSettings.CrosshairType.IsNone())
	{
		CurrentSettings.CrosshairType = DefaultTypeName;
	}

	LoadSettings();
}

void UCrosshairSettingsSubsystem::SetupDefaultRegistry()
{
	if (FallbackCrosshairClass)
	{
		CrosshairTypeRegistry.Add(DefaultTypeName, FallbackCrosshairClass);
	}
}

void UCrosshairSettingsSubsystem::RegisterCrosshairType(FName TypeName, TSubclassOf<UBaseCrosshairWidget> WidgetClass)
{
	if (!WidgetClass)
	{
		return;
	}

	CrosshairTypeRegistry.Add(TypeName, WidgetClass);
}

void UCrosshairSettingsSubsystem::SetFloatParam(FName Key, float Value)
{
	CurrentSettings.FloatParams.Add(Key, Value);
}

void UCrosshairSettingsSubsystem::SetColorParam(FName Key, FLinearColor Value)
{
	CurrentSettings.ColorParams.Add(Key, Value);
}

void UCrosshairSettingsSubsystem::SetCrosshairType(FName NewType)
{
	CurrentSettings.CrosshairType = NewType;
}

TSubclassOf<UBaseCrosshairWidget> UCrosshairSettingsSubsystem::GetCurrentCrosshairClass() const
{
	if (const TSubclassOf<UBaseCrosshairWidget>* Found = CrosshairTypeRegistry.Find(CurrentSettings.CrosshairType))
	{
		if (*Found)
		{
			return *Found;
		}
	}

	return FallbackCrosshairClass;
}

void UCrosshairSettingsSubsystem::ApplyToWidget(UBaseCrosshairWidget* Widget) const
{
	if (!Widget)
	{
		return;
	}

	Widget->ApplySettings(CurrentSettings);
}

void UCrosshairSettingsSubsystem::GatherFromWidget(UBaseCrosshairWidget* Widget)
{
	if (!Widget)
	{
		return;
	}

	CurrentSettings = Widget->GatherSettings();
}

void UCrosshairSettingsSubsystem::SaveSettings()
{
	UCrosshairSaveGame* SaveObj = Cast<UCrosshairSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UCrosshairSaveGame::StaticClass()));
	if (!SaveObj)
	{
		return;
	}

	SaveObj->Settings = CurrentSettings;
	UGameplayStatics::SaveGameToSlot(SaveObj, GetSaveSlotName(), 0);

	UE_LOG(LogTemp, Log, TEXT("CrosshairSettings: saved [%s]"), *CurrentSettings.CrosshairType.ToString());
}

void UCrosshairSettingsSubsystem::LoadSettings()
{
	UCrosshairSaveGame* SaveObj = Cast<UCrosshairSaveGame>(
		UGameplayStatics::LoadGameFromSlot(GetSaveSlotName(), 0));

	if (SaveObj)
	{
		CurrentSettings = SaveObj->Settings;
		UE_LOG(LogTemp, Log, TEXT("CrosshairSettings: loaded [%s]"), *CurrentSettings.CrosshairType.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("CrosshairSettings: no save found, using defaults"));
	}
}

FString UCrosshairSettingsSubsystem::GetSaveSlotName() const
{
	return TEXT("CrosshairSettings");
}
