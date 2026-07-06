// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "BasePlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScoreUpdated, float, NewScore);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnKillsUpdated, int32, NewKills);

UCLASS(Blueprintable)
class MYFPS_DEMO_API ABasePlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ABasePlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnScoreUpdated OnScoreUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnKillsUpdated OnKillsUpdated;

	// APlayerState 已自带 float Score 和 OnRep_Score，这里只加 Kills/Deaths
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Score")
	int32 Kills = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Score")
	int32 Deaths = 0;

	UFUNCTION(BlueprintCallable, Category = "Score")
	void AddKill();

	UFUNCTION(BlueprintCallable, Category = "Score")
	void AddDeath();

protected:
	UFUNCTION()
	void OnRep_Kills();
};
