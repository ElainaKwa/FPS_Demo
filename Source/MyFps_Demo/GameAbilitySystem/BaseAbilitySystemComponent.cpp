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
