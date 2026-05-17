// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseGameplayTags.h"

namespace BaseGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Fire, "Ability.Fire", "Tag for the fire ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Reload, "Ability.Reload", "Tag for the reload ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Firing, "State.Firing", "Actor is currently firing a weapon");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Reloading, "State.Reloading", "Actor is currently reloading");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_OutOfAmmo, "Event.OutOfAmmo", "Broadcast when ammo reaches zero");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Death, "Event.Death", "Broadcast when health reaches zero");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage, "Data.Damage", "SetByCaller magnitude tag for damage");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Actor is dead");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Crouch, "Ability.Crouch", "Tag for the crouch ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Sprint, "Ability.Sprint", "Tag for the sprint ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Jump, "Ability.Jump", "Tag for the jump ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Crouching, "State.Crouching", "Actor is crouching");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Sprinting, "State.Sprinting", "Actor is sprinting");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Airborne, "State.Airborne", "Actor is in the air (jumping/falling)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_StaminaConsumed, "Event.StaminaConsumed", "Broadcast when stamina is consumed");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_StaminaRegen_CD, "Ability.StaminaRegen.CD", "Stamina regen ability is on cooldown");
}
