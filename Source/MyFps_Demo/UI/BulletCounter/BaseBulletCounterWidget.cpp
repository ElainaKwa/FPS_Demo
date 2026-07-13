#include "BaseBulletCounterWidget.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "BaseCharacter.h"
#include "GameAbilitySystem/BaseWeaponAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Weapons/BaseWeapon.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

void UBaseBulletCounterWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (AmmoBorder)
	{
		AmmoBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
		AmmoBorder->SetPadding(FMargin(8.0f, 4.0f));
	}

	if (AmmoText)
	{
		FSlateFontInfo FontInfo = AmmoText->GetFont();
		FontInfo.Size = 28;
		AmmoText->SetFont(FontInfo);
		AmmoText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		AmmoText->SetShadowColorAndOpacity(FLinearColor::Black);
		AmmoText->SetShadowOffset(FVector2D(1.0f, 1.0f));
		AmmoText->SetText(FText::FromString(TEXT("0 / 0")));
	}

	UE_LOG(LogTemp, Warning, TEXT("BaseBulletCounterWidget: created and shown"));

	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UBaseBulletCounterWidget::TryBindDelegate);
}

void UBaseBulletCounterWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

void UBaseBulletCounterWidget::TryBindDelegate()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(PC->GetPawn()))
	{
		BaseChar->OnBulletCountUpdated.AddDynamic(this, &UBaseBulletCounterWidget::OnAmmoUpdated);

		if (UAbilitySystemComponent* ASC = BaseChar->GetAbilitySystemComponent())
		{
			const float CurrentAmmo = ASC->GetNumericAttribute(UBaseWeaponAttributeSet::GetCurrentAmmoAttribute());
			const float MaxAmmo = ASC->GetNumericAttribute(UBaseWeaponAttributeSet::GetMaxAmmoAttribute());
			UpdateAmmoDisplay(static_cast<int32>(CurrentAmmo), static_cast<int32>(MaxAmmo));
		}
	}
}

void UBaseBulletCounterWidget::OnAmmoUpdated(int32 CurrentAmmo, int32 MaxAmmo)
{
	UpdateAmmoDisplay(CurrentAmmo, MaxAmmo);
}

void UBaseBulletCounterWidget::UpdateAmmoDisplay(int32 CurrentAmmo, int32 MaxAmmo) const
{
	if (AmmoText)
	{
		const FString AmmoString = FString::Printf(TEXT("%d / %d"), CurrentAmmo, MaxAmmo);
		AmmoText->SetText(FText::FromString(AmmoString));
	}
}

FString UBaseBulletCounterWidget::GetModuleName_Implementation() const
{
	return TEXT("BaseBulletCounterWidget");
}