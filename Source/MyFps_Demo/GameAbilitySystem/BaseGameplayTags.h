// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

/**
 * 项目原生 GameplayTag 定义。
 *
 * 使用 NativeGameplayTags（而非 FGameplayTag 成员变量）的好处：
 *   - 编译期定义，避免编辑器 Tag 注册顺序问题
 *   - C++ 中可直接比较，无需从 Tag Manager 查询
 *   - 类型安全：BaseGameplayTags::State_Reloading 是编译期常量
 *
 * Tag 命名规范：
 *   Ability.X   — 能力标签（标记 GA）
 *   State.X     — 状态标签（阻塞/条件判断）
 *   Event.X     — 事件标签（跨能力通信）
 *   Data.X      — 数据标签（SetByCaller Magnitude）
 */
namespace BaseGameplayTags
{
	// ================================================================
	//  战斗能力
	// ================================================================

	/** 开火能力标签 */
	MYFPS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Fire);

	/** 换弹能力标签 */
	MYFPS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Reload);

	/** 正在开火中 */
	MYFPS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Firing);

	/** 正在换弹中 — 阻塞开火 + 阻塞自身重复激活 */
	MYFPS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Reloading);

	/** 弹药耗尽事件 — GA_WeaponFire 广播，ABaseCharacter 监听后自动触发换弹 */
	MYFPS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_OutOfAmmo);

	/** 死亡事件（已声明但当前使用 State_Dead 替代） */
	MYFPS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Death);

	/** 伤害数值标签 — GE_Damage 的 SetByCaller Magnitude 键 */
	MYFPS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);

	// ================================================================
	//  死亡状态 — 全局阻塞
	// ================================================================

	/** 已死亡 — 阻塞所有能力激活 + 输入回调前置守卫 */
	MYFPS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);

	// ================================================================
	//  移动能力
	// ================================================================

	MYFPS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Crouch);
	MYFPS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Sprint);
	MYFPS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Jump);

	/** 下蹲中 — 阻塞冲刺 + 自身重复激活 */
	MYFPS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Crouching);

	/** 冲刺中 — 阻塞下蹲/换弹 + 自身重复激活 */
	MYFPS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Sprinting);

	/** 空中 — 阻塞下蹲/冲刺 */
	MYFPS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Airborne);

	// ================================================================
	//  体力系统
	// ================================================================

	/** 体力被消耗事件 — 通知体力回复能力进入 CD */
	MYFPS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_StaminaConsumed);

	/** 体力回复能力处于冷却中 */
	MYFPS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_StaminaRegen_CD);
};
