// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseWeaponAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "BaseCharacter.h"
#include "AbilitySystemComponent.h"

UBaseWeaponAttributeSet::UBaseWeaponAttributeSet()
{
	CurrentAmmo = 0.0f;
	MaxAmmo = 30.0f;
}

void UBaseWeaponAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UBaseWeaponAttributeSet, CurrentAmmo, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseWeaponAttributeSet, MaxAmmo, COND_None, REPNOTIFY_Always);
}

void UBaseWeaponAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.Target.GetOwnerActor() && !Data.Target.GetOwnerActor()->HasAuthority())
	{
		return;
	}

	const FGameplayAttribute& ModifiedAttribute = Data.EvaluatedData.Attribute;
	if (ModifiedAttribute == GetCurrentAmmoAttribute())
	{
		SetCurrentAmmo(FMath::Clamp(GetCurrentAmmo(), 0.0f, GetMaxAmmo()));
	}
	if (ModifiedAttribute == GetMaxAmmoAttribute())
	{
		SetMaxAmmo(FMath::Max(GetMaxAmmo(), 1.0f));
	}
}

void UBaseWeaponAttributeSet::OnRep_CurrentAmmo(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseWeaponAttributeSet, CurrentAmmo, OldValue);

	UE_LOG(LogTemp, Warning, TEXT("[OnRep] CurrentAmmo 复制: %.0f -> %.0f"), OldValue.GetCurrentValue(), GetCurrentAmmo());

	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		if (ABaseCharacter* Character = Cast<ABaseCharacter>(ASC->GetOwnerActor()))
		{
			Character->UpdateWeaponHUD(static_cast<int32>(GetCurrentAmmo()), static_cast<int32>(GetMaxAmmo()));
		}
	}
}

void UBaseWeaponAttributeSet::OnRep_MaxAmmo(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseWeaponAttributeSet, MaxAmmo, OldValue);

	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		if (ABaseCharacter* Character = Cast<ABaseCharacter>(ASC->GetOwnerActor()))
		{
			Character->UpdateWeaponHUD(static_cast<int32>(GetCurrentAmmo()), static_cast<int32>(GetMaxAmmo()));
		}
	}
}
