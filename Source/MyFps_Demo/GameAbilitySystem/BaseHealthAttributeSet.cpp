// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseHealthAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemComponent.h"
#include "BaseCharacter.h"
#include "Engine/Engine.h"

UBaseHealthAttributeSet::UBaseHealthAttributeSet()
{
	Health = 100.0f;
	MaxHealth = 100.0f;
	IncomingDamage = 0.0f;
	IncomingHeal = 0.0f;
}

void UBaseHealthAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UBaseHealthAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseHealthAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UBaseHealthAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
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

			if (AActor* Owner = Data.Target.GetOwnerActor())
			{
				const FString DamageMsg = FString::Printf(TEXT("[受击] %s 受到伤害 %.0f  |  血量: %.0f / %.0f"),
					*Owner->GetName(), Damage, GetHealth(), GetMaxHealth());
				UE_LOG(LogTemp, Warning, TEXT("%s"), *DamageMsg);
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, DamageMsg);
				}
			}
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

	if (ModifiedAttribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}

	if (GetHealth() <= 0.0f)
	{
		if (ABaseCharacter* Character = Cast<ABaseCharacter>(Data.Target.GetOwnerActor()))
		{
			Character->OnDeath();
		}
	}

	ABaseCharacter* Character = Cast<ABaseCharacter>(Data.Target.GetOwnerActor());
	if (Character)
	{
		Character->UpdateHealthHUD();
	}
}

void UBaseHealthAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseHealthAttributeSet, Health, OldValue);

	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		if (ABaseCharacter* Character = Cast<ABaseCharacter>(ASC->GetOwnerActor()))
		{
			Character->UpdateHealthHUD();
		}
	}
}

void UBaseHealthAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseHealthAttributeSet, MaxHealth, OldValue);

	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		if (ABaseCharacter* Character = Cast<ABaseCharacter>(ASC->GetOwnerActor()))
		{
			Character->UpdateHealthHUD();
		}
	}
}
