// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "BaseGameplayAbility.generated.h"

/**
 * 项目 GameplayAbility 基类 — 所有能力（Fire/Reload/Crouch/Sprint/Jump）的公共父类。
 *
 * 统一设置 InstancingPolicy = InstancedPerActor：
 *   - 每个角色持有一个能力实例（不是每次激活创建新的）
 *   - 适合需要维护状态的能力（如 bInputReleased、bIsReloading）
 *   - 避免频繁创建/销毁能力实例的开销
 *
 * 所有 GA 类继承此类而非直接继承 UGameplayAbility。
 */
UCLASS(Abstract)
class MYFPS_DEMO_API UBaseGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UBaseGameplayAbility();
};
