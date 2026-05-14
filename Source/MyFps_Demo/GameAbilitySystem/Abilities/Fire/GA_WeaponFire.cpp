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

UGA_WeaponFire::UGA_WeaponFire()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	SetAssetTags(FGameplayTagContainer(BaseGameplayTags::Ability_Fire));
}

void UGA_WeaponFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Reloading))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ABaseWeapon* Weapon = GetWeapon())
	{
		Weapon->bIsFiring = true;
	}

	bInputReleased = false;

	UAbilityTask_WaitInputRelease* WaitRelease = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
	WaitRelease->OnRelease.AddDynamic(this, &UGA_WeaponFire::OnInputReleased);
	WaitRelease->ReadyForActivation();

	PerformFire();
}

void UGA_WeaponFire::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
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

void UGA_WeaponFire::PerformFire()
{
	ABaseWeapon* Weapon = GetWeapon();
	if (!Weapon)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const UBaseWeaponAttributeSet* AttrSet = ASC ? Cast<UBaseWeaponAttributeSet>(ASC->GetAttributeSet(UBaseWeaponAttributeSet::StaticClass())) : nullptr;

	const bool bOutOfAmmo = (AttrSet && AttrSet->GetCurrentAmmo() <= 0.0f) || Weapon->CurrentBullets <= 0;
	if (bOutOfAmmo)
	{
		FGameplayEventData EventData;
		EventData.EventTag = BaseGameplayTags::Event_OutOfAmmo;
		ASC->HandleGameplayEvent(BaseGameplayTags::Event_OutOfAmmo, &EventData);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	const FVector MuzzleLoc = Weapon->GetMuzzleLocation();
	const FVector TargetLocation = Weapon->WeaponOwner->GetWeaponTargetLocation();
	const FVector AimDir = (TargetLocation - MuzzleLoc).GetSafeNormal();
	const FVector SpreadDir = Weapon->CalculateSpread(AimDir);
	const FVector TraceEnd = MuzzleLoc + SpreadDir * Weapon->MaxRange;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Weapon);
	QueryParams.AddIgnoredActor(GetAvatarActorFromActorInfo());

	if (AActor* OwnerActor = Weapon->GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor->GetOwner());
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByObjectType(Hit, MuzzleLoc, TraceEnd, ObjectParams, QueryParams);

	DrawDebugLine(GetWorld(), MuzzleLoc, bHit ? Hit.ImpactPoint : TraceEnd, bHit ? FColor::Green : FColor::Red, false, 0.5f, 0, 1.0f);

	if (bHit && Hit.GetActor())
	{
		const float Damage = Weapon->GetEffectiveDamage();
		AActor* HitTarget = Hit.GetActor();

		if (ABaseCharacter* TargetChar = Cast<ABaseCharacter>(HitTarget))
		{
			if (UAbilitySystemComponent* TargetASC = TargetChar->GetAbilitySystemComponent())
			{
				if (DamageEffectClass)
				{
					FGameplayEffectSpecHandle DamageSpec = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
					if (DamageSpec.IsValid())
					{
						DamageSpec.Data->SetSetByCallerMagnitude(BaseGameplayTags::Data_Damage, Damage);
						TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpec.Data.Get());
					}
				}
			}
		}
	}

	Weapon->WeaponOwner->PlayFiringMontage(Weapon->FiringMontage);
	Weapon->WeaponOwner->AddWeaponRecoil(Weapon->GetEffectiveRecoil());

	Weapon->CurrentBullets = FMath::Max(0, Weapon->CurrentBullets - 1);

	if (AmmoCostEffectClass)
	{
		FGameplayEffectSpecHandle AmmoSpec = MakeOutgoingGameplayEffectSpec(AmmoCostEffectClass, GetAbilityLevel());
		ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, AmmoSpec);
	}

	if (ASC)
	{
		ASC->SetNumericAttributeBase(UBaseWeaponAttributeSet::GetCurrentAmmoAttribute(), static_cast<float>(Weapon->CurrentBullets));
	}

	Weapon->TimeOfLastShot = GetWorld()->GetTimeSeconds();

	Weapon->WeaponOwner->UpdateWeaponHUD(Weapon->CurrentBullets, Weapon->GetEffectiveMagazineSize());

	const bool bEmptyAfterFire = (AttrSet && AttrSet->GetCurrentAmmo() <= 0.0f) || Weapon->CurrentBullets <= 0;
	if (bEmptyAfterFire)
	{
		FGameplayEventData EventData;
		EventData.EventTag = BaseGameplayTags::Event_OutOfAmmo;
		ASC->HandleGameplayEvent(BaseGameplayTags::Event_OutOfAmmo, &EventData);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	if (bInputReleased)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	const float Delay = Weapon->RefireRate;
	UAbilityTask_WaitDelay* RefireTask = UAbilityTask_WaitDelay::WaitDelay(this, Delay);
	RefireTask->OnFinish.AddDynamic(this, &UGA_WeaponFire::OnRefireReady);
	RefireTask->ReadyForActivation();
}

void UGA_WeaponFire::OnRefireReady()
{
	if (!bInputReleased && GetWeapon() && GetWeapon()->bFullAuto)
	{
		PerformFire();
	}
	else
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

ABaseWeapon* UGA_WeaponFire::GetWeapon() const
{
	if (const UBaseAbilitySystemComponent* MyASC = Cast<UBaseAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
	{
		return MyASC->GetCurrentWeapon();
	}
	return nullptr;
}
