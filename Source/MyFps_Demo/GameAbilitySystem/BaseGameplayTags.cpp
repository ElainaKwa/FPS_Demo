// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseGameplayTags.h"

/**
 * UE_DEFINE_GAMEPLAY_TAG_COMMENT 宏：
 *   参数1：C++ 变量名（如 Ability_Fire）
 *   参数2：Tag 字符串（如 "Ability.Fire"）
 *   参数3：编辑器中的注释/描述
 *
 * 这些 Tag 在引擎启动时注册到 GameplayTagManager，无需在编辑器中手动创建。
 * C++ 中直接用 BaseGameplayTags::Ability_Fire 比较，运行时高效。
 */
namespace BaseGameplayTags
{
	// ---- 战斗 ----
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Fire, "Ability.Fire", "Tag for the fire ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Reload, "Ability.Reload", "Tag for the reload ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Firing, "State.Firing", "Actor is currently firing a weapon");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Reloading, "State.Reloading", "Actor is currently reloading");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_OutOfAmmo, "Event.OutOfAmmo", "Broadcast when ammo reaches zero");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Death, "Event.Death", "Broadcast when health reaches zero");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage, "Data.Damage", "SetByCaller magnitude tag for damage");

	// ---- 死亡 ----
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Actor is dead");

	// ---- 移动 ----
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Crouch, "Ability.Crouch", "Tag for the crouch ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Sprint, "Ability.Sprint", "Tag for the sprint ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Jump, "Ability.Jump", "Tag for the jump ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Crouching, "State.Crouching", "Actor is crouching");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Sprinting, "State.Sprinting", "Actor is sprinting");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Airborne, "State.Airborne", "Actor is in the air (jumping/falling)");

	// ---- 体力 ----
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_StaminaConsumed, "Event.StaminaConsumed", "Broadcast when stamina is consumed");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_StaminaRegen_CD, "Ability.StaminaRegen.CD", "Stamina regen ability is on cooldown");
}
