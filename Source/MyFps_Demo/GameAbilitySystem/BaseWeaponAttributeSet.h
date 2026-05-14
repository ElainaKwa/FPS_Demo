// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "BaseWeaponAttributeSet.generated.h"

UCLASS()
class MYFPS_DEMO_API UBaseWeaponAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UBaseWeaponAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentAmmo, Category = "Ammo")
	FGameplayAttributeData CurrentAmmo;
	ATTRIBUTE_ACCESSORS_BASIC(UBaseWeaponAttributeSet, CurrentAmmo)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxAmmo, Category = "Ammo")
	FGameplayAttributeData MaxAmmo;
	ATTRIBUTE_ACCESSORS_BASIC(UBaseWeaponAttributeSet, MaxAmmo)

	UFUNCTION()
	virtual void OnRep_CurrentAmmo(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_MaxAmmo(const FGameplayAttributeData& OldValue);
};
