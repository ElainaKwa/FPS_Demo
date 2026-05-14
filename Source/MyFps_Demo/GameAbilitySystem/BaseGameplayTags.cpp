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
}
