#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UnLuaInterface.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "BaseHealthBarWidget.generated.h"

UCLASS(Blueprintable)
class MYFPS_DEMO_API UBaseHealthBarWidget : public UUserWidget, public IUnLuaInterface
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthText;

public:
	UFUNCTION(BlueprintCallable)
	void UpdateHealth(float Health, float MaxHealth) const;

	virtual FString GetModuleName_Implementation() const override;

private:
	void TryBindDelegate();

	UFUNCTION()
	void OnHealthUpdated(float Health, float MaxHealth);
};
