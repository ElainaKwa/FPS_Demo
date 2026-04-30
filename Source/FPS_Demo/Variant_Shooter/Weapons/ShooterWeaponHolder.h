// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ShooterWeaponHolder.generated.h"

class AShooterWeapon;
class UAnimMontage;


UINTERFACE(MinimalAPI)
class UShooterWeaponHolder : public UInterface
{
	GENERATED_BODY()
};

class FPS_DEMO_API IShooterWeaponHolder
{
	GENERATED_BODY()

public:

	virtual void AttachWeaponMeshes(AShooterWeapon* Weapon) = 0;

	virtual void PlayFiringMontage(UAnimMontage* Montage) = 0;

	virtual void AddWeaponRecoil(float Recoil) = 0;

	virtual void UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize) = 0;

	virtual FVector GetWeaponTargetLocation() = 0;

	virtual void AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass) = 0;

	virtual void OnWeaponActivated(AShooterWeapon* Weapon) = 0;

	virtual void OnWeaponDeactivated(AShooterWeapon* Weapon) = 0;

	virtual void OnSemiWeaponRefire() = 0;
};
