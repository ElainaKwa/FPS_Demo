// Copyright Epic Games, Inc. All Rights Reserved.

#include "GA_Jump.h"
#include "BaseGameplayTags.h"
#include "BaseStaminaAttributeSet.h"
#include "PlayerCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"

UGA_Jump::UGA_Jump()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	SetAssetTags(FGameplayTagContainer(BaseGameplayTags::Ability_Jump));
}

bool UGA_Jump::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const AActor* const AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (AvatarActor == nullptr || AvatarActor->GetLocalRole() == ROLE_SimulatedProxy)
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC)
	{
		return false;
	}

	if (ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Dead) ||
		ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Airborne) ||
		ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Reloading))
	{
		return false;
	}

	return true;
}

void UGA_Jump::ActivateAbility(
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

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Dead) ||
		ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Airborne))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UCharacterMovementComponent* CMC = Character->GetCharacterMovement();
	if (CMC && CMC->IsFalling())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const UBaseStaminaAttributeSet* StaminaAttr = Cast<UBaseStaminaAttributeSet>(
		ASC->GetAttributeSet(UBaseStaminaAttributeSet::StaticClass()));
	if (StaminaAttr && StaminaAttr->GetStamina() <= 0.0f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (StaminaCostEffectClass)
	{
		FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(StaminaCostEffectClass, GetAbilityLevel());
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	}

	ASC->AddLooseGameplayTag(BaseGameplayTags::State_Airborne);
	if (APlayerCharacter* Player = Cast<APlayerCharacter>(Character))
	{
		Player->MulticastPerformJump();
	}
	else
	{
		Character->Jump();
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_Jump::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
