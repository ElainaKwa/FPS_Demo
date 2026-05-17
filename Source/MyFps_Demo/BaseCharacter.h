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
class UBaseHealthAttributeSet;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBulletCountUpdated, int32, CurrentBullets, int32, MaxBullets);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthUpdated, float, Health, float, MaxHealth);

UCLASS(Abstract, Blueprintable)
class MYFPS_DEMO_API ABaseCharacter : public ACharacter, public IBaseWeaponHolder, public IAbilitySystemInterface
{
	GENERATED_BODY()

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBaseAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBaseHealthAttributeSet> HealthAttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBaseWeaponAttributeSet> WeaponAttributeSet;

	UPROPERTY(EditAnywhere, Category = "Weapon")
	FName ThirdPersonWeaponSocket = FName("SOCKET_Weapon");

	UPROPERTY()
	TArray<TObjectPtr<ABaseWeapon>> OwnedWeapons;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon)
	TObjectPtr<ABaseWeapon> CurrentWeapon;

	UPROPERTY(EditAnywhere, Category = "Weapon")
	TSubclassOf<ABaseWeapon> DefaultWeaponClass;

public:

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnBulletCountUpdated OnBulletCountUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnHealthUpdated OnHealthUpdated;

	ABaseCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void InitAbilitySystem();

	void SpawnDefaultWeapon();

public:
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "MyFps")
	virtual ABaseWeapon* GetCurrentWeapon() const override { return CurrentWeapon; }

	virtual void AttachWeaponMeshes(ABaseWeapon* Weapon) override;
	virtual FVector GetWeaponTargetLocation() const override;
	virtual void PlayFiringMontage(UAnimMontage* Montage) override;
	virtual void PlayReloadMontage(UAnimMontage* Montage) override;
	virtual void AddWeaponRecoil(float RecoilAmount) override;
	virtual void UpdateWeaponHUD(int32 CurrentAmmo, int32 MaxAmmo) override;

	virtual void UpdateHealthHUD();

	virtual void OnDeath();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastDeathVisuals();

	UFUNCTION()
	void OnRep_CurrentWeapon();

	UFUNCTION(BlueprintCallable, Category = "MyFps|Weapon")
	void AddWeapon(TSubclassOf<ABaseWeapon> WeaponClass);

	UFUNCTION(BlueprintCallable, Category = "MyFps|Weapon")
	void SwitchToWeapon(ABaseWeapon* NewWeapon);

protected:
	ABaseWeapon* FindWeaponOfClass(TSubclassOf<ABaseWeapon> WeaponClass) const;

	void SyncAttributeSetFromWeapon() const;
};
