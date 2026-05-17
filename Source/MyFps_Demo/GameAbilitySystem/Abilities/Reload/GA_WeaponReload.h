// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseGameplayAbility.h"
#include "GA_WeaponReload.generated.h"

UCLASS()
class MYFPS_DEMO_API UGA_WeaponReload : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_WeaponReload();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Effects")
	TSubclassOf<class UGameplayEffect> ReloadAmmoEffectClass;

	UFUNCTION()
	void OnDropMagazine();

	UFUNCTION()
	void OnInsertMagazine();

	UFUNCTION()
	void OnReloadComplete();

	class ABaseWeapon* GetWeapon() const;
};
