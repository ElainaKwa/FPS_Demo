// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseGameMode.h"
#include "BaseCharacter.h"
#include "BasePlayerController.h"
#include "UObject/ConstructorHelpers.h"

ABaseGameMode::ABaseGameMode()
{
	DefaultPawnClass = ABaseCharacter::StaticClass();
	PlayerControllerClass = ABasePlayerController::StaticClass();
}
