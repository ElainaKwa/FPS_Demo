#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BasePlayerController.generated.h"

class UBaseBulletCounterWidget;

UCLASS()
class MYFPS_DEMO_API ABasePlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	UPROPERTY()
	TObjectPtr<UBaseBulletCounterWidget> BulletCounterWidget;
};
