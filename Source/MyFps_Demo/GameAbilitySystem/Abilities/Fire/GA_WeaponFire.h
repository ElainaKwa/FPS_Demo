// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseGameplayAbility.h"
#include "GA_WeaponFire.generated.h"

UCLASS()
class MYFPS_DEMO_API UGA_WeaponFire : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_WeaponFire();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Effects")
	TSubclassOf<class UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "GAS|Effects")
	TSubclassOf<class UGameplayEffect> AmmoCostEffectClass;

	bool bInputReleased = false;

	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	void PerformFire();

	UFUNCTION()
	void OnRefireReady();

	class ABaseWeapon* GetWeapon() const;
};
