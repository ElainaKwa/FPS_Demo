// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseCharacter.h"
#include "PlayerCharacter.generated.h"

class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UInputComponent;
class UBaseStaminaAttributeSet;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaminaUpdated, float, Stamina, float, MaxStamina);

UCLASS(Blueprintable)
class MYFPS_DEMO_API APlayerCharacter : public ABaseCharacter
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBaseStaminaAttributeSet> StaminaAttributeSet;

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

	UPROPERTY(EditAnywhere, Category = "Aim", meta = (ClampMin = 100.0f, ClampMax = 100000.0f, Units = "cm"))
	float MaxAimDistance = 10000.0f;

	UPROPERTY(EditAnywhere, Category = "Weapon")
	FName FirstPersonWeaponSocket = FName("HandGrip_R");

public:

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnStaminaUpdated OnStaminaUpdated;

	APlayerCharacter();

	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void AttachWeaponMeshes(ABaseWeapon* Weapon) override;
	virtual FVector GetWeaponTargetLocation() const override;
	virtual void PlayFiringMontage(UAnimMontage* Montage) override;
	virtual void PlayReloadMontage(UAnimMontage* Montage) override;
	virtual void AddWeaponRecoil(float RecoilAmount) override;
	virtual void UpdateHealthHUD() override;
	virtual void OnDeath() override;

	void UpdateStaminaHUD();

protected:
	void OnMove(const struct FInputActionValue& Value);
	void OnLook(const struct FInputActionValue& Value);
	void OnJumpStarted();
	void OnJumpEnded();
	void OnStartFiring();
	void OnStopFiring();
	void OnReload();
	void OnSwitchWeapon();
};
