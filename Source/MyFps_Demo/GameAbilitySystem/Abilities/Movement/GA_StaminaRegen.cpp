// Copyright Epic Games, Inc. All Rights Reserved.

#include "GA_StaminaRegen.h"
#include "BaseStaminaAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "ActiveGameplayEffectHandle.h"

UGA_StaminaRegen::UGA_StaminaRegen()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UGA_StaminaRegen::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!K2_HasAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ScheduleRegenCheck();
}

void UGA_StaminaRegen::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (K2_HasAuthority())
	{
		if (RegenEffectHandle.IsValid())
		{
			if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
			{
				ASC->RemoveActiveGameplayEffect(RegenEffectHandle);
				RegenEffectHandle.Invalidate();
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_StaminaRegen::ScheduleRegenCheck()
{
	UAbilityTask_WaitDelay* Task = UAbilityTask_WaitDelay::WaitDelay(this, 0.1f);
	Task->OnFinish.AddDynamic(this, &UGA_StaminaRegen::OnRegenCheck);
	Task->ReadyForActivation();
}

void UGA_StaminaRegen::OnRegenCheck()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		ScheduleRegenCheck();
		return;
	}

	const UBaseStaminaAttributeSet* StaminaAttr = Cast<UBaseStaminaAttributeSet>(
		ASC->GetAttributeSet(UBaseStaminaAttributeSet::StaticClass()));
	if (!StaminaAttr)
	{
		ScheduleRegenCheck();
		return;
	}

	const float CurrentStamina = StaminaAttr->GetStamina();
	const float MaxStamina = StaminaAttr->GetMaxStamina();

	AActor* Owner = GetAvatarActorFromActorInfo();
	if (Owner && StaminaAttr->LastStaminaConsumptionTime > 0.0f)
	{
		const float TimeSinceConsumption = Owner->GetWorld()->GetTimeSeconds() - StaminaAttr->LastStaminaConsumptionTime;
		if (TimeSinceConsumption < 0.15f)
		{
			if (RegenEffectHandle.IsValid())
			{
				ASC->RemoveActiveGameplayEffect(RegenEffectHandle);
				RegenEffectHandle.Invalidate();
			}

			if (GetCooldownGameplayEffect())
			{
				ApplyCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
			}

			ScheduleRegenCheck();
			return;
		}
	}

	if (CurrentStamina >= MaxStamina)
	{
		if (RegenEffectHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(RegenEffectHandle);
			RegenEffectHandle.Invalidate();
		}
		ScheduleRegenCheck();
		return;
	}

	if (!CheckCooldown(CurrentSpecHandle, CurrentActorInfo))
	{
		if (RegenEffectHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(RegenEffectHandle);
			RegenEffectHandle.Invalidate();
		}
		ScheduleRegenCheck();
		return;
	}

	if (!RegenEffectHandle.IsValid() && StaminaRegenEffectClass)
	{
		FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(StaminaRegenEffectClass, GetAbilityLevel());
		RegenEffectHandle = ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, Spec);
	}

	ScheduleRegenCheck();
}
