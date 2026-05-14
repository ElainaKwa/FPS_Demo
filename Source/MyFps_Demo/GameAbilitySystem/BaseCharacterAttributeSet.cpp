// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseCharacterAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemComponent.h"
#include "BaseCharacter.h"
#include "PlayerCharacter.h"

UBaseCharacterAttributeSet::UBaseCharacterAttributeSet()
{
	Health = 100.0f;
	MaxHealth = 100.0f;
	IncomingDamage = 0.0f;
	IncomingHeal = 0.0f;
	Stamina = 100.0f;
	MaxStamina = 100.0f;
}

void UBaseCharacterAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharacterAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharacterAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharacterAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseCharacterAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
}

void UBaseCharacterAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.Target.GetOwnerActor() && !Data.Target.GetOwnerActor()->HasAuthority())
	{
		return;
	}

	const FGameplayAttribute& ModifiedAttribute = Data.EvaluatedData.Attribute;

	if (ModifiedAttribute == GetIncomingDamageAttribute())
	{
		const float Damage = GetIncomingDamage();
		SetIncomingDamage(0.0f);

		if (Damage > 0.0f)
		{
			const float NewHealth = GetHealth() - Damage;
			SetHealth(FMath::Clamp(NewHealth, 0.0f, GetMaxHealth()));
		}
	}

	if (ModifiedAttribute == GetIncomingHealAttribute())
	{
		const float Heal = GetIncomingHeal();
		SetIncomingHeal(0.0f);

		if (Heal > 0.0f)
		{
			const float NewHealth = GetHealth() + Heal;
			SetHealth(FMath::Clamp(NewHealth, 0.0f, GetMaxHealth()));
		}
	}

	if (ModifiedAttribute == GetMaxHealthAttribute())
	{
		SetMaxHealth(FMath::Max(GetMaxHealth(), 1.0f));
	}
	if (ModifiedAttribute == GetStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(), 0.0f, GetMaxStamina()));
	}
	if (ModifiedAttribute == GetMaxStaminaAttribute())
	{
		SetMaxStamina(FMath::Max(GetMaxStamina(), 1.0f));
	}

	ABaseCharacter* Character = Cast<ABaseCharacter>(Data.Target.GetOwnerActor());
	if (!Character)
	{
		return;
	}

	if (ModifiedAttribute == GetIncomingDamageAttribute() || ModifiedAttribute == GetIncomingHealAttribute()
		|| ModifiedAttribute == GetMaxHealthAttribute())
	{
		Character->UpdateHealthHUD();
	}
	if (ModifiedAttribute == GetStaminaAttribute() || ModifiedAttribute == GetMaxStaminaAttribute())
	{
		if (APlayerCharacter* Player = Cast<APlayerCharacter>(Character))
		{
			Player->UpdateStaminaHUD();
		}
	}
}

void UBaseCharacterAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharacterAttributeSet, Health, OldValue);

	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		if (ABaseCharacter* Character = Cast<ABaseCharacter>(ASC->GetOwnerActor()))
		{
			Character->UpdateHealthHUD();
		}
	}
}

void UBaseCharacterAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharacterAttributeSet, MaxHealth, OldValue);

	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		if (ABaseCharacter* Character = Cast<ABaseCharacter>(ASC->GetOwnerActor()))
		{
			Character->UpdateHealthHUD();
		}
	}
}

void UBaseCharacterAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharacterAttributeSet, Stamina, OldValue);

	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		if (APlayerCharacter* Player = Cast<APlayerCharacter>(ASC->GetOwnerActor()))
		{
			Player->UpdateStaminaHUD();
		}
	}
}

void UBaseCharacterAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseCharacterAttributeSet, MaxStamina, OldValue);

	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		if (APlayerCharacter* Player = Cast<APlayerCharacter>(ASC->GetOwnerActor()))
		{
			Player->UpdateStaminaHUD();
		}
	}
}
