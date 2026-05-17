// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseMovementAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "PlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

UBaseMovementAttributeSet::UBaseMovementAttributeSet()
{
	BaseMoveSpeed = 600.0f;
	SprintSpeedMultiplier = 1.5f;
	CrouchSpeedMultiplier = 0.33f;
}

void UBaseMovementAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayAttribute& ModifiedAttribute = Data.EvaluatedData.Attribute;

	if (ModifiedAttribute == GetBaseMoveSpeedAttribute())
	{
		SetBaseMoveSpeed(FMath::Max(GetBaseMoveSpeed(), 1.0f));
	}

	if (ModifiedAttribute == GetSprintSpeedMultiplierAttribute())
	{
		SetSprintSpeedMultiplier(FMath::Max(GetSprintSpeedMultiplier(), 0.0f));
	}

	if (ModifiedAttribute == GetCrouchSpeedMultiplierAttribute())
	{
		SetCrouchSpeedMultiplier(FMath::Clamp(GetCrouchSpeedMultiplier(), 0.1f, 1.0f));
	}

	if (APlayerCharacter* Player = Cast<APlayerCharacter>(Data.Target.GetOwnerActor()))
	{
		Player->ApplyMovementSpeed();
	}
}
