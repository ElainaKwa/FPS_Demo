// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FPS_DemoCharacter.h"
#include "ShooterWeaponHolder.h"
#include "ShooterCharacter.generated.h"

class AShooterWeapon;
class UInputAction;
class UInputComponent;
class UPawnNoiseEmitterComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBulletCountUpdatedDelegate, int32, MagazineSize, int32, Bullets);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDamagedDelegate, float, LifePercent);

UCLASS(abstract)
class FPS_DEMO_API AShooterCharacter : public AFPS_DemoCharacter, public IShooterWeaponHolder
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UPawnNoiseEmitterComponent* PawnNoiseEmitter;

protected:

	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* FireAction;

	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* SwitchWeaponAction;

	UPROPERTY(EditAnywhere, Category ="Weapons")
	FName FirstPersonWeaponSocket = FName("HandGrip_R");

	UPROPERTY(EditAnywhere, Category ="Weapons")
	FName ThirdPersonWeaponSocket = FName("HandGrip_R");

	UPROPERTY(EditAnywhere, Category ="Aim", meta = (ClampMin = 0, ClampMax = 100000, Units = "cm"))
	float MaxAimDistance = 10000.0f;

	UPROPERTY(EditAnywhere, Category="Health")
	float MaxHP = 500.0f;

	float CurrentHP = 0.0f;

	UPROPERTY(EditAnywhere, Category="Team")
	uint8 TeamByte = 0;

	UPROPERTY(EditAnywhere, Category="Team")
	FName DeathTag = FName("Dead");

	TArray<AShooterWeapon*> OwnedWeapons;

	TObjectPtr<AShooterWeapon> CurrentWeapon;

	UPROPERTY(EditAnywhere, Category ="Destruction", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float RespawnTime = 5.0f;

	FTimerHandle RespawnTimer;

public:

	FBulletCountUpdatedDelegate OnBulletCountUpdated;

	FDamagedDelegate OnDamaged;

public:

	AShooterCharacter();

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

public:

	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

public:

	virtual void DoAim(float Yaw, float Pitch) override;

	virtual void DoMove(float Right, float Forward)  override;

	virtual void DoJumpStart()  override;

	virtual void DoJumpEnd()  override;

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoStartFiring();

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoStopFiring();

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoSwitchWeapon();

public:

	//~Begin IShooterWeaponHolder interface

	virtual void AttachWeaponMeshes(AShooterWeapon* Weapon) override;

	virtual void PlayFiringMontage(UAnimMontage* Montage) override;

	virtual void AddWeaponRecoil(float Recoil) override;

	virtual void UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize) override;

	virtual FVector GetWeaponTargetLocation() override;

	virtual void AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass) override;

	virtual void OnWeaponActivated(AShooterWeapon* Weapon) override;

	virtual void OnWeaponDeactivated(AShooterWeapon* Weapon) override;

	virtual void OnSemiWeaponRefire() override;

	//~End IShooterWeaponHolder interface

protected:

	AShooterWeapon* FindWeaponOfType(TSubclassOf<AShooterWeapon> WeaponClass) const;

	void Die();

	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta = (DisplayName = "On Death"))
	void BP_OnDeath();

	void OnRespawn();

public:

	bool IsDead() const;
};
