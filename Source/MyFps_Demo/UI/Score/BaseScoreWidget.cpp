#include "BaseScoreWidget.h"
#include "BasePlayerState.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UBaseScoreWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (TryBindDelegate())
	{
		return;
	}

	// If PlayerState isn't ready yet, poll every 0.1s
	GetWorld()->GetTimerManager().SetTimer(BindRetryHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (TryBindDelegate())
		{
			GetWorld()->GetTimerManager().ClearTimer(BindRetryHandle);
		}
	}), 0.1f, true);
}

void UBaseScoreWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

bool UBaseScoreWidget::TryBindDelegate()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return false;
	}

	ABasePlayerState* PS = PC->GetPlayerState<ABasePlayerState>();
	if (!PS)
	{
		return false;
	}

	PS->OnScoreUpdated.AddDynamic(this, &UBaseScoreWidget::OnScoreUpdated);
	// Show current value immediately
	UpdateScore(PS->GetScore());
	return true;
}

void UBaseScoreWidget::UpdateScore(float NewScore) const
{
	if (ScoreText)
	{
		ScoreText->SetText(FText::FromString(FString::Printf(TEXT("Score: %.0f"), NewScore)));
	}
}

void UBaseScoreWidget::OnScoreUpdated(float NewScore)
{
	UpdateScore(NewScore);
}
