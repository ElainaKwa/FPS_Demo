// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseAbilitySystemComponent.h"
#include "BaseGameplayTags.h"

void UBaseAbilitySystemComponent::GrantDefaultAbilities()
{
	if (FireAbilityClass)
	{
		FireAbilityHandle = GiveAbility(FGameplayAbilitySpec(FireAbilityClass, 1, static_cast<int32>(EAbilityInputID::Fire), this));
	}

	if (ReloadAbilityClass)
	{
		ReloadAbilityHandle = GiveAbility(FGameplayAbilitySpec(ReloadAbilityClass, 1, static_cast<int32>(EAbilityInputID::Reload), this));
	}
}

void UBaseAbilitySystemComponent::GrantMovementAbilities()
{
	if (CrouchAbilityClass)
	{
		CrouchAbilityHandle = GiveAbility(FGameplayAbilitySpec(CrouchAbilityClass, 1, static_cast<int32>(EAbilityInputID::Crouch), this));
	}

	if (SprintAbilityClass)
	{
		SprintAbilityHandle = GiveAbility(FGameplayAbilitySpec(SprintAbilityClass, 1, static_cast<int32>(EAbilityInputID::Sprint), this));
	}

	if (JumpAbilityClass)
	{
		JumpAbilityHandle = GiveAbility(FGameplayAbilitySpec(JumpAbilityClass, 1, static_cast<int32>(EAbilityInputID::Jump), this));
	}
}

void UBaseAbilitySystemComponent::CancelFireAbility()
{
	if (FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(FireAbilityHandle))
	{
		if (Spec->IsActive())
		{
			CancelAbilitySpec(*Spec, nullptr);
		}
	}
}

void UBaseAbilitySystemComponent::CancelSprintAbility()
{
	if (FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(SprintAbilityHandle))
	{
		if (Spec->IsActive())
		{
			CancelAbilitySpec(*Spec, nullptr);
		}
	}
}

void UBaseAbilitySystemComponent::GrantPassiveAbilities()
{
	if (StaminaRegenAbilityClass)
	{
		StaminaRegenAbilityHandle = GiveAbility(FGameplayAbilitySpec(StaminaRegenAbilityClass, 1, -1, this));
		if (StaminaRegenAbilityHandle.IsValid())
		{
			TryActivateAbility(StaminaRegenAbilityHandle);
		}
	}
}
