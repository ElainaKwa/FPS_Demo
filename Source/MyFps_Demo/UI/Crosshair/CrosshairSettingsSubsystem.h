#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CrosshairSettingsTypes.h"
#include "CrosshairSettingsSubsystem.generated.h"

class UBaseCrosshairWidget;

UCLASS()
class MYFPS_DEMO_API UCrosshairSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "Crosshair|Settings")
	void SetFloatParam(FName Key, float Value);

	UFUNCTION(BlueprintCallable, Category = "Crosshair|Settings")
	void SetColorParam(FName Key, FLinearColor Value);

	UFUNCTION(BlueprintCallable, Category = "Crosshair|Settings")
	void SetCrosshairType(FName NewType);

	UFUNCTION(BlueprintPure, Category = "Crosshair|Settings")
	FName GetCurrentCrosshairType() const { return CurrentSettings.CrosshairType; }

	UFUNCTION(BlueprintPure, Category = "Crosshair|Settings")
	TSubclassOf<UBaseCrosshairWidget> GetCurrentCrosshairClass() const;

	UFUNCTION(BlueprintCallable, Category = "Crosshair|Settings")
	void RegisterCrosshairType(FName TypeName, TSubclassOf<UBaseCrosshairWidget> WidgetClass);

	UFUNCTION(BlueprintCallable, Category = "Crosshair|Settings")
	void ApplyToWidget(UBaseCrosshairWidget* Widget) const;

	UFUNCTION(BlueprintCallable, Category = "Crosshair|Settings")
	void GatherFromWidget(class UBaseCrosshairWidget* Widget);

	UFUNCTION(BlueprintCallable, Category = "Crosshair|Settings")
	void SaveSettings();

	const FCrosshairSettings& GetSettings() const { return CurrentSettings; }

protected:
	void LoadSettings();

	UPROPERTY()
	FString FallbackCrosshairClassPath;

private:
	void SetupDefaultRegistry();
	FString GetSaveSlotName() const;

	UPROPERTY()
	TMap<FName, TSubclassOf<UBaseCrosshairWidget>> CrosshairTypeRegistry;

	UPROPERTY()
	FCrosshairSettings CurrentSettings;

	UPROPERTY()
	TSubclassOf<UBaseCrosshairWidget> FallbackCrosshairClass;

	static const FName DefaultTypeName;
};
