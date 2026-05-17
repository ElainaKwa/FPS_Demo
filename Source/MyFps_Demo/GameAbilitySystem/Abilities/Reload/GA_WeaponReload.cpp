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

UGA_WeaponReload::UGA_WeaponReload()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	SetAssetTags(FGameplayTagContainer(BaseGameplayTags::Ability_Reload));

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = BaseGameplayTags::Event_OutOfAmmo;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

bool UGA_WeaponReload::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	if (const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		if (ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Reloading))
		{
			return false;
		}
	}

	return true;
}

void UGA_WeaponReload::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!K2_HasAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Reload] ActivateAbility 被调用 | Authority=%d | PredictionKey=%d"),
		K2_HasAuthority() ? 1 : 0, ActivationInfo.GetActivationPredictionKey().Current);

	ABaseWeapon* Weapon = GetWeapon();
	if (!Weapon)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Reload] 失败: 武器为 null"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (Weapon->CurrentBullets >= Weapon->GetEffectiveMagazineSize())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Reload] 失败: 弹药已满 (%d / %d)"), Weapon->CurrentBullets, Weapon->GetEffectiveMagazineSize());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Reload] 失败: CommitAbility 返回 false"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(BaseGameplayTags::State_Reloading);

		if (UBaseAbilitySystemComponent* MyASC = Cast<UBaseAbilitySystemComponent>(ASC))
		{
			MyASC->CancelSprintAbility();
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[Reload] 激活成功，开始换弹流程"));

	Weapon->bIsReloading = true;

	if (APlayerCharacter* Player = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo()))
	{
		Player->MulticastSetReloading(true);
	}

	if (Weapon->WeaponOwner)
	{
		Weapon->WeaponOwner->PlayReloadMontage(Weapon->ReloadMontage);
	}

	const float EffectiveTime = Weapon->GetEffectiveReloadTime();

	if (Weapon->MagazineDropDelay > 0.0f)
	{
		UAbilityTask_WaitDelay* DropTask = UAbilityTask_WaitDelay::WaitDelay(this, Weapon->MagazineDropDelay);
		DropTask->OnFinish.AddDynamic(this, &UGA_WeaponReload::OnDropMagazine);
		DropTask->ReadyForActivation();
	}
	else
	{
		OnDropMagazine();
	}

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

	UAbilityTask_WaitDelay* CompleteTask = UAbilityTask_WaitDelay::WaitDelay(this, EffectiveTime);
	CompleteTask->OnFinish.AddDynamic(this, &UGA_WeaponReload::OnReloadComplete);
	CompleteTask->ReadyForActivation();
}

void UGA_WeaponReload::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (K2_HasAuthority())
	{
		if (APlayerCharacter* Player = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo()))
		{
			Player->MulticastSetReloading(false);
		}
	}

	if (ABaseWeapon* Weapon = GetWeapon())
	{
		Weapon->bIsReloading = false;
		if (bWasCancelled)
		{
			Weapon->CancelReloadVisuals();
		}
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(BaseGameplayTags::State_Reloading);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_WeaponReload::OnDropMagazine()
{
	if (ABaseWeapon* Weapon = GetWeapon())
	{
		Weapon->DropMagazine();
	}
}

void UGA_WeaponReload::OnInsertMagazine()
{
	if (ABaseWeapon* Weapon = GetWeapon())
	{
		Weapon->InsertMagazine();
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

	if (ReloadAmmoEffectClass)
	{
		FGameplayEffectSpecHandle RefillSpec = MakeOutgoingGameplayEffectSpec(ReloadAmmoEffectClass, GetAbilityLevel());
		ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, RefillSpec);
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (const UBaseWeaponAttributeSet* AttrSet = Cast<UBaseWeaponAttributeSet>(ASC->GetAttributeSet(UBaseWeaponAttributeSet::StaticClass())))
		{
			const int32 NewBullets = ReloadAmmoEffectClass
				? FMath::FloorToInt32(AttrSet->GetCurrentAmmo())
				: Weapon->GetEffectiveMagazineSize();

			ASC->SetNumericAttributeBase(UBaseWeaponAttributeSet::GetCurrentAmmoAttribute(), static_cast<float>(NewBullets));

			Weapon->CurrentBullets = NewBullets;
		}
	}

	if (Weapon->WeaponOwner)
	{
		Weapon->WeaponOwner->UpdateWeaponHUD(Weapon->CurrentBullets, Weapon->GetEffectiveMagazineSize());
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

ABaseWeapon* UGA_WeaponReload::GetWeapon() const
{
	if (const UBaseAbilitySystemComponent* MyASC = Cast<UBaseAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
	{
		return MyASC->GetCurrentWeapon();
	}
	return nullptr;
}
