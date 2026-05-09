// Copyright Epic Games, Inc. All Rights Reserved.

#include "GA_WeaponReload.h"
#include "BaseAbilitySystemComponent.h"
#include "BaseWeaponAttributeSet.h"
#include "BaseGameplayTags.h"
#include "BaseWeapon.h"
#include "BaseWeaponHolder.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"

UGA_WeaponReload::UGA_WeaponReload()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	SetAssetTags(FGameplayTagContainer(BaseGameplayTags::Ability_Reload));
	ActivationOwnedTags.AddTag(BaseGameplayTags::State_Reloading);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = BaseGameplayTags::Event_OutOfAmmo;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

void UGA_WeaponReload::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ABaseWeapon* Weapon = GetWeapon();
	if (!Weapon)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (Weapon->CurrentBullets >= Weapon->GetEffectiveMagazineSize())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Weapon->bIsReloading = true;

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
	if (bWasCancelled)
	{
		if (ABaseWeapon* Weapon = GetWeapon())
		{
			Weapon->bIsReloading = false;
			Weapon->CancelReloadVisuals();
		}
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

	if (ReloadAmmoEffectClass)
	{
		FGameplayEffectSpecHandle RefillSpec = MakeOutgoingGameplayEffectSpec(ReloadAmmoEffectClass, GetAbilityLevel());
		ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, RefillSpec);
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (const UBaseWeaponAttributeSet* AttrSet = Cast<UBaseWeaponAttributeSet>(ASC->GetAttributeSet(UBaseWeaponAttributeSet::StaticClass())))
		{
			Weapon->CurrentBullets = FMath::FloorToInt32(AttrSet->GetCurrentAmmo());
		}
	}

	Weapon->bIsReloading = false;

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
