#include "BaseHealthBarWidget.h"
#include "BaseCharacter.h"
#include "GameAbilitySystem/BaseHealthAttributeSet.h"
#include "GameFramework/PlayerController.h"
#include "Components/WidgetComponent.h"
#include "TimerManager.h"

void UBaseHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (HealthBar)
	{
		HealthBar->SetFillColorAndOpacity(FLinearColor(0.15f, 0.85f, 0.25f, 1.0f));
		HealthBar->SetPercent(1.0f);
	}

	if (HealthText)
	{
		FSlateFontInfo FontInfo = HealthText->GetFont();
		FontInfo.Size = 18;
		HealthText->SetFont(FontInfo);
		HealthText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		HealthText->SetShadowColorAndOpacity(FLinearColor::Black);
		HealthText->SetShadowOffset(FVector2D(1.0f, 1.0f));
		HealthText->SetText(FText::FromString(TEXT("100 / 100")));
	}

	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UBaseHealthBarWidget::TryBindDelegate);
}

void UBaseHealthBarWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

void UBaseHealthBarWidget::TryBindDelegate()
{
	if (UWidgetComponent* WidgetComp = Cast<UWidgetComponent>(GetOuter()))
	{
		if (ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(WidgetComp->GetOwner()))
		{
			OwnerChar->OnHealthUpdated.AddDynamic(this, &UBaseHealthBarWidget::OnHealthUpdated);

			if (UAbilitySystemComponent* ASC = OwnerChar->GetAbilitySystemComponent())
			{
				const float Health = ASC->GetNumericAttribute(UBaseHealthAttributeSet::GetHealthAttribute());
				const float MaxHealth = ASC->GetNumericAttribute(UBaseHealthAttributeSet::GetMaxHealthAttribute());
				UpdateHealth(Health, MaxHealth);
			}
		}
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(PC->GetPawn()))
	{
		BaseChar->OnHealthUpdated.AddDynamic(this, &UBaseHealthBarWidget::OnHealthUpdated);

		const float Health = BaseChar->GetAbilitySystemComponent() ?
			BaseChar->GetAbilitySystemComponent()->GetNumericAttribute(
				UBaseHealthAttributeSet::GetHealthAttribute()) : 100.0f;
		const float MaxHealth = BaseChar->GetAbilitySystemComponent() ?
			BaseChar->GetAbilitySystemComponent()->GetNumericAttribute(
				UBaseHealthAttributeSet::GetMaxHealthAttribute()) : 100.0f;
		UpdateHealth(Health, MaxHealth);
	}
}

void UBaseHealthBarWidget::OnHealthUpdated(float Health, float MaxHealth)
{
	UpdateHealth(Health, MaxHealth);
}

void UBaseHealthBarWidget::UpdateHealth(float Health, float MaxHealth) const
{
	if (HealthBar)
	{
		HealthBar->SetPercent(MaxHealth > 0.0f ? Health / MaxHealth : 0.0f);
	}

	if (HealthText)
	{
		const FString Text = FString::Printf(TEXT("%.0f / %.0f"), Health, MaxHealth);
		HealthText->SetText(FText::FromString(Text));
	}
}

FString UBaseHealthBarWidget::GetModuleName_Implementation() const
{
	return TEXT("BaseHealthBarWidget");
}
