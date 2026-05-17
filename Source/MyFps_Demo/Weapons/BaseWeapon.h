// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseWeaponHolder.h"
#include "BaseWeapon.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class UAnimMontage;
class UAnimInstance;
enum class EBaseAttachmentSlot : uint8;
class ABaseWeaponAttachment;

UCLASS(Abstract, Blueprintable)
class MYFPS_DEMO_API ABaseWeapon : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> ThirdPersonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> LeftHandMagazineMesh;

public:

	IBaseWeaponHolder* WeaponOwner;

	// ---- Ammo ----

	UPROPERTY(EditAnywhere, Category = "Ammo", meta = (ClampMin = 1, ClampMax = 200))
	int32 MagazineSize = 30;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentBullets)
	int32 CurrentBullets = 0;

	// ---- Fire Mode ----

	UPROPERTY(EditAnywhere, Category = "Fire Mode")
	bool bFullAuto = true;

	UPROPERTY(EditAnywhere, Category = "Fire Mode", meta = (ClampMin = 0.01f, ClampMax = 5.0f, Units = "s"))
	float RefireRate = 0.1f;

	float TimeOfLastShot = 0.0f;

	bool bIsReloading = false;

	bool bIsFiring = false;

	// ---- Damage ----

	UPROPERTY(EditAnywhere, Category = "Damage", meta = (ClampMin = 0.0f, ClampMax = 1000.0f))
	float HitDamage = 25.0f;

	UPROPERTY(EditAnywhere, Category = "Damage", meta = (ClampMin = 100.0f, ClampMax = 100000.0f, Units = "cm"))
	float MaxRange = 10000.0f;

	UPROPERTY(EditAnywhere, Category = "Damage")
	TSubclassOf<UDamageType> DamageTypeClass;

	// ---- Recoil & Spread ----

	UPROPERTY(EditAnywhere, Category = "Recoil", meta = (ClampMin = 0.0f, ClampMax = 100.0f))
	float FiringRecoil = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Recoil", meta = (ClampMin = 0.0f, ClampMax = 90.0f, Units = "Degrees"))
	float AimVariance = 2.0f;

	// ---- Muzzle ----

	UPROPERTY(EditAnywhere, Category = "Muzzle")
	FName MuzzleSocketName = FName("SOCKET_Muzzle");

	UPROPERTY(EditAnywhere, Category = "Muzzle", meta = (ClampMin = 0.0f, ClampMax = 1000.0f, Units = "cm"))
	float MuzzleOffset = 10.0f;

	// ---- Animation ----

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimMontage> FiringMontage;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TSubclassOf<UAnimInstance> FirstPersonAnimInstanceClass;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TSubclassOf<UAnimInstance> ThirdPersonAnimInstanceClass;

	// ---- Reload ----

	UPROPERTY(EditAnywhere, Category = "Reload", meta = (ClampMin = 0.1f, ClampMax = 10.0f, Units = "s"))
	float ReloadTime = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Reload")
	TObjectPtr<UAnimMontage> ReloadMontage;

	UPROPERTY(EditAnywhere, Category = "Reload")
	TObjectPtr<UStaticMesh> DroppedMagazineMesh;

	UPROPERTY(EditAnywhere, Category = "Reload", meta = (ClampMin = 0.0f, ClampMax = 2.0f, Units = "s"))
	float MagazineDropDelay = 0.15f;

	UPROPERTY(EditAnywhere, Category = "Reload", meta = (ClampMin = 0.0f, ClampMax = 2.0f, Units = "s"))
	float MagazineInsertBeforeEnd = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Reload")
	FName LeftHandSocketName = FName("hand_l");

	// ---- Attachment Slots ----

	UPROPERTY(EditAnywhere, Category = "Attachments|Socket Names")
	FName MuzzleSocket = FName("SOCKET_Muzzle");

	UPROPERTY(EditAnywhere, Category = "Attachments|Socket Names")
	FName OpticSocket = FName("SOCKET_Optic");

	UPROPERTY(EditAnywhere, Category = "Attachments|Socket Names")
	FName TacticalSocket = FName("SOCKET_Tactical");

	UPROPERTY(EditAnywhere, Category = "Attachments|Socket Names")
	FName ForegripSocket = FName("SOCKET_Foregrip");

	UPROPERTY(EditAnywhere, Category = "Attachments|Socket Names")
	FName MagazineSocket = FName("SOCKET_Magazine");

	UPROPERTY()
	TMap<EBaseAttachmentSlot, TObjectPtr<ABaseWeaponAttachment>> EquippedAttachments;

public:

	ABaseWeapon();

	UPROPERTY(EditAnywhere, Category = "Mesh")
	FRotator FirstPersonMeshRotationOffset = FRotator::ZeroRotator;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnOwnerDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void OnRep_CurrentBullets();

public:

	void ActivateWeapon();

	void DeactivateWeapon();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void DropToGround();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastDropToGroundVisuals();

	FVector GetMuzzleLocation() const;

	FVector CalculateSpread(const FVector& AimDirection) const;

	FName GetSocketNameForSlot(EBaseAttachmentSlot Slot) const;

public:

	UFUNCTION(BlueprintPure, Category = "Weapon")
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	USkeletalMeshComponent* GetThirdPersonMesh() const { return ThirdPersonMesh; }

	const TSubclassOf<UAnimInstance>& GetFirstPersonAnimInstanceClass() const { return FirstPersonAnimInstanceClass; }

	const TSubclassOf<UAnimInstance>& GetThirdPersonAnimInstanceClass() const { return ThirdPersonAnimInstanceClass; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
	int32 GetCurrentBullets() const { return CurrentBullets; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
	int32 GetMaxAmmo() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsReloading() const { return bIsReloading; }

	// ---- Attachment API ----

	UFUNCTION(BlueprintCallable, Category = "Weapon|Attachments")
	void EquipAttachment(EBaseAttachmentSlot Slot, TSubclassOf<ABaseWeaponAttachment> AttachmentClass);

	UFUNCTION(BlueprintCallable, Category = "Weapon|Attachments")
	void RemoveAttachment(EBaseAttachmentSlot Slot);

	UFUNCTION(BlueprintPure, Category = "Weapon|Attachments")
	ABaseWeaponAttachment* GetAttachment(EBaseAttachmentSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Weapon|Attachments")
	bool HasAttachment(EBaseAttachmentSlot Slot) const;

	UFUNCTION(BlueprintCallable, Category = "Weapon|Attachments")
	void ClearAllAttachments();

	float GetEffectiveDamage() const;

	float GetEffectiveRecoil() const;

	float GetEffectiveAimVariance() const;

	float GetEffectiveReloadTime() const;

	int32 GetEffectiveMagazineSize() const;

	// ---- Reload Helpers ----

	UFUNCTION(BlueprintCallable, Category = "Weapon|Reload")
	void DropMagazine();

	UFUNCTION(BlueprintCallable, Category = "Weapon|Reload")
	void InsertMagazine();

	UFUNCTION(BlueprintCallable, Category = "Weapon|Reload")
	void CancelReloadVisuals();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastDropMagazineVisuals(UStaticMesh* Mesh);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastInsertMagazineVisuals();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastCancelReloadVisuals();

protected:
};
