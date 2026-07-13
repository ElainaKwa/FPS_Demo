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
class URecoilComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaminaUpdated, float, Stamina, float, MaxStamina);

// ------------------------------------------------------------------
//  输入模式枚举
// ------------------------------------------------------------------

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

/**
 * 玩家角色 — FPS 第一人称视角，Enhanced Input 驱动。
 *
 * 组件层级：
 *   CapsuleComponent
 *     ├── Mesh (TP, bOwnerNoSee)          — 其他玩家看到的身体
 *     ├── FirstPersonMesh (FP, bOnlyOwnerSee) — 自己看到的双臂
 *     ├── FirstPersonCamera               — 头显位置
 *     ├── RecoilComponent                 — 后坐力平滑执行器
 *     └── GAS (AbilitySystemComponent + AttributeSets)
 *
 * 输入架构（Listen Server）：
 *   按键 → Enhanced Input 回调 → IsDead() 守卫 → Server RPC → TryActivateAbility
 *   视觉效果 → Multicast RPC → 所有客户端播放 FP + TP 蒙太奇
 *
 * 网络角色：
 *   - 服务端：执行 GA 逻辑（开火/换弹/移动）、伤害判定、弹药消耗
 *   - 客户端：通过 Server RPC 转发输入，通过 Multicast RPC 接收视觉效果
 */
UCLASS(Blueprintable)
class MYFPS_DEMO_API APlayerCharacter : public ABaseCharacter
{
	GENERATED_BODY()

	// ================================================================
	//  FP 专属组件
	// ================================================================

	/** 第一人称手臂/武器骨骼网格 — 仅本地玩家可见 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	/** 第一人称摄像机 — 跟随胶囊体，使用 PawnControlRotation */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	// ================================================================
	//  Player 专属 AttributeSet
	// ================================================================

	/** 体力属性集 — Stamina / MaxStamina / IncomingStaminaCost */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBaseStaminaAttributeSet> StaminaAttributeSet;

	/** 移动速度属性集 — BaseMoveSpeed / SprintMultiplier / CrouchMultiplier */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBaseMovementAttributeSet> MovementAttributeSet;

	// ================================================================
	//  Enhanced Input 配置（蓝图指定资产）
	// ================================================================

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

	// ================================================================
	//  瞄准 & 后坐力 & 武器
	// ================================================================

	/** 瞄准最大距离（LineTrace 从摄像机向前） */
	UPROPERTY(EditAnywhere, Category = "Aim", meta = (ClampMin = 100.0f, ClampMax = 100000.0f, Units = "cm"))
	float MaxAimDistance = 10000.0f;

	/** 后坐力平滑组件 — 纯执行器，参数由武器 AddRecoil 时传入 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recoil", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URecoilComponent> RecoilComponent;

	/** FP 骨骼上挂载武器模型的 Socket 名称 */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	FName FirstPersonWeaponSocket = FName("SOCKET_Weapon");

public:
	// ================================================================
	//  委托 & 复制属性
	// ================================================================

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

	// ================================================================
	//  初始化
	// ================================================================
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void InitAbilitySystem() override;

	// ================================================================
	//  IBaseWeaponHolder 重写 — 玩家特化
	// ================================================================

	/** 挂载武器：同时挂 FP 和 TP 网格（基类只挂 TP） */
	virtual void AttachWeaponMeshes(ABaseWeapon* Weapon) override;

	/** 瞄准位置从 FP 摄像机出发（基类从眼睛高度出发） */
	virtual FVector GetWeaponTargetLocation() const override;

	/** 开火蒙太奇 → Multicast 到所有客户端的 FP + TP */
	virtual void PlayFiringMontage(UAnimMontage* Montage) override;

	/** 换弹蒙太奇 → Multicast 到所有客户端的 FP + TP */
	virtual void PlayReloadMontage(UAnimMontage* Montage) override;

	/** 后坐力 → 委托给 RecoilComponent + Multicast RPC */
	virtual void AddWeaponRecoil(float RecoilAmount, float InterpSpeed, float RecoverySpeed, float MaxAccumulation) override;

	virtual void UpdateHealthHUD() override;

	// ================================================================
	//  死亡系统（重写）
	// ================================================================

	/** 死亡入口：调用 Super + 额外的客户端处理在 MulticastDeathVisuals 中 */
	virtual void OnDeath() override;

	/**
	 * 死亡视觉 RPC — 覆盖基类：
	 *   ① 基类 Ragdoll
	 *   ② 隐藏 FP 手臂 + FP 武器
	 *   ③ 镜头后拉（-300, 0, 100）
	 *   ④ 本地玩家：显示鼠标 + 切换到 UI 模式 + 弹出死亡 Widget
	 */
	virtual void MulticastDeathVisuals_Implementation() override;

	void UpdateStaminaHUD();

	/** 根据当前 GAS 状态标签（Sprint/Crouch）调整移动速度 */
	void ApplyMovementSpeed();

protected:
	// ================================================================
	//  Enhanced Input 回调 — 全部以 IsDead() 守卫开头
	// ================================================================

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

	// ================================================================
	//  Server RPC — 客户端 → 服务端转发输入
	//  服务端收到后调用 TryActivateAbility / CancelAbilityHandle
	// ================================================================

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

	// ================================================================
	//  Multicast RPC — 服务端 → 所有客户端播放视觉效果
	// ================================================================

	/** 在所有客户端同时播放开火蒙太奇（FP + TP 双网格） */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayFiringMontage(UAnimMontage* Montage);

	/** 在所有客户端同时播放换弹蒙太奇（FP + TP 双网格） */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayReloadMontage(UAnimMontage* Montage);

	/** 在所有客户端累加后坐力目标值（Tick 中插值应用） */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastAddWeaponRecoil(float RecoilAmount, float InterpSpeed, float RecoverySpeed, float MaxAccumulation);

public:
	// ================================================================
	//  移动 Multicast RPC — 广播移动状态到所有客户端
	// ================================================================

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
	/** 体力平滑值 — Tick 中 FInterpTo 目标值，用于 UI 平滑显示 */
	float SmoothedStamina = 0.0f;
};
