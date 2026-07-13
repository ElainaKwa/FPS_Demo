// Copyright Epic Games, Inc. All Rights Reserved.

#include "RecoilComponent.h"
#include "GameFramework/Pawn.h"

URecoilComponent::URecoilComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void URecoilComponent::AddRecoil(float Amount, float InInterpSpeed, float InRecoverySpeed, float InMaxAccumulation)
{
	InterpSpeed = InInterpSpeed;
	RecoverySpeed = InRecoverySpeed;
	MaxAccumulation = InMaxAccumulation;
	TargetPitch = FMath::Clamp(TargetPitch + Amount, 0.0f, MaxAccumulation);
}

void URecoilComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const float OldPitch = CurrentPitch;

	// Use recovery speed when the target is falling, rise speed when climbing
	if (TargetPitch < CurrentPitch - KINDA_SMALL_NUMBER)
	{
		CurrentPitch = FMath::FInterpTo(CurrentPitch, TargetPitch, DeltaTime, RecoverySpeed);
	}
	else
	{
		CurrentPitch = FMath::FInterpTo(CurrentPitch, TargetPitch, DeltaTime, InterpSpeed);
	}

	const float DeltaPitch = CurrentPitch - OldPitch;
	if (!FMath::IsNearlyZero(DeltaPitch))
	{
		if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			// Negative pitch = look up (muzzle climb)
			OwnerPawn->AddControllerPitchInput(-DeltaPitch);
		}
	}

	// Target naturally decays back to zero
	TargetPitch = FMath::FInterpTo(TargetPitch, 0.0f, DeltaTime, RecoverySpeed);
}
