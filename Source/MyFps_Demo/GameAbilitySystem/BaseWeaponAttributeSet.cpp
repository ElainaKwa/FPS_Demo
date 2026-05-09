// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseWeaponAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

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

void UBaseWeaponAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetCurrentAmmoAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxAmmo());
	}
	if (Attribute == GetMaxAmmoAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
}

void UBaseWeaponAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
}

void UBaseWeaponAttributeSet::OnRep_CurrentAmmo(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseWeaponAttributeSet, CurrentAmmo, OldValue);
}

void UBaseWeaponAttributeSet::OnRep_MaxAmmo(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseWeaponAttributeSet, MaxAmmo, OldValue);
}
