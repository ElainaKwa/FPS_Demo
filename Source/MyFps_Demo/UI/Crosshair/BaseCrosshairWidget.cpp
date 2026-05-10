#include "BaseCrosshairWidget.h"
#include "BaseCharacter.h"
#include "Weapons/BaseWeapon.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"
#include "CrosshairSettingsSubsystem.h"

void UBaseCrosshairWidget::NativeConstruct()
{
	Super::NativeConstruct();

	FTimerDelegate Delegate;
	Delegate.BindUObject(this, &UBaseCrosshairWidget::TryBindCharacter);
	GetWorld()->GetTimerManager().SetTimerForNextTick(Delegate);
}

void UBaseCrosshairWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

void UBaseCrosshairWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!CachedCharacter.IsValid())
	{
		OnVisibilityChanged(false);
		return;
	}

	OnVisibilityChanged(true);

	ABaseWeapon* PrevWeapon = CachedWeapon.Get();
	CachedWeapon = CachedCharacter->GetCurrentWeapon();

	if (CachedWeapon.Get() != PrevWeapon)
	{
		FireSpreadAccum = 0.0f;
		LastKnownShotTime = 0.0f;
	}

	if (bEnableSpread)
	{
		if (CachedWeapon.IsValid())
		{
			if (CachedWeapon->TimeOfLastShot > LastKnownShotTime)
			{
				FireSpreadAccum = FMath::Min(FireSpreadAccum + ShotSpreadIncrement, 1.0f);
				LastKnownShotTime = CachedWeapon->TimeOfLastShot;
			}
			else if (!CachedWeapon->bIsFiring)
			{
				FireSpreadAccum = FMath::Max(FireSpreadAccum - SpreadRecoveryRate * InDeltaTime, 0.0f);
			}
		}

		const float Target = ComputeTargetSpread();
		CurrentSpread = FMath::FInterpTo(CurrentSpread, Target, InDeltaTime, InterpSpeed);
	}
	else
	{
		CurrentSpread = 0.0f;
		FireSpreadAccum = 0.0f;
	}

	OnSpreadUpdated(CurrentSpread);
}

FString UBaseCrosshairWidget::GetModuleName_Implementation() const
{
	return TEXT("BaseCrosshairWidget");
}

void UBaseCrosshairWidget::TryBindCharacter()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	CachedCharacter = Cast<ABaseCharacter>(PC->GetPawn());
	if (CachedCharacter.IsValid())
	{
		CachedWeapon = CachedCharacter->GetCurrentWeapon();
	}
}

float UBaseCrosshairWidget::ComputeTargetSpread() const
{
	float MoveSpread = 0.0f;
	if (CachedCharacter.IsValid())
	{
		const float Speed = CachedCharacter->GetVelocity().Size();
		MoveSpread = FMath::Clamp(Speed / MaxSpeedFullSpread, 0.0f, 1.0f);
	}

	float WeaponSpread = 0.0f;
	if (CachedWeapon.IsValid())
	{
		const float Variance = CachedWeapon->GetEffectiveAimVariance();
		WeaponSpread = FMath::Clamp(Variance / MaxVarianceDeg, 0.0f, 1.0f);
	}

	return MoveSpread * MoveWeight + FireSpreadAccum * FireWeight + WeaponSpread * WeaponWeight;
}

FCrosshairSettings UBaseCrosshairWidget::GatherSettings_Implementation() const
{
	return FCrosshairSettings();
}

void UBaseCrosshairWidget::ApplySettings_Implementation(const FCrosshairSettings& Settings)
{
}

void UBaseCrosshairWidget::OnSpreadUpdated_Implementation(float Spread)
{
}

void UBaseCrosshairWidget::OnVisibilityChanged_Implementation(bool bVisible)
{
}
