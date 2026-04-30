// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyFpsGameMode.h"
#include "MyFpsCharacter.h"
#include "MyFpsPlayerController.h"
#include "UObject/ConstructorHelpers.h"

AMyFpsGameMode::AMyFpsGameMode()
{
	DefaultPawnClass = AMyFpsCharacter::StaticClass();
	PlayerControllerClass = AMyFpsPlayerController::StaticClass();
}
