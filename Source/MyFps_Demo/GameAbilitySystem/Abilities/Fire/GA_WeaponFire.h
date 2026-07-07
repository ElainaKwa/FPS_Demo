// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseGameplayAbility.h"
#include "GA_WeaponFire.generated.h"

/**
 * 开火能力 — 驱动所有武器的射击行为。
 *
 * 核心策略：
 *   - InstancedPerActor：复用实例，维护 bInputReleased 状态
 *   - ServerOnly：只在服务端执行逻辑，客户端通过 Server RPC 触发
 *   - 全自动循环：PerformFire → WaitDelay(RefireRate) → OnRefireReady → 循环
 *   - 半自动：PerformFire → WaitDelay → 检测 bInputReleased → EndAbility
 *
 * 弹药消耗顺序（重要）：
 *   1. Weapon->CurrentBullets -= 1（先扣，兜底）
 *   2. GE_AmmoCost → 修改 AttributeSet（附加）
 *   3. SetNumericAttributeBase → 同步 AttributeSet ← Weapon
 *
 * 阻塞条件：
 *   - State_Reloading：换弹中不可开火（ActivateAbility 手动检查）
 *   - 弹药耗尽：广播 Event_OutOfAmmo → 自动触发换弹
 */
UCLASS()
class MYFPS_DEMO_API UGA_WeaponFire : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_WeaponFire();

	// ================================================================
	//  GAS 生命周期
	// ================================================================

	/**
	 * 激活入口 — 服务端调用。
	 * 流程：Authority 守卫 → State_Reloading 检查 → CommitAbility → WaitInputRelease → PerformFire
	 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** 结束能力 — 重置 bIsFiring 状态 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// ================================================================
	//  蓝图配置（在 BP_GA_Fire 中指定）
	// ================================================================

	/** 伤害 GE — SetByCaller(Data.Damage) 传递伤害值到目标 ASC */
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Effects")
	TSubclassOf<class UGameplayEffect> DamageEffectClass;

	/** 弹药消耗 GE — 每次击发扣 1 发弹药 */
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Effects")
	TSubclassOf<class UGameplayEffect> AmmoCostEffectClass;

	// ================================================================
	//  运行时状态
	// ================================================================

	/** 是否已松开开火键 — 由 WaitInputRelease Task 设置，全自动循环的退出条件 */
	bool bInputReleased = false;

	/** WaitInputRelease 的回调 — 设置 bInputReleased = true */
	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	// ================================================================
	//  射击逻辑
	// ================================================================

	/**
	 * 执行一次射击。
	 * 流程：
	 *   弹药检查 → 枪口位置/方向/散布计算 → LineTrace → 命中伤害处理
	 *   → 蒙太奇 + 后坐力 → 弹药消耗（Weapon → GE → AttributeSet 同步）
	 *   → 弹药耗尽检查 → 松开检查 → 全自动延迟循环 / 半自动结束
	 */
	void PerformFire();

	/** RefireRate 延迟后的回调 → 全自动继续 / 半自动结束 */
	UFUNCTION()
	void OnRefireReady();

	/** 从 ASC 获取当前武器（不依赖角色具体类型） */
	class ABaseWeapon* GetWeapon() const;
};
