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

/**
 * 武器基类 — 纯数据容器 + 可视化，不含任何行为逻辑。
 *
 * 核心设计原则：
 *   - 武器不包含开火/换弹逻辑，所有行为由 GAS Ability 驱动
 *   - 属性通过 GetEffective*() 汇总配件修正值
 *   - FP/TP 双网格：FP 只对 Owner 可见，TP 对其他玩家可见
 *   - 网络复制：CurrentBullets 复制到客户端 → OnRep 驱动 HUD 更新
 *
 * 属性分类：
 *   Ammo      — 弹药（MagazineSize, CurrentBullets）
 *   Fire Mode — 射速、全自动/半自动
 *   Damage    — 伤害、射程、伤害类型
 *   Recoil    — 后坐力（4 个独立参数）+ 散布
 *   Animation — 蒙太奇、AnimInstance 类引用
 *   Reload    — 换弹时间、弹匣模型、计时节点
 *   Attachments — Socket 名称映射 + 装备的配件
 */
UCLASS(Abstract, Blueprintable)
class MYFPS_DEMO_API ABaseWeapon : public AActor
{
	GENERATED_BODY()

	// ================================================================
	//  网格组件
	// ================================================================

	/** 第一人称武器模型 — 仅本地玩家可见，挂载在 Player 的 FP 手臂骨骼上 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	/** 第三人称武器模型 — 对其他玩家可见，挂载在 TP 身体骨骼上 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> ThirdPersonMesh;

	/** 换弹时左手持有的替换弹匣（静态网格） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> LeftHandMagazineMesh;

public:

	/** 武器持有者接口引用 — 用于回调 AttachWeaponMeshes / UpdateHUD / PlayMontage 等 */
	IBaseWeaponHolder* WeaponOwner;

	// ================================================================
	//  弹药
	// ================================================================

	/** 弹匣容量（基础值，配件可加成） */
	UPROPERTY(EditAnywhere, Category = "Ammo", meta = (ClampMin = 1, ClampMax = 200))
	int32 MagazineSize = 30;

	/** 当前弹匣内子弹数 — 网络复制，客户端 OnRep 触发 HUD 刷新 */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentBullets)
	int32 CurrentBullets = 0;

	// ================================================================
	//  射击模式
	// ================================================================

	/** 是否全自动：true = 按住连射，false = 每按一次射一发 */
	UPROPERTY(EditAnywhere, Category = "Fire Mode")
	bool bFullAuto = true;

	/** 射击间隔（秒）— 全自动模式下的连射速率 */
	UPROPERTY(EditAnywhere, Category = "Fire Mode", meta = (ClampMin = 0.01f, ClampMax = 5.0f, Units = "s"))
	float RefireRate = 0.1f;

	/** 上一次射击的时间戳（用于限速） */
	float TimeOfLastShot = 0.0f;

	/** 是否正在换弹（GA_WeaponReload 设置） */
	bool bIsReloading = false;

	/** 是否正在开火中（GA_WeaponFire 设置） */
	bool bIsFiring = false;

	// ================================================================
	//  伤害
	// ================================================================

	/** 单发伤害（基础值，配件可加成） */
	UPROPERTY(EditAnywhere, Category = "Damage", meta = (ClampMin = 0.0f, ClampMax = 1000.0f))
	float HitDamage = 25.0f;

	/** 最大射程（超出此距离的 LineTrace 视为未命中） */
	UPROPERTY(EditAnywhere, Category = "Damage", meta = (ClampMin = 100.0f, ClampMax = 100000.0f, Units = "cm"))
	float MaxRange = 10000.0f;

	/** 伤害类型（用于伤害事件筛选和 UI 显示） */
	UPROPERTY(EditAnywhere, Category = "Damage")
	TSubclassOf<UDamageType> DamageTypeClass;

	// ================================================================
	//  后坐力 & 散布
	// ================================================================

	/** 单发后坐力幅度 — 每击发一次加到 RecoilComponent.TargetPitch */
	UPROPERTY(EditAnywhere, Category = "Recoil", meta = (ClampMin = 0.0f, ClampMax = 100.0f))
	float FiringRecoil = 1.0f;

	/** 弹道散布角度（度）— 子弹方向在圆锥内随机偏移 */
	UPROPERTY(EditAnywhere, Category = "Recoil", meta = (ClampMin = 0.0f, ClampMax = 90.0f, Units = "Degrees"))
	float AimVariance = 2.0f;

	/** 后坐力上升追赶速度 — RecoilComponent.Tick 中 FInterpTo 的 InterpSpeed */
	UPROPERTY(EditAnywhere, Category = "Recoil", meta = (ClampMin = 0.1f, ClampMax = 100.0f))
	float RecoilInterpSpeed = 15.0f;

	/** 后坐力恢复速度 — 用于 Target 衰减 + Current 回落 */
	UPROPERTY(EditAnywhere, Category = "Recoil", meta = (ClampMin = 0.0f, ClampMax = 50.0f))
	float RecoilRecoverySpeed = 5.0f;

	/** 后坐力累积上限 — 连射时 TargetPitch 的硬天花板 */
	UPROPERTY(EditAnywhere, Category = "Recoil", meta = (ClampMin = 0.0f))
	float RecoilMaxAccumulation = 20.0f;

	// ================================================================
	//  枪口
	// ================================================================

	/** 枪口 Socket 名称 — 用于获取子弹发射位置 */
	UPROPERTY(EditAnywhere, Category = "Muzzle")
	FName MuzzleSocketName = FName("SOCKET_Muzzle");

	/** 枪口向前的偏移量（cm）— 避免子弹从枪管侧面出发 */
	UPROPERTY(EditAnywhere, Category = "Muzzle", meta = (ClampMin = 0.0f, ClampMax = 1000.0f, Units = "cm"))
	float MuzzleOffset = 10.0f;

	// ================================================================
	//  动画
	// ================================================================

	/** 开火蒙太奇 — 由 GA_WeaponFire 触发，通过 Multicast RPC 播放 */
	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimMontage> FiringMontage;

	/** FP 武器动画蓝图类 — 用于武器自身动画叠加（如枪机运动） */
	UPROPERTY(EditAnywhere, Category = "Animation")
	TSubclassOf<UAnimInstance> FirstPersonAnimInstanceClass;

	/** TP 武器动画蓝图类 — 同上但用于第三人称 */
	UPROPERTY(EditAnywhere, Category = "Animation")
	TSubclassOf<UAnimInstance> ThirdPersonAnimInstanceClass;

	// ================================================================
	//  换弹
	// ================================================================

	/** 换弹总时间（秒）— 基础值，配件可加成 */
	UPROPERTY(EditAnywhere, Category = "Reload", meta = (ClampMin = 0.1f, ClampMax = 10.0f, Units = "s"))
	float ReloadTime = 2.0f;

	/** 换弹蒙太奇 */
	UPROPERTY(EditAnywhere, Category = "Reload")
	TObjectPtr<UAnimMontage> ReloadMontage;

	/** 掉落弹匣的物理模型 */
	UPROPERTY(EditAnywhere, Category = "Reload")
	TObjectPtr<UStaticMesh> DroppedMagazineMesh;

	/** 换弹开始到弹匣掉落的时间偏移（秒） */
	UPROPERTY(EditAnywhere, Category = "Reload", meta = (ClampMin = 0.0f, ClampMax = 2.0f, Units = "s"))
	float MagazineDropDelay = 0.15f;

	/** 弹匣插入到换弹结束的时间偏移（秒）— 用于对齐动画节奏 */
	UPROPERTY(EditAnywhere, Category = "Reload", meta = (ClampMin = 0.0f, ClampMax = 2.0f, Units = "s"))
	float MagazineInsertBeforeEnd = 0.4f;

	/** 左手骨骼 Socket 名称 — 换弹时挂载替换弹匣的位置 */
	UPROPERTY(EditAnywhere, Category = "Reload")
	FName LeftHandSocketName = FName("hand_l");

	// ================================================================
	//  配件系统
	// ================================================================

	/** 各配件插槽对应的 Socket 名称（用于 AttachToComponent） */
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

	/** 当前装备的配件表（Slot → Attachment Actor） */
	UPROPERTY()
	TMap<EBaseAttachmentSlot, TObjectPtr<ABaseWeaponAttachment>> EquippedAttachments;

public:

	ABaseWeapon();

	/** FP 武器模型在挂载时的额外旋转偏移（蓝图可调，微调握持角度） */
	UPROPERTY(EditAnywhere, Category = "Mesh")
	FRotator FirstPersonMeshRotationOffset = FRotator::ZeroRotator;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Owner 销毁时连带销毁武器 */
	UFUNCTION()
	void OnOwnerDestroyed(AActor* DestroyedActor);

	/** CurrentBullets 复制回调 → 驱动 HUD 刷新 */
	UFUNCTION()
	void OnRep_CurrentBullets();

public:
	// ================================================================
	//  武器激活/停用
	// ================================================================

	/** 装备武器时调用：显示模型 + 刷新 HUD */
	void ActivateWeapon();

	/** 收起武器时调用：隐藏模型 */
	void DeactivateWeapon();

	/** 武器掉落：解除 Owner 绑定 → 开物理 → 60s 后自动销毁 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void DropToGround();

	/** 武器掉落视觉 RPC — 所有客户端分离 TP Mesh + 开启物理 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastDropToGroundVisuals();

	// ================================================================
	//  战斗计算
	// ================================================================

	/** 获取枪口的世界空间位置（从 FP Mesh 的 MuzzleSocket 读取） */
	FVector GetMuzzleLocation() const;

	/** 计算散布后的弹道方向 — 在 AimDirection 圆锥内随机偏移 AimVariance 度 */
	FVector CalculateSpread(const FVector& AimDirection) const;

	/** 根据配件插槽获取对应的 Socket 名称 */
	FName GetSocketNameForSlot(EBaseAttachmentSlot Slot) const;

public:
	// ================================================================
	//  属性访问器
	// ================================================================

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

	// ================================================================
	//  配件 API
	// ================================================================

	/** 装备配件到指定插槽（覆盖已有） */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Attachments")
	void EquipAttachment(EBaseAttachmentSlot Slot, TSubclassOf<ABaseWeaponAttachment> AttachmentClass);

	/** 移除指定插槽的配件并销毁 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Attachments")
	void RemoveAttachment(EBaseAttachmentSlot Slot);

	UFUNCTION(BlueprintPure, Category = "Weapon|Attachments")
	ABaseWeaponAttachment* GetAttachment(EBaseAttachmentSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Weapon|Attachments")
	bool HasAttachment(EBaseAttachmentSlot Slot) const;

	/** 移除并销毁所有配件 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Attachments")
	void ClearAllAttachments();

	// ================================================================
	//  GetEffective* — 汇总配件修正后的实际值
	//  模式：基础值 × (配件1倍率 × 配件2倍率 × ...) 或 基础值 + 配件加成
	// ================================================================

	float GetEffectiveDamage() const;

	float GetEffectiveRecoil() const;
	float GetEffectiveRecoilInterpSpeed() const;
	float GetEffectiveRecoilRecoverySpeed() const;
	float GetEffectiveRecoilMaxAccumulation() const;

	float GetEffectiveAimVariance() const;

	/** 有效换弹时间 = ReloadTime × 所有配件倍率 */
	float GetEffectiveReloadTime() const;

	/** 有效弹匣容量 = MagazineSize + 所有配件加成（弹匣倍率为加法） */
	int32 GetEffectiveMagazineSize() const;

	// ================================================================
	//  换弹辅助方法（由 GA_WeaponReload 的计时 Task 调用）
	// ================================================================

	/** 生成掉落弹匣物理 Actor + 左手显示替换弹匣 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Reload")
	void DropMagazine();

	/** 隐藏左手弹匣，恢复武器弹匣槽配件可见 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Reload")
	void InsertMagazine();

	/** 清理换弹视觉（左手弹匣隐藏 + 弹匣槽恢复） */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Reload")
	void CancelReloadVisuals();

	// ---- 换弹视觉 Multicast RPC ----

	UFUNCTION(NetMulticast, Reliable)
	void MulticastDropMagazineVisuals(UStaticMesh* Mesh);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastInsertMagazineVisuals();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastCancelReloadVisuals();
};
