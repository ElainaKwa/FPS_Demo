// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseGameplayAbility.h"
#include "GA_WeaponReload.generated.h"

/**
 * 换弹能力 — 管理换弹计时 + 弹药补充。
 *
 * 激活方式：
 *   1. 玩家按 Reload 键 → ServerReload RPC → TryActivateAbility
 *   2. Event_OutOfAmmo 自动触发（AbilityTriggers 配置）
 *
 * 并发计时 Task（三个独立计时器）：
 *   DropMagazine  → MagazineDropDelay 秒后 → OnDropMagazine   → 弹匣掉落
 *   InsertMagazine → 结束前 InsertBeforeEnd 秒 → OnInsertMagazine → 弹匣插入
 *   ReloadComplete → EffectiveTime 秒后 → OnReloadComplete → 补充弹药 + 结束
 *
 * 阻塞：
 *   State_Reloading 标签阻塞开火 + 自身重复激活（CanActivateAbility 检查）
 *   弹药已满时拒绝换弹（CurrentBullets >= MagazineSize）
 */
UCLASS()
class MYFPS_DEMO_API UGA_WeaponReload : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_WeaponReload();

	// ================================================================
	//  GAS 生命周期
	// ================================================================

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 前置检查：State_Reloading 阻塞自身重复激活 */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	// ================================================================
	//  蓝图配置
	// ================================================================

	/** 补充弹药 GE — 将 CurrentAmmo 设为 MagazineSize */
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Effects")
	TSubclassOf<class UGameplayEffect> ReloadAmmoEffectClass;

	// ================================================================
	//  计时回调（由 WaitDelay Task 触发）
	// ================================================================

	/** 弹匣掉落：生成物理弹匣 Actor + 左手显示替换弹匣 */
	UFUNCTION()
	void OnDropMagazine();

	/** 弹匣插入：隐藏左手弹匣 + 恢复武器弹匣槽 */
	UFUNCTION()
	void OnInsertMagazine();

	/** 换弹完成：补充弹药 → 同步 Weapon → 刷新 HUD → EndAbility */
	UFUNCTION()
	void OnReloadComplete();

	class ABaseWeapon* GetWeapon() const;
};
