#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BasePlayerController.generated.h"

class UBaseBulletCounterWidget;
class UBaseCrosshairWidget;
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
	TObjectPtr<UBaseBulletCounterWidget> BulletCounterWidget;

	UPROPERTY()
	TObjectPtr<UBaseCrosshairWidget> CrosshairWidget;

public:
	UFUNCTION(BlueprintCallable, Category = "Crosshair")
	void SetCrosshairType(FName TypeName);
};
