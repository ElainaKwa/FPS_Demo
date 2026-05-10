#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UnLuaInterface.h"
#include "BaseBulletCounterWidget.generated.h"

class UTextBlock;
class UBorder;

UCLASS(Blueprintable)
class MYFPS_DEMO_API UBaseBulletCounterWidget : public UUserWidget, public IUnLuaInterface
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> AmmoText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> AmmoBorder;

public:
	UFUNCTION(BlueprintCallable)
	void UpdateAmmoDisplay(int32 CurrentAmmo, int32 MaxAmmo) const;

	virtual FString GetModuleName_Implementation() const override;

private:
	void TryBindDelegate();

	UFUNCTION()
	void OnAmmoUpdated(int32 CurrentAmmo, int32 MaxAmmo);
};