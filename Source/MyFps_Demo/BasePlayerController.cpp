#include "BasePlayerController.h"
#include "UI/BulletCounter/BaseBulletCounterWidget.h"
#include "UI/Crosshair/BaseCrosshairWidget.h"
#include "UI/Crosshair/CrosshairSettingsSubsystem.h"
#include "UI/Death/BaseDeathWidget.h"
#include "UI/Health/BaseHealthBarWidget.h"
#include "UI/Stamina/BaseStaminaBarWidget.h"
#include "UI/Score/BaseScoreWidget.h"
#include "Blueprint/UserWidget.h"
#include "Misc/ConfigCacheIni.h"

// ================================================================
//  BeginPlay — 创建所有 UI Widget（仅本地玩家）
// ================================================================
void ABasePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 只为本地玩家创建 UI（远程玩家的 Controller 不需要 Widget）
	if (!IsLocalPlayerController())
	{
		return;
	}

	/**
	 * Widget 加载模式：
	 *   1. 从 DefaultGame.ini 读取蓝图类路径
	 *   2. LoadClass 运行时加载
	 *   3. CreateWidget 创建实例
	 *   4. AddToViewport 显示
	 *
	 * 注意：直接 Read ConfigFile 而非 GConfig->GetString，
	 *   因为 GConfig 的分层覆盖逻辑在某些情况下不生效。
	 */

	// ---- 弹药计数器 ----
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

	// ---- 血条 ----
	if (HealthBarWidgetClassPath.IsEmpty())
	{
		FConfigFile ConfigFile;
		ConfigFile.Read(*(FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini")));
		ConfigFile.GetString(TEXT("/Script/MyFps_Demo.ABasePlayerController"),
			TEXT("HealthBarWidgetClassPath"), HealthBarWidgetClassPath);
	}

	if (!HealthBarWidgetClassPath.IsEmpty())
	{
		UClass* Class = LoadClass<UBaseHealthBarWidget>(nullptr, *HealthBarWidgetClassPath);
		if (Class)
		{
			HealthBarWidget = CreateWidget<UBaseHealthBarWidget>(this, Class);
			if (HealthBarWidget)
			{
				HealthBarWidget->AddToViewport();
			}
		}
	}

	// ---- 体力条 ----
	if (StaminaBarWidgetClassPath.IsEmpty())
	{
		FConfigFile ConfigFile;
		ConfigFile.Read(*(FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini")));
		ConfigFile.GetString(TEXT("/Script/MyFps_Demo.ABasePlayerController"),
			TEXT("StaminaBarWidgetClassPath"), StaminaBarWidgetClassPath);
	}

	if (!StaminaBarWidgetClassPath.IsEmpty())
	{
		UClass* Class = LoadClass<UBaseStaminaBarWidget>(nullptr, *StaminaBarWidgetClassPath);
		if (Class)
		{
			StaminaBarWidget = CreateWidget<UBaseStaminaBarWidget>(this, Class);
			if (StaminaBarWidget)
			{
				StaminaBarWidget->AddToViewport();
			}
		}
	}

	// ---- 分数 ----
	if (ScoreWidgetClassPath.IsEmpty())
	{
		FConfigFile ConfigFile;
		ConfigFile.Read(*(FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini")));
		ConfigFile.GetString(TEXT("/Script/MyFps_Demo.ABasePlayerController"),
			TEXT("ScoreWidgetClassPath"), ScoreWidgetClassPath);
	}

	if (!ScoreWidgetClassPath.IsEmpty())
	{
		UClass* Class = LoadClass<UBaseScoreWidget>(nullptr, *ScoreWidgetClassPath);
		if (Class)
		{
			ScoreWidget = CreateWidget<UBaseScoreWidget>(this, Class);
			if (ScoreWidget)
			{
				ScoreWidget->AddToViewport();
				UE_LOG(LogTemp, Warning, TEXT("BasePlayerController: ScoreWidget added to viewport"));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("BasePlayerController: ScoreWidgetClassPath is not configured"));
	}

	// ---- 死亡提示（创建但不添加到 Viewport）----
	if (DeathWidgetClassPath.IsEmpty())
	{
		FConfigFile ConfigFile;
		ConfigFile.Read(*(FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini")));
		ConfigFile.GetString(TEXT("/Script/MyFps_Demo.ABasePlayerController"),
			TEXT("DeathWidgetClassPath"), DeathWidgetClassPath);
	}

	if (!DeathWidgetClassPath.IsEmpty())
	{
		UClass* Class = LoadClass<UBaseDeathWidget>(nullptr, *DeathWidgetClassPath);
		if (Class)
		{
			// 只创建，不添加到 Viewport — 死亡时才 ShowDeathScreen
			DeathWidget = CreateWidget<UBaseDeathWidget>(this, Class);
		}
	}

	// ---- 准星（通过 CrosshairSettingsSubsystem 获取当前选中类型）----
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
		// 优先级 -1：在所有 UI 下层（准星在 HUD 之下）
		CrosshairWidget->AddToViewport(-1);
		Subsystem->ApplyToWidget(CrosshairWidget);
		UE_LOG(LogTemp, Warning, TEXT("BasePlayerController: CrosshairWidget [%s] added to viewport"),
			*Subsystem->GetCurrentCrosshairType().ToString());
	}
}

// ================================================================
//  SetCrosshairType — 运行时切换准星（设置面板触发）
// ================================================================
void ABasePlayerController::SetCrosshairType(FName TypeName)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	UCrosshairSettingsSubsystem* Subsystem = GI->GetSubsystem<UCrosshairSettingsSubsystem>();
	if (!Subsystem) return;

	// 保存当前准星设置 → 移除旧 Widget
	if (CrosshairWidget)
	{
		Subsystem->GatherFromWidget(CrosshairWidget);
		CrosshairWidget->RemoveFromParent();
		CrosshairWidget = nullptr;
	}

	// 切换到新类型
	Subsystem->SetCrosshairType(TypeName);

	TSubclassOf<UBaseCrosshairWidget> NewClass = Subsystem->GetCurrentCrosshairClass();
	if (!NewClass) return;

	// 创建并应用新准星
	CrosshairWidget = CreateWidget<UBaseCrosshairWidget>(this, NewClass);
	if (CrosshairWidget)
	{
		CrosshairWidget->AddToViewport(-1);
		Subsystem->ApplyToWidget(CrosshairWidget);
	}
}

// ================================================================
//  ShowDeathScreen — 死亡时调用（本地玩家）
// ================================================================
void ABasePlayerController::ShowDeathScreen()
{
	// 移除所有游戏 HUD
	if (CrosshairWidget)
	{
		CrosshairWidget->RemoveFromParent();
	}
	if (BulletCounterWidget)
	{
		BulletCounterWidget->RemoveFromParent();
	}
	if (HealthBarWidget)
	{
		HealthBarWidget->RemoveFromParent();
	}
	if (StaminaBarWidget)
	{
		StaminaBarWidget->RemoveFromParent();
	}
	if (ScoreWidget)
	{
		ScoreWidget->RemoveFromParent();
	}

	// 显示死亡 overlay（优先级 10，覆盖在所有 UI 之上）
	if (DeathWidget && !DeathWidget->IsInViewport())
	{
		DeathWidget->AddToViewport(10);
	}
}
