#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BaseScoreWidget.generated.h"

class UTextBlock;
class UBorder;

UCLASS(Blueprintable)
class MYFPS_DEMO_API UBaseScoreWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ScoreText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> ScoreBorder;

public:
	UFUNCTION(BlueprintCallable)
	void UpdateScore(float NewScore) const;

private:
	/** Returns true when binding succeeded (PlayerState ready). */
	bool TryBindDelegate();

	UFUNCTION()
	void OnScoreUpdated(float NewScore);

	FTimerHandle BindRetryHandle;
};
