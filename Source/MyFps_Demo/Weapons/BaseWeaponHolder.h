// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BaseWeaponHolder.generated.h"

class ABaseWeapon;
class UAnimMontage;

UINTERFACE(MinimalAPI)
class UBaseWeaponHolder : public UInterface
{
	GENERATED_BODY()
};

/**
 * 武器持有者接口 — 角色与武器之间的所有通信都通过此接口。
 *
 * 设计目的：
 *   - 解耦：Ability 不依赖具体的 APlayerCharacter / ABaseEnemy 类型
 *   - 多态：Player 和 Enemy 各自实现接口，行为不同
 *     - AttachWeaponMeshes：Player 挂 FP+TP，Enemy 只挂 TP
 *     - PlayFiringMontage：Player Multicast 双网格，Enemy 仅 TP
 *     - AddWeaponRecoil：Player 委托 RecoilComponent，Enemy 空
 *   - 扩展：未来载具/AI 同伴只需实现本接口即可持有武器
 *
 * 纯虚函数（= 0）：必须实现
 *   - GetWeaponTargetLocation  — 瞄准方向（FPS 从摄像机，AI 从眼睛高度）
 *   - AttachWeaponMeshes        — 武器网格挂载
 *   - GetCurrentWeapon          — 获取当前装备的武器
 *
 * 虚函数（有默认空实现）：可选
 *   - PlayFiringMontage / PlayReloadMontage — 动画播放
 *   - AddWeaponRecoil                      — 后坐力
 *   - UpdateWeaponHUD                     — UI 刷新
 */
class MYFPS_DEMO_API IBaseWeaponHolder
{
	GENERATED_BODY()

public:

	/** 挂载武器的 FP + TP 网格到角色骨骼上（必须实现） */
	virtual void AttachWeaponMeshes(ABaseWeapon* Weapon) = 0;

	/** 获取武器瞄准的目标位置（必须实现）— Player 从摄像机，Enemy 从眼睛高度 */
	virtual FVector GetWeaponTargetLocation() const = 0;

	/** 播放开火蒙太奇（可选）— 默认仅在 TP 网格上播放 */
	virtual void PlayFiringMontage(UAnimMontage* Montage) {}

	/** 播放换弹蒙太奇（可选） */
	virtual void PlayReloadMontage(UAnimMontage* Montage) {}

	/**
	 * 施加后坐力（可选）。
	 * 四个参数从武器侧 GetEffective*() 读取，由 RecoilComponent 平滑执行。
	 */
	virtual void AddWeaponRecoil(float RecoilAmount, float InterpSpeed, float RecoverySpeed, float MaxAccumulation) {}

	/** 刷新武器 HUD（弹药数变化时回调） */
	virtual void UpdateWeaponHUD(int32 CurrentAmmo, int32 MaxAmmo) {}

	/** 获取当前装备的武器（必须实现） */
	virtual ABaseWeapon* GetCurrentWeapon() const = 0;
};
