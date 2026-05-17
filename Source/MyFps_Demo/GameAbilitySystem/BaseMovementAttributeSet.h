// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "BaseMovementAttributeSet.generated.h"

UCLASS()
class MYFPS_DEMO_API UBaseMovementAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UBaseMovementAttributeSet();

	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	FGameplayAttributeData BaseMoveSpeed;
ATTRIBUTE_ACCESSORS_BASIC(UBaseMovementAttributeSet, BaseMoveSpeed)
	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	FGameplayAttributeData SprintSpeedMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(UBaseMovementAttributeSet, SprintSpeedMultiplier)
	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	FGameplayAttributeData CrouchSpeedMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(UBaseMovementAttributeSet, CrouchSpeedMultiplier)
};
