#include "BaseStaminaBarWidget.h"
#include "PlayerCharacter.h"
#include "GameAbilitySystem/BaseStaminaAttributeSet.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

void UBaseStaminaBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (StaminaBar)
	{
		StaminaBar->SetFillColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.0f, 1.0f));
		StaminaBar->SetPercent(1.0f);
	}

	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UBaseStaminaBarWidget::TryBindDelegate);
}

void UBaseStaminaBarWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

void UBaseStaminaBarWidget::TryBindDelegate()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	if (APlayerCharacter* Player = Cast<APlayerCharacter>(PC->GetPawn()))
	{
		Player->OnStaminaUpdated.AddDynamic(this, &UBaseStaminaBarWidget::OnStaminaUpdated);

		const float Stamina = Player->GetAbilitySystemComponent() ?
			Player->GetAbilitySystemComponent()->GetNumericAttribute(
				UBaseStaminaAttributeSet::GetStaminaAttribute()) : 100.0f;
		const float MaxStamina = Player->GetAbilitySystemComponent() ?
			Player->GetAbilitySystemComponent()->GetNumericAttribute(
				UBaseStaminaAttributeSet::GetMaxStaminaAttribute()) : 100.0f;
		UpdateStamina(Stamina, MaxStamina);
	}
}

void UBaseStaminaBarWidget::OnStaminaUpdated(float Stamina, float MaxStamina)
{
	UpdateStamina(Stamina, MaxStamina);
}

void UBaseStaminaBarWidget::UpdateStamina(float Stamina, float MaxStamina) const
{
	if (StaminaBar)
	{
		StaminaBar->SetPercent(MaxStamina > 0.0f ? Stamina / MaxStamina : 0.0f);
	}
}

FString UBaseStaminaBarWidget::GetModuleName_Implementation() const
{
	return TEXT("BaseStaminaBarWidget");
}
