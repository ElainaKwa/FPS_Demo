// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseGameplayAbility.h"
#include "GA_StaminaRegen.generated.h"

class UGameplayEffect;
struct FActiveGameplayEffectHandle;

UCLASS()
class MYFPS_DEMO_API UGA_StaminaRegen : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_StaminaRegen();

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

	UPROPERTY(EditDefaultsOnly, Category = "Regen")
	TSubclassOf<UGameplayEffect> StaminaRegenEffectClass;

private:
	UFUNCTION()
	void OnRegenCheck();

	void ScheduleRegenCheck();

	FActiveGameplayEffectHandle RegenEffectHandle;
};
