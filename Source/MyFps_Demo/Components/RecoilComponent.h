// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RecoilComponent.generated.h"

/**
 * 后坐力执行器 — 挂在 Player 身上的 ActorComponent。
 *
 * 职责：
 *   累加射击产生的后坐力目标值 → 在 Tick 中逐帧 FInterpTo 平滑 → 输出 Pitch 到控制器。
 *
 * 为什么是纯执行器（零配置）？
 *   - 所有参数（InterpSpeed / RecoverySpeed / MaxAccumulation）由武器侧传入
 *   - 不同武器可拥有完全不同的后坐力手感曲线
 *   - 配件通过倍率修正这些参数
 *
 * 平滑策略：
 *   - 上升阶段：TargetPitch 累加 → CurrentPitch 以 InterpSpeed 追赶
 *   - 恢复阶段：TargetPitch 自动衰减到 0 → CurrentPitch 以 RecoverySpeed 回落
 *   - 只应用帧间差值 AddControllerPitchInput(-Delta)，实现平滑过渡
 *   - 负号：UE 中正 Pitch = 低头，取反 = 枪口上跳
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MYFPS_DEMO_API URecoilComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URecoilComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * 累加后坐力 — 每次击发调用一次。
	 * @param Amount             单发后坐力幅度（Weapon->GetEffectiveRecoil()）
	 * @param InInterpSpeed      上升追赶速度
	 * @param InRecoverySpeed    恢复速度
	 * @param InMaxAccumulation  连射累积上限
	 */
	void AddRecoil(float Amount, float InInterpSpeed, float InRecoverySpeed, float InMaxAccumulation);

private:
	/** 目标后坐力值 — 击发时累加，Tick 中自动衰减到 0 */
	float TargetPitch = 0.0f;

	/** 当前已平滑应用的后坐力值 — Tick 中的插值起点 */
	float CurrentPitch = 0.0f;

	/** 运行时参数 — 每次 AddRecoil 时由武器侧更新（切换武器时参数变化） */
	float InterpSpeed = 15.0f;
	float RecoverySpeed = 5.0f;
	float MaxAccumulation = 20.0f;
};
