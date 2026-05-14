#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UnLuaInterface.h"
#include "Components/ProgressBar.h"
#include "BaseStaminaBarWidget.generated.h"

class APlayerCharacter;

UCLASS(Blueprintable)
class MYFPS_DEMO_API UBaseStaminaBarWidget : public UUserWidget, public IUnLuaInterface
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> StaminaBar;

public:
	UFUNCTION(BlueprintCallable)
	void UpdateStamina(float Stamina, float MaxStamina) const;

	virtual FString GetModuleName_Implementation() const override;

private:
	void TryBindDelegate();

	UFUNCTION()
	void OnStaminaUpdated(float Stamina, float MaxStamina);
};
