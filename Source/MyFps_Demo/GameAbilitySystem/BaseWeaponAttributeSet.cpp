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
}

void UBaseWeaponAttributeSet::OnRep_MaxAmmo(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseWeaponAttributeSet, MaxAmmo, OldValue);
}
