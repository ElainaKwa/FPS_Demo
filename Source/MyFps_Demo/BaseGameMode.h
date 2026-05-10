// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UnLuaInterface.h"
#include "BaseGameMode.generated.h"

UCLASS()
class MYFPS_DEMO_API ABaseGameMode : public AGameModeBase, public IUnLuaInterface
{
	GENERATED_BODY()

public:

	ABaseGameMode();

	virtual FString GetModuleName_Implementation() const override;
};
