// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "BaseWeaponAttributeSet.generated.h"

/**
 * 弹药属性集 — 网络复制的武器弹药数据。
 *
 * 属性：
 *   CurrentAmmo — 当前弹匣内子弹数（钳制 [0, MaxAmmo]）
 *   MaxAmmo     — 最大弹匣容量（最低 1.0）
 *
 * 同步策略：
 *   - 开火/换弹时 GA 通过 GE 修改属性（自动复制）
 *   - 同时手动同步 Weapon.CurrentBullets ← AttributeSet（双保险）
 *   - Weapon.CurrentBullets 也有独立的 ReplicatedUsing 复制通道
 *
 * 注意：弹药消耗先扣 Weapon.CurrentBullets，GE 作为附加。
 *   这是为了兼容 GE 未配置时的兜底情况。
 */
UCLASS()
class MYFPS_DEMO_API UBaseWeaponAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UBaseWeaponAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** GE 执行后钳制弹药范围 [0, MaxAmmo] */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentAmmo, Category = "Ammo")
	FGameplayAttributeData CurrentAmmo;
	ATTRIBUTE_ACCESSORS_BASIC(UBaseWeaponAttributeSet, CurrentAmmo)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxAmmo, Category = "Ammo")
	FGameplayAttributeData MaxAmmo;
	ATTRIBUTE_ACCESSORS_BASIC(UBaseWeaponAttributeSet, MaxAmmo)

	UFUNCTION()
	virtual void OnRep_CurrentAmmo(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_MaxAmmo(const FGameplayAttributeData& OldValue);
};
