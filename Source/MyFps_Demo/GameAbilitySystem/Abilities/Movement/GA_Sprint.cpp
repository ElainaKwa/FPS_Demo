// Copyright Epic Games, Inc. All Rights Reserved.

#include "GA_Sprint.h"
#include "BaseGameplayTags.h"
#include "BaseMovementAttributeSet.h"
#include "BaseStaminaAttributeSet.h"
#include "PlayerCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "ActiveGameplayEffectHandle.h"

UGA_Sprint::UGA_Sprint()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	SetAssetTags(FGameplayTagContainer(BaseGameplayTags::Ability_Sprint));
}

bool UGA_Sprint::CanActivateAbility(
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
		ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Crouching) ||
		ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Reloading) ||
		ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Sprinting) ||
		ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Airborne))
	{
		return false;
	}

	return true;
}

void UGA_Sprint::ActivateAbility(
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
		ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Crouching) ||
		ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Reloading) ||
		ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Sprinting) ||
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

	ASC->AddLooseGameplayTag(BaseGameplayTags::State_Sprinting);

	if (APlayerCharacter* Player = Cast<APlayerCharacter>(Character))
	{
		Player->MulticastStartSprint();
		Player->ApplyMovementSpeed();
	}

	if (StaminaDrainEffectClass)
	{
		FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(StaminaDrainEffectClass, GetAbilityLevel());
		StaminaDrainHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);

		if (!StaminaDrainHandle.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("[GA_Sprint] StaminaDrain GE 应用失败，请检查 BP 中 StaminaDrainEffectClass 配置"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_Sprint] StaminaDrainEffectClass 未配置，疾跑不会消耗体力"));
	}
}

void UGA_Sprint::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (K2_HasAuthority())
	{
		if (StaminaDrainHandle.IsValid())
		{
			if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
			{
				ASC->RemoveActiveGameplayEffect(StaminaDrainHandle);
				StaminaDrainHandle.Invalidate();
			}
		}

		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(BaseGameplayTags::State_Sprinting);
		}

		if (APlayerCharacter* Player = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo()))
		{
			Player->MulticastStopSprint();
			Player->ApplyMovementSpeed();
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
