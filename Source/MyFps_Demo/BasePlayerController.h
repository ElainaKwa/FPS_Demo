#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BasePlayerController.generated.h"

class UBaseBulletCounterWidget;
class UBaseCrosshairWidget;
class UBaseHealthBarWidget;
class UBaseStaminaBarWidget;
class UCrosshairSettingsSubsystem;

UCLASS()
class MYFPS_DEMO_API ABasePlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	UPROPERTY()
	FString BulletCounterWidgetClassPath;

	UPROPERTY()
	FString HealthBarWidgetClassPath;

	UPROPERTY()
	FString StaminaBarWidgetClassPath;

	UPROPERTY()
	TObjectPtr<UBaseBulletCounterWidget> BulletCounterWidget;

	UPROPERTY()
	TObjectPtr<UBaseCrosshairWidget> CrosshairWidget;

	UPROPERTY()
	TObjectPtr<UBaseHealthBarWidget> HealthBarWidget;

	UPROPERTY()
	TObjectPtr<UBaseStaminaBarWidget> StaminaBarWidget;

public:
	UFUNCTION(BlueprintCallable, Category = "Crosshair")
	void SetCrosshairType(FName TypeName);
};
