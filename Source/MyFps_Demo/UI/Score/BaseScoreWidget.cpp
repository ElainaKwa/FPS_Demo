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

	GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
	{
		TryBindDelegate();
	});
}

void UBaseScoreWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

void UBaseScoreWidget::TryBindDelegate()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	ABasePlayerState* PS = PC->GetPlayerState<ABasePlayerState>();
	if (!PS)
	{
		return;
	}

	PS->OnScoreUpdated.AddDynamic(this, &UBaseScoreWidget::OnScoreUpdated);
	OnScoreUpdated(PS->GetScore());
}

void UBaseScoreWidget::UpdateScore(float NewScore) const
{
	if (ScoreText)
	{
		ScoreText->SetText(FText::FromString(FString::Printf(TEXT("分数: %.0f"), NewScore)));
	}
}

void UBaseScoreWidget::OnScoreUpdated(float NewScore)
{
	UpdateScore(NewScore);
}
