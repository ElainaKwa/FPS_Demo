// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BaseWeaponHolder.generated.h"

class ABaseWeapon;
class UAnimMontage;

UINTERFACE(MinimalAPI)
class UBaseWeaponHolder : public UInterface
{
	GENERATED_BODY()
};

class MYFPS_DEMO_API IBaseWeaponHolder
{
	GENERATED_BODY()

public:

	virtual void AttachWeaponMeshes(ABaseWeapon* Weapon) = 0;

	virtual FVector GetWeaponTargetLocation() const = 0;

	virtual void PlayFiringMontage(UAnimMontage* Montage) {}

	virtual void PlayReloadMontage(UAnimMontage* Montage) {}

	virtual void AddWeaponRecoil(float RecoilAmount) {}

	virtual void UpdateWeaponHUD(int32 CurrentAmmo, int32 MaxAmmo) {}

	virtual ABaseWeapon* GetCurrentWeapon() const = 0;
};
