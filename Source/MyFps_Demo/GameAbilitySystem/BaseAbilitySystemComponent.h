// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "BaseAbilitySystemComponent.generated.h"

/**
 * 能力输入 ID — 用于 GAS 的 AbilityInputID 映射。
 *
 * 每个 AbilitySpec 在 GiveAbility 时绑定一个 InputID。
 * 服务端通过 TryActivateAbility(Handle) 激活，不依赖 LocalInputPressed 机制。
 *
 * None/Confirm/Cancel 为 UE 内置保留值，自定义从 Fire 开始。
 */
UENUM()
enum class EAbilityInputID : uint8
{
	None		UMETA(Hidden),
	Confirm		UMETA(Hidden),
	Cancel		UMETA(Hidden),
	Fire,       // 开火（全自动/半自动）
	Reload,     // 换弹
	Crouch,     // 下蹲
	Sprint,     // 冲刺
	Jump        // 跳跃
};

class ABaseWeapon;

/**
 * 自定义 AbilitySystemComponent — 项目的 GAS 核心。
 *
 * 职责：
 *   1. 持有当前武器引用（能力通过 ASC 获取武器数据而非直接依赖角色类型）
 *   2. 管理能力 Spec 的授予（Grant）和 Handle 缓存
 *   3. 提供便捷的 Cancel 方法（CancelFireAbility / CancelSprintAbility）
 *
 * 能力分三组授予：
 *   - GrantDefaultAbilities()   → Fire + Reload  （玩家和敌人都需要）
 *   - GrantMovementAbilities()  → Crouch + Sprint + Jump （仅玩家）
 *   - GrantPassiveAbilities()   → StaminaRegen （仅玩家，授予后立即激活）
 *
 * 蓝图配置：
 *   每个 AbilityClass 属性需要在 BP_AbilitySystemComponent 中指定对应的
 *   Blueprint 子类（如 BP_GA_Fire）。若不配置则跳过该能力的授予。
 */
UCLASS()
class MYFPS_DEMO_API UBaseAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	// ================================================================
	//  武器引用（能力通过 ASC 获取武器数据）
	// ================================================================

	void SetCurrentWeapon(ABaseWeapon* InWeapon) { CurrentWeapon = InWeapon; }
	ABaseWeapon* GetCurrentWeapon() const { return CurrentWeapon; }

	// ================================================================
	//  能力授予（服务端调用）
	// ================================================================

	/** 授予基础能力：Fire + Reload */
	void GrantDefaultAbilities();

	/** 授予移动能力：Crouch + Sprint + Jump（仅 Player） */
	void GrantMovementAbilities();

	/** 授予被动能力：StaminaRegen → 立即激活（仅 Player） */
	void GrantPassiveAbilities();

	// ================================================================
	//  便捷取消方法
	// ================================================================

	/** 取消开火能力（换弹/切枪时调用） */
	void CancelFireAbility();

	/** 取消冲刺能力（下蹲/跳跃时调用） */
	void CancelSprintAbility();

	// ================================================================
	//  Handle 访问器
	// ================================================================

	const FGameplayAbilitySpecHandle& GetFireAbilityHandle() const { return FireAbilityHandle; }
	const FGameplayAbilitySpecHandle& GetReloadAbilityHandle() const { return ReloadAbilityHandle; }
	const FGameplayAbilitySpecHandle& GetCrouchAbilityHandle() const { return CrouchAbilityHandle; }
	const FGameplayAbilitySpecHandle& GetSprintAbilityHandle() const { return SprintAbilityHandle; }
	const FGameplayAbilitySpecHandle& GetJumpAbilityHandle() const { return JumpAbilityHandle; }
	const FGameplayAbilitySpecHandle& GetStaminaRegenAbilityHandle() const { return StaminaRegenAbilityHandle; }

protected:
	/** 当前装备的武器 — 能力通过 ASC 读取武器数据，不直接依赖角色 */
	UPROPERTY()
	TObjectPtr<ABaseWeapon> CurrentWeapon;

	// ================================================================
	//  能力蓝图类引用（EditDefaultsOnly → 在 BP_ASC 中配置）
	// ================================================================

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TSubclassOf<class UGameplayAbility> FireAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TSubclassOf<class UGameplayAbility> ReloadAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TSubclassOf<class UGameplayAbility> CrouchAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TSubclassOf<class UGameplayAbility> SprintAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TSubclassOf<class UGameplayAbility> JumpAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TSubclassOf<class UGameplayAbility> StaminaRegenAbilityClass;

	// ================================================================
	//  Handle 缓存（GiveAbility 返回的 Handle，用于后续激活/取消）
	// ================================================================

	FGameplayAbilitySpecHandle FireAbilityHandle;
	FGameplayAbilitySpecHandle ReloadAbilityHandle;
	FGameplayAbilitySpecHandle CrouchAbilityHandle;
	FGameplayAbilitySpecHandle SprintAbilityHandle;
	FGameplayAbilitySpecHandle JumpAbilityHandle;
	FGameplayAbilitySpecHandle StaminaRegenAbilityHandle;
};
