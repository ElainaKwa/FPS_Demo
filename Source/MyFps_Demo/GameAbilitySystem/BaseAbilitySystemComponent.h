// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "BaseAbilitySystemComponent.generated.h"

UENUM()
enum class EAbilityInputID : uint8
{
	None		UMETA(Hidden),
	Confirm		UMETA(Hidden),
	Cancel		UMETA(Hidden),
	Fire,
	Reload,
	Crouch,
	Sprint,
	Jump
};

class ABaseWeapon;

UCLASS()
class MYFPS_DEMO_API UBaseAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	void SetCurrentWeapon(ABaseWeapon* InWeapon) { CurrentWeapon = InWeapon; }
	ABaseWeapon* GetCurrentWeapon() const { return CurrentWeapon; }

	void GrantDefaultAbilities();
	void GrantMovementAbilities();
	void GrantPassiveAbilities();
	void CancelFireAbility();
	void CancelSprintAbility();

	const FGameplayAbilitySpecHandle& GetFireAbilityHandle() const { return FireAbilityHandle; }
	const FGameplayAbilitySpecHandle& GetReloadAbilityHandle() const { return ReloadAbilityHandle; }
	const FGameplayAbilitySpecHandle& GetCrouchAbilityHandle() const { return CrouchAbilityHandle; }
	const FGameplayAbilitySpecHandle& GetSprintAbilityHandle() const { return SprintAbilityHandle; }
	const FGameplayAbilitySpecHandle& GetJumpAbilityHandle() const { return JumpAbilityHandle; }
	const FGameplayAbilitySpecHandle& GetStaminaRegenAbilityHandle() const { return StaminaRegenAbilityHandle; }

protected:
	UPROPERTY()
	TObjectPtr<ABaseWeapon> CurrentWeapon;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TSubclassOf<class UGameplayAbility> FireAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TSubclassOf<class UGameplayAbility> ReloadAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TSubclassOf<class UGameplayAbility> CrouchAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TSubclassOf<class UGameplayAbility> SprintAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TSubclassOf<class UGameplayAbility> JumpAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TSubclassOf<class UGameplayAbility> StaminaRegenAbilityClass;

	FGameplayAbilitySpecHandle FireAbilityHandle;
	FGameplayAbilitySpecHandle ReloadAbilityHandle;
	FGameplayAbilitySpecHandle CrouchAbilityHandle;
	FGameplayAbilitySpecHandle SprintAbilityHandle;
	FGameplayAbilitySpecHandle JumpAbilityHandle;
	FGameplayAbilitySpecHandle StaminaRegenAbilityHandle;
};
