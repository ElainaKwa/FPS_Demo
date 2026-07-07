// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseWeaponAttachment.generated.h"

class UStaticMeshComponent;

/**
 * 武器配件插槽枚举 — 每个槽位只能装一个配件。
 */
UENUM(BlueprintType)
enum class EBaseAttachmentSlot : uint8
{
	Muzzle		UMETA(DisplayName = "Muzzle (枪口)"),      // 补偿器/消音器
	Optic		UMETA(DisplayName = "Optic (瞄准镜)"),     // 红点/全息/ACOG
	Tactical	UMETA(DisplayName = "Tactical Device (战术设备)"), // 激光/手电
	Foregrip	UMETA(DisplayName = "Foregrip (前握把)"),  // 垂直/三角握把
	Magazine	UMETA(DisplayName = "Magazine (弹匣)")     // 扩容/快速弹匣
};

/**
 * 武器配件基类 — 挂载到武器骨骼 Socket 上的修改器。
 *
 * 每个配件通过倍率（Multiplier）修正武器属性：
 *   - RecoilMultiplier  < 1.0  → 补偿器，降低后坐力
 *   - DamageMultiplier  > 1.0  → 重型枪管，增伤
 *   - 倍率默认 1.0  → 不影响武器原有属性
 *
 * 弹匣容量用加法修正（MagazineSizeBonus）：
 *   - +10 → 扩容弹匣，多 10 发
 *
 * 配件通过 ABaseWeapon::EquipAttachment / RemoveAttachment 管理。
 * 装备/卸载时分别触发 BP_OnEquipped / BP_OnUnequipped 蓝图事件。
 */
UCLASS(Abstract, Blueprintable)
class MYFPS_DEMO_API ABaseWeaponAttachment : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	/** 配件的静态网格（模型） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> AttachmentMesh;

public:

	/** 配件的插槽类型 — 决定挂载到武器的哪个 Socket */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment")
	EBaseAttachmentSlot AttachmentSlot;

	// ================================================================
	//  后坐力修正倍率（乘算）
	// ================================================================

	/** 后坐力幅度倍率 — 补偿器 0.7~0.9，重型枪口 1.1~1.3 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment|Stats", meta = (ClampMin = 0.0f, ClampMax = 2.0f))
	float RecoilMultiplier = 1.0f;

	/** 后坐力上升速度倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment|Stats", meta = (ClampMin = 0.0f, ClampMax = 2.0f))
	float RecoilInterpMultiplier = 1.0f;

	/** 后坐力恢复速度倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment|Stats", meta = (ClampMin = 0.0f, ClampMax = 2.0f))
	float RecoilRecoveryMultiplier = 1.0f;

	/** 后坐力累积上限倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment|Stats", meta = (ClampMin = 0.0f, ClampMax = 2.0f))
	float RecoilMaxAccumulationMultiplier = 1.0f;

	// ================================================================
	//  其他修正
	// ================================================================

	/** 散布倍率 — 枪口收束器 0.8，消音器 1.1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment|Stats", meta = (ClampMin = 0.0f, ClampMax = 2.0f))
	float AimVarianceMultiplier = 1.0f;

	/** 弹匣容量加成（加算）— 扩容弹匣 +10，轻型弹匣 -5 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment|Stats")
	int32 MagazineSizeBonus = 0;

	/** 换弹时间倍率 — 快速弹匣 0.7，重型弹匣 1.3 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment|Stats", meta = (ClampMin = 0.5f, ClampMax = 5.0f))
	float ReloadTimeMultiplier = 1.0f;

	/** 枪声传播距离倍率 — 消音器 0.3~0.5 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment|Stats", meta = (ClampMin = 0.0f, ClampMax = 1.0f))
	float ShotLoudnessMultiplier = 1.0f;

	/** 伤害倍率 — 重型枪管 1.1，亚音速弹 0.8 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment|Stats")
	float DamageMultiplier = 1.0f;

public:

	ABaseWeaponAttachment();

	// ================================================================
	//  属性访问器
	// ================================================================

	UFUNCTION(BlueprintPure, Category = "Attachment")
	EBaseAttachmentSlot GetAttachmentSlot() const { return AttachmentSlot; }

	UFUNCTION(BlueprintPure, Category = "Attachment")
	UStaticMeshComponent* GetAttachmentMesh() const { return AttachmentMesh; }

	UFUNCTION(BlueprintPure, Category = "Attachment|Stats")
	float GetRecoilMultiplier() const { return RecoilMultiplier; }

	UFUNCTION(BlueprintPure, Category = "Attachment|Stats")
	float GetRecoilInterpMultiplier() const { return RecoilInterpMultiplier; }

	UFUNCTION(BlueprintPure, Category = "Attachment|Stats")
	float GetRecoilRecoveryMultiplier() const { return RecoilRecoveryMultiplier; }

	UFUNCTION(BlueprintPure, Category = "Attachment|Stats")
	float GetRecoilMaxAccumulationMultiplier() const { return RecoilMaxAccumulationMultiplier; }

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

	// ================================================================
	//  蓝图事件 — 装备/卸载时触发（音效、特效、UI 闪烁等）
	// ================================================================

	/** 配件装备到武器上时触发（蓝图实现） */
	UFUNCTION(BlueprintImplementableEvent, Category = "Attachment", meta = (DisplayName = "On Equipped"))
	void BP_OnEquipped(AActor* WeaponOwner);

	/** 配件从武器上移除时触发（蓝图实现） */
	UFUNCTION(BlueprintImplementableEvent, Category = "Attachment", meta = (DisplayName = "On Unequipped"))
	void BP_OnUnequipped(AActor* WeaponOwner);
};
