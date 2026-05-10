#include "BasePlayerController.h"
#include "UI/BulletCounter/BaseBulletCounterWidget.h"
#include "UI/Crosshair/BaseCrosshairWidget.h"
#include "UI/Crosshair/CrosshairSettingsSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Misc/ConfigCacheIni.h"

void ABasePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Read widget class path directly from DefaultGame.ini (bypasses GConfig layering)
	if (BulletCounterWidgetClassPath.IsEmpty())
	{
		FConfigFile ConfigFile;
		ConfigFile.Read(*(FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini")));
		ConfigFile.GetString(TEXT("/Script/MyFps_Demo.ABasePlayerController"),
			TEXT("BulletCounterWidgetClassPath"), BulletCounterWidgetClassPath);
	}

	if (!BulletCounterWidgetClassPath.IsEmpty())
	{
		UClass* Class = LoadClass<UBaseBulletCounterWidget>(nullptr, *BulletCounterWidgetClassPath);
		if (Class)
		{
			BulletCounterWidget = CreateWidget<UBaseBulletCounterWidget>(this, Class);
			if (BulletCounterWidget)
			{
				BulletCounterWidget->AddToViewport();
				UE_LOG(LogTemp, Warning, TEXT("BasePlayerController: BulletCounterWidget added to viewport"));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("BasePlayerController: BulletCounterWidgetClassPath is not configured"));
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UCrosshairSettingsSubsystem* Subsystem = GI->GetSubsystem<UCrosshairSettingsSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("BasePlayerController: CrosshairSettingsSubsystem not found"));
		return;
	}

	TSubclassOf<UBaseCrosshairWidget> CrosshairClass = Subsystem->GetCurrentCrosshairClass();
	if (!CrosshairClass)
	{
		UE_LOG(LogTemp, Error, TEXT("BasePlayerController: No crosshair class registered for type [%s]"),
			*Subsystem->GetCurrentCrosshairType().ToString());
		return;
	}

	CrosshairWidget = CreateWidget<UBaseCrosshairWidget>(this, CrosshairClass);
	if (CrosshairWidget)
	{
		CrosshairWidget->AddToViewport(-1);
		Subsystem->ApplyToWidget(CrosshairWidget);
		UE_LOG(LogTemp, Warning, TEXT("BasePlayerController: CrosshairWidget [%s] added to viewport"),
			*Subsystem->GetCurrentCrosshairType().ToString());
	}
}

void ABasePlayerController::SetCrosshairType(FName TypeName)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UCrosshairSettingsSubsystem* Subsystem = GI->GetSubsystem<UCrosshairSettingsSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	if (CrosshairWidget)
	{
		Subsystem->GatherFromWidget(CrosshairWidget);
		CrosshairWidget->RemoveFromParent();
		CrosshairWidget = nullptr;
	}

	Subsystem->SetCrosshairType(TypeName);

	TSubclassOf<UBaseCrosshairWidget> NewClass = Subsystem->GetCurrentCrosshairClass();
	if (!NewClass)
	{
		return;
	}

	CrosshairWidget = CreateWidget<UBaseCrosshairWidget>(this, NewClass);
	if (CrosshairWidget)
	{
		CrosshairWidget->AddToViewport(-1);
		Subsystem->ApplyToWidget(CrosshairWidget);
	}
}
