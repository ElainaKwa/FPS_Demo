#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BasePlayerController.generated.h"

class UBaseBulletCounterWidget;
class UBaseCrosshairWidget;
class UBaseDeathWidget;
class UBaseHealthBarWidget;
class UBaseStaminaBarWidget;
class UBaseScoreWidget;
class UCrosshairSettingsSubsystem;

/**
 * 项目 PlayerController — 管理本地玩家的所有 UI Widget。
 *
 * Widget 生命周期：
 *   BeginPlay → 从 DefaultGame.ini 读取 ClassPath → LoadClass → CreateWidget → AddToViewport
 *   死亡时   → ShowDeathScreen → 移除游戏 HUD，显示 DeathWidget
 *
 * Widget 配置方式：
 *   - 所有 WidgetClassPath 通过 DefaultGame.ini 配置（非硬编码）
 *   - 避免 GConfig 分层问题：直接 Read ConfigFile 而非 GConfig->GetString
 *   - 只在 IsLocalPlayerController 时创建（不对远程玩家创建 UI）
 */
UCLASS()
class MYFPS_DEMO_API ABasePlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	// ================================================================
	//  Widget 类路径（从 DefaultGame.ini 读取）
	// ================================================================

	UPROPERTY()
	FString BulletCounterWidgetClassPath;

	UPROPERTY()
	FString HealthBarWidgetClassPath;

	UPROPERTY()
	FString StaminaBarWidgetClassPath;

	UPROPERTY()
	FString ScoreWidgetClassPath;

	UPROPERTY()
	FString DeathWidgetClassPath;

	// ================================================================
	//  Widget 实例
	// ================================================================

	UPROPERTY()
	TObjectPtr<UBaseBulletCounterWidget> BulletCounterWidget;

	UPROPERTY()
	TObjectPtr<UBaseCrosshairWidget> CrosshairWidget;

	/** 死亡提示 Widget — BeginPlay 时创建但不添加，死亡时才 AddToViewport */
	UPROPERTY()
	TObjectPtr<UBaseDeathWidget> DeathWidget;

	UPROPERTY()
	TObjectPtr<UBaseHealthBarWidget> HealthBarWidget;

	UPROPERTY()
	TObjectPtr<UBaseStaminaBarWidget> StaminaBarWidget;

	UPROPERTY()
	TObjectPtr<UBaseScoreWidget> ScoreWidget;

public:
	/** 运行时切换准星类型（设置面板触发） */
	UFUNCTION(BlueprintCallable, Category = "Crosshair")
	void SetCrosshairType(FName TypeName);

	/**
	 * 显示死亡画面：移除所有游戏 HUD，显示 DeathWidget。
	 * 由 MulticastDeathVisuals 在本地玩家客户端调用。
	 */
	void ShowDeathScreen();
};
