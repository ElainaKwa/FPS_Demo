// Copyright Epic Games, Inc. All Rights Reserved.

#include "GA_Crouch.h"
#include "BaseGameplayTags.h"
#include "BaseMovementAttributeSet.h"
#include "BaseStaminaAttributeSet.h"
#include "PlayerCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"

UGA_Crouch::UGA_Crouch()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	SetAssetTags(FGameplayTagContainer(BaseGameplayTags::Ability_Crouch));
}

bool UGA_Crouch::CanActivateAbility(
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
		ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Sprinting) ||
		ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Crouching) ||
		ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Airborne))
	{
		return false;
	}

	return true;
}

void UGA_Crouch::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!K2_HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_Crouch] ActivateAbility 非 Authority，终止"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_Crouch] ActivateAbility ASC 为空"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Dead))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_Crouch] 被 State_Dead 阻塞"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Sprinting))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_Crouch] 被 State_Sprinting 阻塞"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Crouching))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_Crouch] 被 State_Crouching 阻塞（已在下蹲中）"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Airborne))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_Crouch] 被 State_Airborne 阻塞"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_Crouch] Character 为 null"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!Character->CanCrouch())
	{
		const bool bNotCrouched = !Character->bIsCrouched;
		const bool bNotFalling = !Character->GetCharacterMovement()->IsFalling();
		const bool bCanEverCrouch = Character->GetCharacterMovement()->NavAgentProps.bCanCrouch;
		const bool bMovingOnGround = Character->GetCharacterMovement()->IsMovingOnGround();
		UE_LOG(LogTemp, Warning, TEXT("[GA_Crouch] CanCrouch()=false | bNotCrouched=%d bNotFalling=%d bCanEverCrouch=%d bMovingOnGround=%d"),
			bNotCrouched, bNotFalling, bCanEverCrouch, bMovingOnGround);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const UBaseStaminaAttributeSet* StaminaAttr = Cast<UBaseStaminaAttributeSet>(
		ASC->GetAttributeSet(UBaseStaminaAttributeSet::StaticClass()));
	if (StaminaAttr && StaminaAttr->GetStamina() <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_Crouch] 体力不足 (Stamina=%.0f)"), StaminaAttr->GetStamina());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_Crouch] CommitAbility 失败"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[GA_Crouch] CommitAbility 成功，执行下蹲"));

	if (StaminaCostEffectClass)
	{
		FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(StaminaCostEffectClass, GetAbilityLevel());
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_Crouch] StaminaCostEffectClass 未配置，下蹲不消耗体力"));
	}

	if (APlayerCharacter* Player = Cast<APlayerCharacter>(Character))
	{
		Player->MulticastPerformCrouch();
	}
	else
	{
		Character->Crouch();
	}
	ASC->AddLooseGameplayTag(BaseGameplayTags::State_Crouching);

	if (APlayerCharacter* Player = Cast<APlayerCharacter>(Character))
	{
		Player->ApplyMovementSpeed();
	}

	UE_LOG(LogTemp, Warning, TEXT("[GA_Crouch] 下蹲执行完毕"));
}

	void UGA_Crouch::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (K2_HasAuthority())
	{
		ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());

		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			if (ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Crouching))
			{
				if (APlayerCharacter* Player = Cast<APlayerCharacter>(Character))
				{
					Player->MulticastPerformUnCrouch();
				}
				else if (Character)
				{
					Character->UnCrouch();
				}
				ASC->RemoveLooseGameplayTag(BaseGameplayTags::State_Crouching);
			}
		}

		if (APlayerCharacter* Player = Cast<APlayerCharacter>(Character))
		{
			Player->ApplyMovementSpeed();
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
