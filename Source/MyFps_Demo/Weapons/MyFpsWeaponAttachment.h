// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyFpsWeaponAttachment.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EMyFpsAttachmentSlot : uint8
{
	Muzzle		UMETA(DisplayName = "Muzzle (枪口)"),
	Optic		UMETA(DisplayName = "Optic (瞄准镜)"),
	Tactical	UMETA(DisplayName = "Tactical Device (战术设备)"),
	Foregrip	UMETA(DisplayName = "Foregrip (前握把)"),
	Magazine	UMETA(DisplayName = "Magazine (弹匣)")
};

UCLASS(Abstract, Blueprintable)
class MYFPS_DEMO_API AMyFpsWeaponAttachment : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> AttachmentMesh;

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment")
	EMyFpsAttachmentSlot AttachmentSlot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment|Stats", meta = (ClampMin = 0.0f, ClampMax = 2.0f))
	float RecoilMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment|Stats", meta = (ClampMin = 0.0f, ClampMax = 2.0f))
	float AimVarianceMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment|Stats")
	int32 MagazineSizeBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment|Stats", meta = (ClampMin = 0.5f, ClampMax = 5.0f))
	float ReloadTimeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment|Stats", meta = (ClampMin = 0.0f, ClampMax = 1.0f))
	float ShotLoudnessMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment|Stats")
	float DamageMultiplier = 1.0f;

public:

	AMyFpsWeaponAttachment();

	UFUNCTION(BlueprintPure, Category = "Attachment")
	EMyFpsAttachmentSlot GetAttachmentSlot() const { return AttachmentSlot; }

	UFUNCTION(BlueprintPure, Category = "Attachment")
	UStaticMeshComponent* GetAttachmentMesh() const { return AttachmentMesh; }

	UFUNCTION(BlueprintPure, Category = "Attachment|Stats")
	float GetRecoilMultiplier() const { return RecoilMultiplier; }

	UFUNCTION(BlueprintPure, Category = "Attachment|Stats")
	float GetAimVarianceMultiplier() const { return AimVarianceMultiplier; }

	UFUNCTION(BlueprintPure, Category = "Attachment|Stats")
	int32 GetMagazineSizeBonus() const { return MagazineSizeBonus; }

	UFUNCTION(BlueprintPure, Category = "Attachment|Stats")
	float GetReloadTimeMultiplier() const { return ReloadTimeMultiplier; }

	UFUNCTION(BlueprintPure, Category = "Attachment|Stats")
	float GetShotLoudnessMultiplier() const { return ShotLoudnessMultiplier; }

	UFUNCTION(BlueprintPure, Category = "Attachment|Stats")
	float GetDamageMultiplier() const { return DamageMultiplier; }

	UFUNCTION(BlueprintImplementableEvent, Category = "Attachment", meta = (DisplayName = "On Equipped"))
	void BP_OnEquipped(AActor* WeaponOwner);

	UFUNCTION(BlueprintImplementableEvent, Category = "Attachment", meta = (DisplayName = "On Unequipped"))
	void BP_OnUnequipped(AActor* WeaponOwner);
};
