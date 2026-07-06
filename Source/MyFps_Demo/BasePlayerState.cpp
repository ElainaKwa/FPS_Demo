// Copyright Epic Games, Inc. All Rights Reserved.

#include "BasePlayerState.h"
#include "Net/UnrealNetwork.h"

ABasePlayerState::ABasePlayerState()
{
	bReplicates = true;
}

void ABasePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABasePlayerState, Kills);
	DOREPLIFETIME(ABasePlayerState, Deaths);
}

void ABasePlayerState::AddKill()
{
	if (!HasAuthority())
	{
		return;
	}

	Kills++;
	SetScore(GetScore() + 1.0f);            // 使用父类 APlayerState 的 Score

	UE_LOG(LogTemp, Warning, TEXT("[BasePlayerState] %s 击杀 +1 | Kills=%d | Score=%.0f"),
		*GetPlayerName(), Kills, GetScore());

	OnKillsUpdated.Broadcast(Kills);
	OnScoreUpdated.Broadcast(GetScore());
}

void ABasePlayerState::AddDeath()
{
	if (!HasAuthority())
	{
		return;
	}

	Deaths++;
}

void ABasePlayerState::OnRep_Kills()
{
	OnKillsUpdated.Broadcast(Kills);
}
