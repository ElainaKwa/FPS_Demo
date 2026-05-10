#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CrosshairSettingsTypes.generated.h"

USTRUCT(BlueprintType)
struct MYFPS_DEMO_API FCrosshairSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName CrosshairType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, float> FloatParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, FLinearColor> ColorParams;
};

UCLASS()
class MYFPS_DEMO_API UCrosshairSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FCrosshairSettings Settings;
};
