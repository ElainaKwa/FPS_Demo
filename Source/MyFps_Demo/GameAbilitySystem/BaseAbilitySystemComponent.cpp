// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseAbilitySystemComponent.h"
#include "BaseGameplayTags.h"

// ================================================================
//  基础能力授予 — Fire + Reload（Player 和 Enemy 共用）
// ================================================================
void UBaseAbilitySystemComponent::GrantDefaultAbilities()
{
	/**
	 * GiveAbility 参数：
	 *   - Class：能力的 Blueprint 子类
	 *   - Level：能力等级（默认 1）
	 *   - InputID：映射到 EAbilityInputID 枚举（当前项目通过 Handle 激活，不依赖此 ID）
	 *   - SourceObject：能力的来源（此处为 ASC 自身）
	 */

	if (FireAbilityClass)
	{
		FireAbilityHandle = GiveAbility(FGameplayAbilitySpec(
			FireAbilityClass, 1,
			static_cast<int32>(EAbilityInputID::Fire),
			this));
	}

	if (ReloadAbilityClass)
	{
		ReloadAbilityHandle = GiveAbility(FGameplayAbilitySpec(
			ReloadAbilityClass, 1,
			static_cast<int32>(EAbilityInputID::Reload),
			this));
	}
}

// ================================================================
//  移动能力授予 — Crouch + Sprint + Jump（仅 Player）
// ================================================================
void UBaseAbilitySystemComponent::GrantMovementAbilities()
{
	if (CrouchAbilityClass)
	{
		CrouchAbilityHandle = GiveAbility(FGameplayAbilitySpec(
			CrouchAbilityClass, 1,
			static_cast<int32>(EAbilityInputID::Crouch),
			this));
	}

	if (SprintAbilityClass)
	{
		SprintAbilityHandle = GiveAbility(FGameplayAbilitySpec(
			SprintAbilityClass, 1,
			static_cast<int32>(EAbilityInputID::Sprint),
			this));
	}

	if (JumpAbilityClass)
	{
		JumpAbilityHandle = GiveAbility(FGameplayAbilitySpec(
			JumpAbilityClass, 1,
			static_cast<int32>(EAbilityInputID::Jump),
			this));
	}
}

// ================================================================
//  便捷取消 — 换弹/切枪/冲刺冲突时调用
// ================================================================

void UBaseAbilitySystemComponent::CancelFireAbility()
{
	// FindAbilitySpecFromHandle：从 Handle 查找对应的 AbilitySpec
	if (FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(FireAbilityHandle))
	{
		if (Spec->IsActive())
		{
			CancelAbilitySpec(*Spec, nullptr);
		}
	}
}

void UBaseAbilitySystemComponent::CancelSprintAbility()
{
	if (FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(SprintAbilityHandle))
	{
		if (Spec->IsActive())
		{
			CancelAbilitySpec(*Spec, nullptr);
		}
	}
}

// ================================================================
//  被动能力 — StaminaRegen（授予后立即激活，持续运行）
// ================================================================
void UBaseAbilitySystemComponent::GrantPassiveAbilities()
{
	if (StaminaRegenAbilityClass)
	{
		// InputID = -1：被动能力不需要输入
		StaminaRegenAbilityHandle = GiveAbility(FGameplayAbilitySpec(
			StaminaRegenAbilityClass, 1, -1, this));

		// 被动能力授予后立即激活（不需要等待输入）
		if (StaminaRegenAbilityHandle.IsValid())
		{
			TryActivateAbility(StaminaRegenAbilityHandle);
		}
	}
}
