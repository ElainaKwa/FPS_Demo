// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseStaminaAttributeSet.h"
#include "BaseAbilitySystemComponent.h"
#include "BaseGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemComponent.h"
#include "PlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

UBaseStaminaAttributeSet::UBaseStaminaAttributeSet()
{
	Stamina = 100.0f;
	MaxStamina = 100.0f;
	IncomingStaminaCost = 0.0f;
}

void UBaseStaminaAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UBaseStaminaAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseStaminaAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
}

void UBaseStaminaAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.Target.GetOwnerActor() && !Data.Target.GetOwnerActor()->HasAuthority())
	{
		return;
	}

	const FGameplayAttribute& ModifiedAttribute = Data.EvaluatedData.Attribute;

	if (ModifiedAttribute == GetIncomingStaminaCostAttribute())
	{
		const float Cost = GetIncomingStaminaCost();
		SetIncomingStaminaCost(0.0f);

		if (Cost > 0.0f)
		{
			if (APlayerCharacter* Player = Cast<APlayerCharacter>(Data.Target.GetOwnerActor()))
			{
				UCharacterMovementComponent* CMC = Player->GetCharacterMovement();
				if (CMC && CMC->Velocity.SizeSquared2D() < 1.0f)
				{
					return;
				}
			}

			const float NewStamina = GetStamina() - Cost;
			SetStamina(FMath::Clamp(NewStamina, 0.0f, GetMaxStamina()));

			if (GetStamina() <= 0.0f)
			{
				if (UBaseAbilitySystemComponent* BASC = Cast<UBaseAbilitySystemComponent>(GetOwningAbilitySystemComponent()))
				{
					const FGameplayAbilitySpecHandle SprintHandle = BASC->GetSprintAbilityHandle();
					if (SprintHandle.IsValid())
					{
						BASC->CancelAbilityHandle(SprintHandle);
					}
				}
			}

			if (Data.Target.GetOwnerActor())
			{
				LastStaminaConsumptionTime = Data.Target.GetOwnerActor()->GetWorld()->GetTimeSeconds();
			}

			if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
			{
				FGameplayEventData EventData;
				ASC->HandleGameplayEvent(BaseGameplayTags::Event_StaminaConsumed, &EventData);
			}
		}
	}

	if (ModifiedAttribute == GetStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(), 0.0f, GetMaxStamina()));

		if (GetStamina() <= 0.0f)
		{
			if (UBaseAbilitySystemComponent* BASC = Cast<UBaseAbilitySystemComponent>(GetOwningAbilitySystemComponent()))
			{
				const FGameplayAbilitySpecHandle SprintHandle = BASC->GetSprintAbilityHandle();
				if (SprintHandle.IsValid())
				{
					BASC->CancelAbilityHandle(SprintHandle);
				}
			}
		}
	}

	if (ModifiedAttribute == GetMaxStaminaAttribute())
	{
		SetMaxStamina(FMath::Max(GetMaxStamina(), 1.0f));
	}

	if (APlayerCharacter* Player = Cast<APlayerCharacter>(Data.Target.GetOwnerActor()))
	{
		Player->UpdateStaminaHUD();
	}
}

void UBaseStaminaAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseStaminaAttributeSet, Stamina, OldValue);

	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		if (APlayerCharacter* Player = Cast<APlayerCharacter>(ASC->GetOwnerActor()))
		{
			Player->UpdateStaminaHUD();
		}
	}
}

void UBaseStaminaAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseStaminaAttributeSet, MaxStamina, OldValue);

	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		if (APlayerCharacter* Player = Cast<APlayerCharacter>(ASC->GetOwnerActor()))
		{
			Player->UpdateStaminaHUD();
		}
	}
}
