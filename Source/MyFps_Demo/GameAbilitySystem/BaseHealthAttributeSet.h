// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "BaseHealthAttributeSet.generated.h"

/**
 * 生命值属性集 — 所有角色的血量管理。
 *
 * 属性：
 *   Health / MaxHealth  — 当前血量 / 最大血量（网络复制）
 *   IncomingDamage      — 临时属性，接收 GE 传入的伤害值（不复制）
 *   IncomingHeal        — 临时属性，接收 GE 传入的治疗值（不复制）
 *
 * 模式：GE 修改 IncomingDamage → PostGameplayEffectExecute 计算实际扣血 → 钳制到 [0, MaxHealth]
 *
 * Health <= 0 时触发：
 *   1. Character->OnDeath()    — 死亡处理
 *   2. GameMode->OnKill()      — 计分
 */
UCLASS()
class MYFPS_DEMO_API UBaseHealthAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UBaseHealthAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** GE 执行后的回调 — 在此处计算实际伤害/治疗 + 死亡检测 */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// ================================================================
	//  属性定义
	// ================================================================

	/** 当前血量 — 网络复制，OnRep 触发 HUD 更新 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes|Health")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UBaseHealthAttributeSet, Health)

	/** 最大血量 — 网络复制 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes|Health")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UBaseHealthAttributeSet, MaxHealth)

	/**
	 * 接收伤害 — 临时属性（不复制）。
	 * GE_Damage 通过 SetByCaller(Data.Damage) 设置此值，
	 * PostGameplayEffectExecute 读取后立即清零，计算 Health -= IncomingDamage。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Health")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS_BASIC(UBaseHealthAttributeSet, IncomingDamage)

	/** 接收治疗 — 同上但用于回血 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Health")
	FGameplayAttributeData IncomingHeal;
	ATTRIBUTE_ACCESSORS_BASIC(UBaseHealthAttributeSet, IncomingHeal)

	// ================================================================
	//  复制回调
	// ================================================================

	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
};
