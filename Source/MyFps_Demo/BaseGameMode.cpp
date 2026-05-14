// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseGameMode.h"
#include "PlayerCharacter.h"
#include "BasePlayerController.h"
#include "UObject/ConstructorHelpers.h"

ABaseGameMode::ABaseGameMode()
{
	DefaultPawnClass = APlayerCharacter::StaticClass();
	PlayerControllerClass = ABasePlayerController::StaticClass();
}

FString ABaseGameMode::GetModuleName_Implementation() const
{
	return TEXT("BaseGameMode");
}
