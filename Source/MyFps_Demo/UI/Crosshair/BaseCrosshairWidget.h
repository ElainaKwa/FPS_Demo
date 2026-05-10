#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UnLuaInterface.h"
#include "CrosshairSettingsTypes.h"
#include "BaseCrosshairWidget.generated.h"

class ABaseCharacter;
class ABaseWeapon;

UCLASS(Abstract, Blueprintable)
class MYFPS_DEMO_API UBaseCrosshairWidget : public UUserWidget, public IUnLuaInterface
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

public:
	virtual FString GetModuleName_Implementation() const override;

	// ---- 持久化接口（子类必须覆写） ----

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Crosshair|Settings")
	FCrosshairSettings GatherSettings() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Crosshair|Settings")
	void ApplySettings(const FCrosshairSettings& Settings);

	// ---- 渲染回调（子类通过覆写实现各自视觉） ----

	UFUNCTION(BlueprintNativeEvent, Category = "Crosshair|Render")
	void OnSpreadUpdated(float Spread);

	UFUNCTION(BlueprintNativeEvent, Category = "Crosshair|Render")
	void OnVisibilityChanged(bool bVisible);

	// ---- 散度开关 ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair")
	bool bEnableSpread = true;

protected:
	void TryBindCharacter();

	UPROPERTY()
	TWeakObjectPtr<ABaseCharacter> CachedCharacter;

	UPROPERTY()
	TWeakObjectPtr<ABaseWeapon> CachedWeapon;

	// ---- 散度参数 ----

	UPROPERTY(EditAnywhere, Category = "Crosshair|Spread", meta = (ClampMin = "100.0", ClampMax = "2000.0"))
	float MaxSpeedFullSpread = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Crosshair|Spread", meta = (ClampMin = "0.01", ClampMax = "0.5"))
	float ShotSpreadIncrement = 0.08f;

	UPROPERTY(EditAnywhere, Category = "Crosshair|Spread", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float SpreadRecoveryRate = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Crosshair|Spread", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float InterpSpeed = 12.0f;

	UPROPERTY(EditAnywhere, Category = "Crosshair|Spread", meta = (ClampMin = "1.0", ClampMax = "90.0"))
	float MaxVarianceDeg = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Crosshair|Spread", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MoveWeight = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Crosshair|Spread", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FireWeight = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Crosshair|Spread", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WeaponWeight = 0.25f;

	float CurrentSpread = 0.0f;
	float FireSpreadAccum = 0.0f;
	float LastKnownShotTime = 0.0f;

	float ComputeTargetSpread() const;
};
