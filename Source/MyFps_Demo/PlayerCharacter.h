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
class UBaseMovementAttributeSet;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaminaUpdated, float, Stamina, float, MaxStamina);

UENUM(BlueprintType)
enum class ESprintInputMode : uint8
{
	Hold    UMETA(DisplayName = "按住"),
	Toggle  UMETA(DisplayName = "切换")
};

UENUM(BlueprintType)
enum class ECrouchInputMode : uint8
{
	Hold    UMETA(DisplayName = "按住"),
	Toggle  UMETA(DisplayName = "切换")
};

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBaseMovementAttributeSet> MovementAttributeSet;

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

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	ECrouchInputMode CrouchInputMode = ECrouchInputMode::Hold;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	ESprintInputMode SprintInputMode = ESprintInputMode::Hold;

	UPROPERTY(EditAnywhere, Category = "Aim", meta = (ClampMin = 100.0f, ClampMax = 100000.0f, Units = "cm"))
	float MaxAimDistance = 10000.0f;

	UPROPERTY(EditAnywhere, Category = "Weapon")
	FName FirstPersonWeaponSocket = FName("SOCKET_Weapon");

public:

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnStaminaUpdated OnStaminaUpdated;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Movement")
	bool bIsSprinting = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Weapon")
	bool bIsReloading = false;

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	ABaseWeapon* GetCurrentWeaponBP() const { return CurrentWeapon; }

	APlayerCharacter();

	virtual void Tick(float DeltaTime) override;

	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void InitAbilitySystem() override;

	virtual void AttachWeaponMeshes(ABaseWeapon* Weapon) override;
	virtual FVector GetWeaponTargetLocation() const override;
	virtual void PlayFiringMontage(UAnimMontage* Montage) override;
	virtual void PlayReloadMontage(UAnimMontage* Montage) override;
	virtual void AddWeaponRecoil(float RecoilAmount) override;
	virtual void UpdateHealthHUD() override;
	virtual void OnDeath() override;

	void UpdateStaminaHUD();

	void ApplyMovementSpeed();

protected:
	void OnMove(const struct FInputActionValue& Value);
	void OnLook(const struct FInputActionValue& Value);
	void OnJumpStarted();
	void OnJumpEnded();
	void OnStartFiring();
	void OnStopFiring();
	void OnReload();
	void OnSwitchWeapon();
	void OnCrouchStarted();
	void OnCrouchEnded();
	void OnSprintStarted();
	void OnSprintEnded();

	virtual void Landed(const FHitResult& Hit) override;

	UFUNCTION(Server, Reliable)
	void ServerStartFiring();

	UFUNCTION(Server, Reliable)
	void ServerStopFiring();

	UFUNCTION(Server, Reliable)
	void ServerReload();

	UFUNCTION(Server, Reliable)
	void ServerSwitchWeapon();

	UFUNCTION(Server, Reliable)
	void ServerCrouch();

	UFUNCTION(Server, Reliable)
	void ServerUnCrouch();

	UFUNCTION(Server, Reliable)
	void ServerSprint();

	UFUNCTION(Server, Reliable)
	void ServerUnSprint();

	UFUNCTION(Server, Reliable)
	void ServerJump();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayFiringMontage(UAnimMontage* Montage);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayReloadMontage(UAnimMontage* Montage);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastAddWeaponRecoil(float RecoilAmount);

public:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPerformCrouch();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPerformUnCrouch();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPerformJump();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartSprint();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStopSprint();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastSetReloading(bool bReloading);

protected:
	float SmoothedStamina = 0.0f;
};
