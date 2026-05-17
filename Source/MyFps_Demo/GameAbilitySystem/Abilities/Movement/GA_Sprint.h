// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseGameplayAbility.h"
#include "GA_Sprint.generated.h"

class UGameplayEffect;
struct FActiveGameplayEffectHandle;

UCLASS()
class MYFPS_DEMO_API UGA_Sprint : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Sprint();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	UPROPERTY(EditDefaultsOnly, Category = "Sprint|Cost")
	TSubclassOf<UGameplayEffect> StaminaDrainEffectClass;

private:
	FActiveGameplayEffectHandle StaminaDrainHandle;
};
