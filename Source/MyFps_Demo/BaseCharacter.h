// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Weapons/BaseWeaponHolder.h"
#include "BaseCharacter.generated.h"

class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UInputComponent;
class UAnimMontage;
class ABaseWeapon;
class UBaseAbilitySystemComponent;
class UBaseWeaponAttributeSet;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBulletCountUpdated, int32, CurrentBullets, int32, MaxBullets);

UCLASS(Abstract, Blueprintable)
class MYFPS_DEMO_API ABaseCharacter : public ACharacter, public IBaseWeaponHolder, public IAbilitySystemInterface
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBaseAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBaseWeaponAttributeSet> WeaponAttributeSet;

public:

	// ---- Input Actions ----

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ReloadAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> SwitchWeaponAction;

	// ---- Weapon Sockets ----

	UPROPERTY(EditAnywhere, Category = "Weapon")
	FName FirstPersonWeaponSocket = FName("HandGrip_R");

	UPROPERTY(EditAnywhere, Category = "Weapon")
	FName ThirdPersonWeaponSocket = FName("HandGrip_R");

	// ---- Aim ----

	UPROPERTY(EditAnywhere, Category = "Aim", meta = (ClampMin = 100.0f, ClampMax = 100000.0f, Units = "cm"))
	float MaxAimDistance = 10000.0f;

	// ---- Weapon Inventory ----

	UPROPERTY()
	TArray<TObjectPtr<ABaseWeapon>> OwnedWeapons;

	UPROPERTY()
	TObjectPtr<ABaseWeapon> CurrentWeapon;

	// ---- Default Weapon ----

	UPROPERTY(EditAnywhere, Category = "Weapon")
	TSubclassOf<ABaseWeapon> DefaultWeaponClass;

public:

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnBulletCountUpdated OnBulletCountUpdated;

public:

	ABaseCharacter();

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void OnMove(const struct FInputActionValue& Value);

	void OnLook(const struct FInputActionValue& Value);

	void OnJumpStarted();

	void OnJumpEnded();

	void OnStartFiring();

	void OnStopFiring();

	void OnReload();

	void OnSwitchWeapon();

	void SpawnDefaultWeapon();

public:

	UFUNCTION(BlueprintPure, Category = "MyFps")
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	UFUNCTION(BlueprintPure, Category = "MyFps")
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	UFUNCTION(BlueprintPure, Category = "MyFps")
	virtual ABaseWeapon* GetCurrentWeapon() const override { return CurrentWeapon; }

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ---- IBaseWeaponHolder Interface ----

	virtual void AttachWeaponMeshes(ABaseWeapon* Weapon) override;

	virtual FVector GetWeaponTargetLocation() const override;

	virtual void PlayFiringMontage(UAnimMontage* Montage) override;

	virtual void PlayReloadMontage(UAnimMontage* Montage) override;

	virtual void AddWeaponRecoil(float RecoilAmount) override;

	virtual void UpdateWeaponHUD(int32 CurrentAmmo, int32 MaxAmmo) override;

	// ---- Weapon Management ----

	UFUNCTION(BlueprintCallable, Category = "MyFps|Weapon")
	void AddWeapon(TSubclassOf<ABaseWeapon> WeaponClass);

	UFUNCTION(BlueprintCallable, Category = "MyFps|Weapon")
	void SwitchToWeapon(ABaseWeapon* NewWeapon);

protected:

	ABaseWeapon* FindWeaponOfClass(TSubclassOf<ABaseWeapon> WeaponClass) const;

	void SyncAttributeSetFromWeapon() const;
};
