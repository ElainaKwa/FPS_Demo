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

// ------------------------------------------------------------------
// 委托：弹药数量变化 & 血量变化 → UI 更新
// ------------------------------------------------------------------
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBulletCountUpdated, int32, CurrentBullets, int32, MaxBullets);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthUpdated, float, Health, float, MaxHealth);

/**
 * 角色基类 — 所有玩家和敌人都继承此类。
 *
 * 架构职责：
 *   - 实现 IAbilitySystemInterface → 持有 ASC
 *   - 实现 IBaseWeaponHolder   → 武器交互接口（开火动画、后坐力、HUD）
 *   - 持有生命值/弹药 AttributeSet
 *   - 武器库存管理（添加、切换、掉落）
 *   - 死亡处理（State.Dead 标签 + Ragdoll 视觉）
 *
 * 玩家和敌人的差异通过虚函数重写实现：
 *   - AddWeaponRecoil  → 玩家：委托 RecoilComponent，敌人：空
 *   - PlayFiringMontage → 玩家：Multicast 到 FP+TP，敌人：仅 TP
 *   - OnDeath           → 玩家：禁用输入 + 死亡 UI，敌人：重生计时器
 */
UCLASS(Abstract, Blueprintable)
class MYFPS_DEMO_API ABaseCharacter : public ACharacter, public IBaseWeaponHolder, public IAbilitySystemInterface
{
	GENERATED_BODY()

protected:
	// ================================================================
	//  GAS 核心组件
	// ================================================================

	/** 能力系统组件 — 管理所有 GameplayAbility 的激活/取消/冷却 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBaseAbilitySystemComponent> AbilitySystemComponent;

	/** 生命值属性集 — Health / MaxHealth / IncomingDamage / IncomingHeal */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBaseHealthAttributeSet> HealthAttributeSet;

	/** 弹药属性集 — CurrentAmmo / MaxAmmo */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBaseWeaponAttributeSet> WeaponAttributeSet;

	// ================================================================
	//  武器系统
	// ================================================================

	/** TP 骨骼上挂载武器的 Socket 名称 */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	FName ThirdPersonWeaponSocket = FName("SOCKET_Weapon");

	/** 拥有的所有武器（库存） */
	UPROPERTY()
	TArray<TObjectPtr<ABaseWeapon>> OwnedWeapons;

	/** 当前装备的武器 — 网络复制，客户端通过 OnRep_CurrentWeapon 同步 ASC 和 HUD */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon)
	TObjectPtr<ABaseWeapon> CurrentWeapon;

	/** 默认武器类 — BeginPlay 时自动生成 */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	TSubclassOf<ABaseWeapon> DefaultWeaponClass;

public:
	// ================================================================
	//  委托（UI 绑定）
	// ================================================================

	/** 弹药变化时广播 → BulletCounterWidget 刷新 */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnBulletCountUpdated OnBulletCountUpdated;

	/** 血量变化时广播 → HealthBarWidget 刷新 */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnHealthUpdated OnHealthUpdated;

	ABaseCharacter();

	// ================================================================
	//  死亡查询
	// ================================================================

	/** 检查是否已死亡（State.Dead 标签）— 所有输入回调的前置守卫 */
	UFUNCTION(BlueprintCallable, Category = "Character")
	bool IsDead() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ================================================================
	//  初始化
	// ================================================================

	/** 初始化 ASC 的 ActorInfo + 服务端授予默认能力 */
	virtual void InitAbilitySystem();

	/** 生成 DefaultWeaponClass 并装备 */
	void SpawnDefaultWeapon();

public:
	// ================================================================
	//  IAbilitySystemInterface
	// ================================================================
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ================================================================
	//  IBaseWeaponHolder — 武器接口实现
	// ================================================================

	UFUNCTION(BlueprintPure, Category = "MyFps")
	virtual ABaseWeapon* GetCurrentWeapon() const override { return CurrentWeapon; }

	/** 将武器网格挂载到角色骨骼上（默认挂 TP Mesh） */
	virtual void AttachWeaponMeshes(ABaseWeapon* Weapon) override;

	/** 获取武器瞄准的目标位置（LineTrace 从角色眼睛向前）— 敌人 AI 用 */
	virtual FVector GetWeaponTargetLocation() const override;

	/** 播放开火蒙太奇 — 基类仅在 TP Mesh 上播放，Player 重写为双网格 Multicast */
	virtual void PlayFiringMontage(UAnimMontage* Montage) override;

	/** 播放换弹蒙太奇 — 同上 */
	virtual void PlayReloadMontage(UAnimMontage* Montage) override;

	/** 施加后坐力 — 基类空实现（敌人无后坐力），Player 重写委托给 RecoilComponent */
	virtual void AddWeaponRecoil(float RecoilAmount, float InterpSpeed, float RecoverySpeed, float MaxAccumulation) override;

	/** 刷新武器 HUD — 广播 OnBulletCountUpdated 委托 */
	virtual void UpdateWeaponHUD(int32 CurrentAmmo, int32 MaxAmmo) override;

	/** 刷新血量 HUD — 广播 OnHealthUpdated 委托 */
	virtual void UpdateHealthHUD();

	// ================================================================
	//  死亡系统
	// ================================================================

	/**
	 * 死亡入口 — PostGameplayEffectExecute 检测 Health<=0 时调用（仅服务端）。
	 *
	 * 流程：
	 *   1. State_Dead 标签守卫（防多帧重复触发）
	 *   2. AddLooseGameplayTag(State_Dead) → 阻塞所有能力激活
	 *   3. CancelAllAbilities() → 中断开火/换弹/移动
	 *   4. 武器 DropToGround()（仅服务端）
	 *   5. MulticastDeathVisuals() → 所有客户端开启 Ragdoll
	 */
	virtual void OnDeath();

	/**
	 * 死亡视觉 RPC — 从服务端广播到所有客户端。
	 * 关闭胶囊体碰撞，对 TP Mesh 开启物理模拟（Ragdoll）。
	 * PlayerCharacter 重写此方法额外处理 FP 隐藏 + 镜头后拉 + 死亡 UI。
	 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastDeathVisuals();

	// ================================================================
	//  武器管理
	// ================================================================

	/** CurrentWeapon 复制回调 — 客户端收到新武器后同步 ASC + AttributeSet + HUD */
	UFUNCTION()
	void OnRep_CurrentWeapon();

	/**
	 * 添加武器到库存。
	 * 如果已拥有同类型则切换到那把（避免重复生成），否则生成新武器。
	 * 仅服务端执行。
	 */
	UFUNCTION(BlueprintCallable, Category = "MyFps|Weapon")
	void AddWeapon(TSubclassOf<ABaseWeapon> WeaponClass);

	/** 切换到指定武器 — 停用旧武器 → 激活新武器 → 同步 ASC + AttributeSet */
	UFUNCTION(BlueprintCallable, Category = "MyFps|Weapon")
	void SwitchToWeapon(ABaseWeapon* NewWeapon);

protected:
	/** 在库存中查找指定类型的武器 */
	ABaseWeapon* FindWeaponOfClass(TSubclassOf<ABaseWeapon> WeaponClass) const;

	/** 把 Weapon.CurrentBullets/MagazineSize 同步到 WeaponAttributeSet */
	void SyncAttributeSetFromWeapon() const;
};
