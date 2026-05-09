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
	Reload
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
	void CancelFireAbility();

	const FGameplayAbilitySpecHandle& GetFireAbilityHandle() const { return FireAbilityHandle; }
	const FGameplayAbilitySpecHandle& GetReloadAbilityHandle() const { return ReloadAbilityHandle; }

protected:
	UPROPERTY()
	TObjectPtr<ABaseWeapon> CurrentWeapon;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TSubclassOf<class UGameplayAbility> FireAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TSubclassOf<class UGameplayAbility> ReloadAbilityClass;

	FGameplayAbilitySpecHandle FireAbilityHandle;
	FGameplayAbilitySpecHandle ReloadAbilityHandle;
};
