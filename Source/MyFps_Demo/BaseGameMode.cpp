// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseGameMode.h"
#include "PlayerCharacter.h"
#include "BasePlayerController.h"
#include "BasePlayerState.h"
#include "BaseCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/PlayerController.h"

ABaseGameMode::ABaseGameMode()
{
	DefaultPawnClass = APlayerCharacter::StaticClass();
	PlayerControllerClass = ABasePlayerController::StaticClass();
	PlayerStateClass = ABasePlayerState::StaticClass();
}

FString ABaseGameMode::GetModuleName_Implementation() const
{
	return TEXT("BaseGameMode");
}

void ABaseGameMode::OnKill(AActor* Killer, ABaseCharacter* Victim)
{
	if (!HasAuthority())
	{
		return;
	}

	// 从 Killer (Pawn) 获取 Controller
	AController* KillerController = nullptr;
	if (Killer)
	{
		if (APawn* KillerPawn = Cast<APawn>(Killer))
		{
			KillerController = KillerPawn->GetController();
		}
		else
		{
			KillerController = Cast<AController>(Killer);
		}
	}

	// 击杀者加分（仅玩家）
	if (APlayerController* KillerPC = Cast<APlayerController>(KillerController))
	{
		if (ABasePlayerState* PS = KillerPC->GetPlayerState<ABasePlayerState>())
		{
			PS->AddKill();
		}
	}

	// 被杀者记录死亡（仅玩家）
	if (ABasePlayerState* VictimPS = Victim->GetPlayerState<ABasePlayerState>())
	{
		VictimPS->AddDeath();
	}
}
