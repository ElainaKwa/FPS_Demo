#include "BasePlayerController.h"
#include "UI/BaseBulletCounterWidget.h"
#include "Blueprint/UserWidget.h"

void ABasePlayerController::BeginPlay()
{
	Super::BeginPlay();

	UClass* WidgetClass = LoadClass<UBaseBulletCounterWidget>(nullptr, TEXT("/Game/FPSContent/Blueprint/UI/UMG_BulletCounter.UMG_BulletCounter_C"));
	if (WidgetClass)
	{
		BulletCounterWidget = CreateWidget<UBaseBulletCounterWidget>(this, WidgetClass);
		if (BulletCounterWidget)
		{
			BulletCounterWidget->AddToViewport();
			UE_LOG(LogTemp, Warning, TEXT("BasePlayerController: BulletCounterWidget added to viewport"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("BasePlayerController: Failed to find UMG_BulletCounter blueprint"));
	}
}
