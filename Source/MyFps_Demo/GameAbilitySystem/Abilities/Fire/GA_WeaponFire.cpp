// Copyright Epic Games, Inc. All Rights Reserved.

#include "GA_WeaponFire.h"
#include "BaseAbilitySystemComponent.h"
#include "BaseWeaponAttributeSet.h"
#include "BaseGameplayTags.h"
#include "BaseWeapon.h"
#include "BaseCharacter.h"
#include "BaseWeaponHolder.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

// ================================================================
//  构造 — 设置能力策略 + AssetTag
// ================================================================
UGA_WeaponFire::UGA_WeaponFire()
{
	// 每个角色一个实例（需要维护 bInputReleased 状态）
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 只在服务端执行逻辑（客户端通过 Server RPC 触发）
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	// SetAssetTags（非 AbilityTags）— UE 5.7 推荐方式
	SetAssetTags(FGameplayTagContainer(BaseGameplayTags::Ability_Fire));
}

// ================================================================
//  激活 — 服务端入口
// ================================================================
void UGA_WeaponFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// ----- 守卫：即使设了 ServerOnly，BP 子类可能覆盖 → 服务端双重校验 -----
	if (!K2_HasAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ----- 守卫：换弹中不可开火（C++ 手动检查，替代 ActivationBlockedTags）-----
	// 原因：BP CDO 会覆盖 C++ 构造函数中设置的 ActivationBlockedTags
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Reloading))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}

	// ----- CommitAbility：检查消耗/冷却/阻塞 -----
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ----- 标记开火状态 -----
	if (ABaseWeapon* Weapon = GetWeapon())
	{
		Weapon->bIsFiring = true;
	}

	// ----- 重置释放标志 + 启动输入监听 Task -----
	bInputReleased = false;

	// WaitInputRelease：监听 ASC->AbilityLocalInputReleased，按键松开时自动回调
	UAbilityTask_WaitInputRelease* WaitRelease = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
	WaitRelease->OnRelease.AddDynamic(this, &UGA_WeaponFire::OnInputReleased);
	WaitRelease->ReadyForActivation();

	// ----- 执行第一次射击 -----
	PerformFire();
}

// ================================================================
//  结束 — 清理开火状态
// ================================================================
void UGA_WeaponFire::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ABaseWeapon* Weapon = GetWeapon())
	{
		Weapon->bIsFiring = false;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_WeaponFire::OnInputReleased(float TimeHeld)
{
	bInputReleased = true;
}

// ================================================================
//  PerformFire — 一次射击的完整流程
// ================================================================
void UGA_WeaponFire::PerformFire()
{
	// ----- ① 获取武器 & ASC & AttributeSet -----
	ABaseWeapon* Weapon = GetWeapon();
	if (!Weapon)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const UBaseWeaponAttributeSet* AttrSet = ASC
		? Cast<UBaseWeaponAttributeSet>(ASC->GetAttributeSet(UBaseWeaponAttributeSet::StaticClass()))
		: nullptr;

	// ----- ② 弹药检查：同时检查 AttributeSet 和 Weapon 双保险 -----
	const bool bOutOfAmmo = (AttrSet && AttrSet->GetCurrentAmmo() <= 0.0f) || Weapon->CurrentBullets <= 0;
	if (bOutOfAmmo)
	{
		// 广播事件 → ABaseCharacter 注册的监听器自动触发换弹
		FGameplayEventData EventData;
		EventData.EventTag = BaseGameplayTags::Event_OutOfAmmo;
		ASC->HandleGameplayEvent(BaseGameplayTags::Event_OutOfAmmo, &EventData);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// ----- ③ 弹道计算 -----
	const FVector MuzzleLoc = Weapon->GetMuzzleLocation();
	const FVector TargetLocation = Weapon->WeaponOwner->GetWeaponTargetLocation();
	const FVector AimDir = (TargetLocation - MuzzleLoc).GetSafeNormal();
	// 散布：在瞄准方向圆锥内随机偏移
	const FVector SpreadDir = Weapon->CalculateSpread(AimDir);
	const FVector TraceEnd = MuzzleLoc + SpreadDir * Weapon->MaxRange;

	// ----- ④ LineTrace 命中检测 -----
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Weapon);
	QueryParams.AddIgnoredActor(GetAvatarActorFromActorInfo());
	// 也忽略武器的 Owner（Character）
	if (AActor* OwnerActor = Weapon->GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor->GetOwner());
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);           // 命中角色
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);    // 命中墙壁/地面
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);   // 命中可移动物体

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByObjectType(Hit, MuzzleLoc, TraceEnd, ObjectParams, QueryParams);

	// 调试弹道线（绿色命中，红色未命中）
	DrawDebugLine(GetWorld(), MuzzleLoc, bHit ? Hit.ImpactPoint : TraceEnd,
		bHit ? FColor::Green : FColor::Red, false, 0.5f, 0, 1.0f);

	// ----- ⑤ 伤害处理 -----
	if (bHit && Hit.GetActor())
	{
		const float Damage = Weapon->GetEffectiveDamage();
		AActor* HitTarget = Hit.GetActor();

		// 只有命中角色才施加伤害（墙壁/物体忽略）
		if (ABaseCharacter* TargetChar = Cast<ABaseCharacter>(HitTarget))
		{
			if (UAbilitySystemComponent* TargetASC = TargetChar->GetAbilitySystemComponent())
			{
				if (DamageEffectClass)
				{
					// 创建伤害 GE → SetByCaller 设置伤害值 → 应用到目标 ASC
					FGameplayEffectSpecHandle DamageSpec = MakeOutgoingGameplayEffectSpec(
						DamageEffectClass, GetAbilityLevel());
					if (DamageSpec.IsValid())
					{
						DamageSpec.Data->SetSetByCallerMagnitude(
							BaseGameplayTags::Data_Damage, Damage);
						TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpec.Data.Get());
					}
				}
			}
		}
	}

	// ----- ⑥ 视觉效果：蒙太奇 + 后坐力 -----
	Weapon->WeaponOwner->PlayFiringMontage(Weapon->FiringMontage);
	Weapon->WeaponOwner->AddWeaponRecoil(
		Weapon->GetEffectiveRecoil(),
		Weapon->GetEffectiveRecoilInterpSpeed(),
		Weapon->GetEffectiveRecoilRecoverySpeed(),
		Weapon->GetEffectiveRecoilMaxAccumulation());

	// ----- ⑦ 弹药消耗（三步）-----
	// 步骤 1：先在 Weapon 上减（不依赖 GE 配置）
	Weapon->CurrentBullets = FMath::Max(0, Weapon->CurrentBullets - 1);

	// 步骤 2：GE 消耗（如果配置了）— 通过 AttributeSet 修改弹药
	if (AmmoCostEffectClass)
	{
		FGameplayEffectSpecHandle AmmoSpec = MakeOutgoingGameplayEffectSpec(
			AmmoCostEffectClass, GetAbilityLevel());
		ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo,
			CurrentActivationInfo, AmmoSpec);
	}

	// 步骤 3：确保 AttributeSet 与 Weapon 数据一致（兜底同步）
	if (ASC)
	{
		ASC->SetNumericAttributeBase(
			UBaseWeaponAttributeSet::GetCurrentAmmoAttribute(),
			static_cast<float>(Weapon->CurrentBullets));
	}

	// ----- ⑧ 记录射击时间戳 + 刷新 HUD -----
	Weapon->TimeOfLastShot = GetWorld()->GetTimeSeconds();
	Weapon->WeaponOwner->UpdateWeaponHUD(
		Weapon->CurrentBullets, Weapon->GetEffectiveMagazineSize());

	// ----- ⑨ 弹药耗尽检查（射击后）→ 广播事件 → 自动换弹 -----
	const bool bEmptyAfterFire = (AttrSet && AttrSet->GetCurrentAmmo() <= 0.0f)
		|| Weapon->CurrentBullets <= 0;
	if (bEmptyAfterFire)
	{
		FGameplayEventData EventData;
		EventData.EventTag = BaseGameplayTags::Event_OutOfAmmo;
		ASC->HandleGameplayEvent(BaseGameplayTags::Event_OutOfAmmo, &EventData);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// ----- ⑩ 松开开火键 → 立即结束（半自动 & 全自动都适用）-----
	if (bInputReleased)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// ----- ⑪ 延迟 RefireRate 后回调 OnRefireReady -----
	const float Delay = Weapon->RefireRate;
	UAbilityTask_WaitDelay* RefireTask = UAbilityTask_WaitDelay::WaitDelay(this, Delay);
	RefireTask->OnFinish.AddDynamic(this, &UGA_WeaponFire::OnRefireReady);
	RefireTask->ReadyForActivation();
}

// ================================================================
//  OnRefireReady — 射击循环的决策点
// ================================================================
void UGA_WeaponFire::OnRefireReady()
{
	// 全自动 + 未松开 → 继续射击
	if (!bInputReleased && GetWeapon() && GetWeapon()->bFullAuto)
	{
		PerformFire();
	}
	else
	{
		// 半自动 或 已松开 → 结束能力（下次按下时重新激活）
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

// ================================================================
//  GetWeapon — 从 ASC 获取当前武器
//  不直接依赖 APlayerCharacter / ABaseEnemy，通过 ASC 的解耦层
// ================================================================
ABaseWeapon* UGA_WeaponFire::GetWeapon() const
{
	if (const UBaseAbilitySystemComponent* MyASC = Cast<UBaseAbilitySystemComponent>(
		GetAbilitySystemComponentFromActorInfo()))
	{
		return MyASC->GetCurrentWeapon();
	}
	return nullptr;
}
