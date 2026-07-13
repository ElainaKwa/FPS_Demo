// Copyright Epic Games, Inc. All Rights Reserved.

#include "GA_WeaponReload.h"
#include "BaseGameplayAbility.h"
#include "BaseGameplayTags.h"
#include "BaseWeaponAttributeSet.h"
#include "BaseWeapon.h"
#include "PlayerCharacter.h"
#include "BaseAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"

// ================================================================
//  构造 — 策略 + AssetTag + 自动触发配置
// ================================================================
UGA_WeaponReload::UGA_WeaponReload()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	SetAssetTags(FGameplayTagContainer(BaseGameplayTags::Ability_Reload));

	// 配置 GameplayEvent 自动触发：收到 Event_OutOfAmmo 时自动激活换弹
	// 不需要在 ABaseCharacter 中手动监听事件
	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = BaseGameplayTags::Event_OutOfAmmo;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

// ================================================================
//  CanActivateAbility — 前置检查
//  替代 ActivationBlockedTags（BP CDO 会覆盖 C++ 设置）
// ================================================================
bool UGA_WeaponReload::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// State_Reloading 阻塞自身重复激活
	if (const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		if (ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Reloading))
		{
			return false;
		}
	}

	return true;
}

// ================================================================
//  激活 — 服务端入口
// ================================================================
void UGA_WeaponReload::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// ----- 权威守卫 -----
	if (!K2_HasAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Reload] ActivateAbility 被调用 | Authority=%d | PredictionKey=%d"),
		K2_HasAuthority() ? 1 : 0, ActivationInfo.GetActivationPredictionKey().Current);

	// ----- 武器有效性检查 -----
	ABaseWeapon* Weapon = GetWeapon();
	if (!Weapon)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Reload] 失败: 武器为 null"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ----- 弹药已满则拒绝 -----
	if (Weapon->CurrentBullets >= Weapon->GetEffectiveMagazineSize())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Reload] 失败: 弹药已满 (%d / %d)"),
			Weapon->CurrentBullets, Weapon->GetEffectiveMagazineSize());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ----- CommitAbility -----
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Reload] 失败: CommitAbility 返回 false"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ----- 施加 State_Reloading 标签（阻塞开火 + 阻塞自身）-----
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(BaseGameplayTags::State_Reloading);

		// 换弹时自动取消冲刺
		if (UBaseAbilitySystemComponent* MyASC = Cast<UBaseAbilitySystemComponent>(ASC))
		{
			MyASC->CancelSprintAbility();
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[Reload] 激活成功，开始换弹流程"));

	// ----- 标记换弹状态 + 广播到客户端（同步 TP 动画）-----
	Weapon->bIsReloading = true;

	if (APlayerCharacter* Player = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo()))
	{
		Player->MulticastSetReloading(true);
	}

	// ----- 播放换弹蒙太奇 -----
	if (Weapon->WeaponOwner)
	{
		Weapon->WeaponOwner->PlayReloadMontage(Weapon->ReloadMontage);
	}

	const float EffectiveTime = Weapon->GetEffectiveReloadTime();

	// ----- 并发计时任务①：弹匣掉落 -----
	if (Weapon->MagazineDropDelay > 0.0f)
	{
		UAbilityTask_WaitDelay* DropTask = UAbilityTask_WaitDelay::WaitDelay(
			this, Weapon->MagazineDropDelay);
		DropTask->OnFinish.AddDynamic(this, &UGA_WeaponReload::OnDropMagazine);
		DropTask->ReadyForActivation();
	}
	else
	{
		OnDropMagazine();  // 延迟为 0 时立即执行
	}

	// ----- 并发计时任务②：弹匣插入 -----
	// 插入时间 = 总时间 - 提前量（对齐动画中弹匣插入的瞬间）
	const float InsertTime = FMath::Max(0.0f, EffectiveTime - Weapon->MagazineInsertBeforeEnd);
	if (InsertTime > 0.0f)
	{
		UAbilityTask_WaitDelay* InsertTask = UAbilityTask_WaitDelay::WaitDelay(this, InsertTime);
		InsertTask->OnFinish.AddDynamic(this, &UGA_WeaponReload::OnInsertMagazine);
		InsertTask->ReadyForActivation();
	}
	else
	{
		OnInsertMagazine();
	}

	// ----- 并发计时任务③：换弹完成 -----
	UAbilityTask_WaitDelay* CompleteTask = UAbilityTask_WaitDelay::WaitDelay(this, EffectiveTime);
	CompleteTask->OnFinish.AddDynamic(this, &UGA_WeaponReload::OnReloadComplete);
	CompleteTask->ReadyForActivation();
}

// ================================================================
//  结束 — 清理换弹状态
// ================================================================
void UGA_WeaponReload::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 广播换弹结束到客户端
	if (K2_HasAuthority())
	{
		if (APlayerCharacter* Player = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo()))
		{
			Player->MulticastSetReloading(false);
		}
	}

	// 重置武器状态
	if (ABaseWeapon* Weapon = GetWeapon())
	{
		Weapon->bIsReloading = false;
		// 被取消时清理左手弹匣视觉效果（如切枪时取消换弹）
		if (bWasCancelled)
		{
			Weapon->CancelReloadVisuals();
		}
	}

	// 移除 State_Reloading 标签 → 解除阻塞
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(BaseGameplayTags::State_Reloading);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ================================================================
//  计时回调
// ================================================================

void UGA_WeaponReload::OnDropMagazine()
{
	if (ABaseWeapon* Weapon = GetWeapon())
	{
		Weapon->DropMagazine();  // 生成物理弹匣 + Multicast 弹匣视觉效果
	}
}

void UGA_WeaponReload::OnInsertMagazine()
{
	if (ABaseWeapon* Weapon = GetWeapon())
	{
		Weapon->InsertMagazine();  // Multicast 隐藏左手弹匣 + 恢复弹匣槽
	}
}

void UGA_WeaponReload::OnReloadComplete()
{
	ABaseWeapon* Weapon = GetWeapon();
	if (!Weapon)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Reload] OnReloadComplete | Authority=%d | ReloadAmmoEffectClass=%s | CurrentBullets=%d"),
		K2_HasAuthority() ? 1 : 0,
		ReloadAmmoEffectClass ? *ReloadAmmoEffectClass->GetName() : TEXT("null"),
		Weapon->CurrentBullets);

	// ----- ① 通过 GE 补充弹药（如果配置了）-----
	if (ReloadAmmoEffectClass)
	{
		FGameplayEffectSpecHandle RefillSpec = MakeOutgoingGameplayEffectSpec(
			ReloadAmmoEffectClass, GetAbilityLevel());
		ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo,
			CurrentActivationInfo, RefillSpec);
	}

	// ----- ② 读取 GE 修改后的弹药值（或回退到满弹匣）-----
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (const UBaseWeaponAttributeSet* AttrSet = Cast<UBaseWeaponAttributeSet>(
			ASC->GetAttributeSet(UBaseWeaponAttributeSet::StaticClass())))
		{
			// GE 配置时从 AttributeSet 读取（已包含 GE 修改后的值）
			// GE 未配置时回退到满弹匣
			const int32 NewBullets = ReloadAmmoEffectClass
				? FMath::FloorToInt32(AttrSet->GetCurrentAmmo())
				: Weapon->GetEffectiveMagazineSize();

			// 同步 AttributeSet
			ASC->SetNumericAttributeBase(
				UBaseWeaponAttributeSet::GetCurrentAmmoAttribute(),
				static_cast<float>(NewBullets));

			// 同步 Weapon
			Weapon->CurrentBullets = NewBullets;
		}
	}

	// ----- ③ 刷新 HUD -----
	if (Weapon->WeaponOwner)
	{
		Weapon->WeaponOwner->UpdateWeaponHUD(
			Weapon->CurrentBullets, Weapon->GetEffectiveMagazineSize());
	}

	// ----- ④ 结束能力 → 移除 State_Reloading 标签 -----
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

ABaseWeapon* UGA_WeaponReload::GetWeapon() const
{
	if (const UBaseAbilitySystemComponent* MyASC = Cast<UBaseAbilitySystemComponent>(
		GetAbilitySystemComponentFromActorInfo()))
	{
		return MyASC->GetCurrentWeapon();
	}
	return nullptr;
}
