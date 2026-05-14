// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseCharacter.h"
#include "Weapons/BaseWeapon.h"
#include "GameAbilitySystem/BaseAbilitySystemComponent.h"
#include "GameAbilitySystem/BaseWeaponAttributeSet.h"
#include "GameAbilitySystem/BaseHealthAttributeSet.h"
#include "GameAbilitySystem/BaseGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "AbilitySystemComponent.h"

ABaseCharacter::ABaseCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	AbilitySystemComponent = CreateDefaultSubobject<UBaseAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	HealthAttributeSet = CreateDefaultSubobject<UBaseHealthAttributeSet>(TEXT("HealthAttributeSet"));

	WeaponAttributeSet = CreateDefaultSubobject<UBaseWeaponAttributeSet>(TEXT("WeaponAttributeSet"));
}

void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	InitAbilitySystem();

	SpawnDefaultWeapon();

	if (!GetMesh()->GetSkeletalMeshAsset())
	{
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationCustomMode);
	}
}

void ABaseCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void ABaseCharacter::InitAbilitySystem()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		AbilitySystemComponent->GrantDefaultAbilities();
	}
}

void ABaseCharacter::SpawnDefaultWeapon()
{
	if (DefaultWeaponClass)
	{
		AddWeapon(DefaultWeaponClass);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] DefaultWeaponClass 未配置"), *GetName());
	}
}

UAbilitySystemComponent* ABaseCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ABaseCharacter::SyncAttributeSetFromWeapon() const
{
	if (!WeaponAttributeSet || !CurrentWeapon)
	{
		return;
	}

	const int32 MaxAmmo = CurrentWeapon->GetEffectiveMagazineSize();
	WeaponAttributeSet->SetMaxAmmo(static_cast<float>(MaxAmmo));
	WeaponAttributeSet->SetCurrentAmmo(static_cast<float>(CurrentWeapon->CurrentBullets));
}

void ABaseCharacter::AttachWeaponMeshes(ABaseWeapon* Weapon)
{
	if (!Weapon)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] 挂载武器 %s  |  MeshHasAsset=%d  |  Socket=%s"),
		*GetName(), *Weapon->GetName(),
		GetMesh()->GetSkeletalMeshAsset() != nullptr,
		*ThirdPersonWeaponSocket.ToString());

	Weapon->GetFirstPersonMesh()->SetVisibility(false);
	Weapon->GetThirdPersonMesh()->SetVisibility(true);

	const FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, false);

	if (GetMesh()->GetSkeletalMeshAsset())
	{
		Weapon->GetThirdPersonMesh()->AttachToComponent(GetMesh(), AttachRules, ThirdPersonWeaponSocket);
	}
	else
	{
		Weapon->GetThirdPersonMesh()->AttachToComponent(GetMesh(), AttachRules);
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] 武器 TP Mesh: HasAsset=%d, bVisible=%d, bOwnerNoSee=%d"),
		*GetName(),
		Weapon->GetThirdPersonMesh()->GetSkeletalMeshAsset() != nullptr,
		Weapon->GetThirdPersonMesh()->IsVisible(),
		Weapon->GetThirdPersonMesh()->bOwnerNoSee);
}

FVector ABaseCharacter::GetWeaponTargetLocation() const
{
	const FVector Start = GetActorLocation() + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FVector End = Start + (GetActorForwardVector() * 10000.0f);

	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams);

	return Hit.bBlockingHit ? Hit.ImpactPoint : Hit.TraceEnd;
}

void ABaseCharacter::PlayFiringMontage(UAnimMontage* Montage)
{
	if (Montage && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_Play(Montage);
	}
}

void ABaseCharacter::PlayReloadMontage(UAnimMontage* Montage)
{
	if (Montage && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_Play(Montage);
	}
}

void ABaseCharacter::AddWeaponRecoil(float RecoilAmount)
{
}

void ABaseCharacter::UpdateWeaponHUD(int32 CurrentAmmo, int32 MaxAmmo)
{
	OnBulletCountUpdated.Broadcast(CurrentAmmo, MaxAmmo);
}

void ABaseCharacter::UpdateHealthHUD()
{
	if (HealthAttributeSet)
	{
		OnHealthUpdated.Broadcast(HealthAttributeSet->GetHealth(), HealthAttributeSet->GetMaxHealth());
	}
}

void ABaseCharacter::OnDeath()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(BaseGameplayTags::State_Dead);
		AbilitySystemComponent->CancelAllAbilities();
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (GetMesh()->GetSkeletalMeshAsset())
	{
		GetMesh()->SetSimulatePhysics(true);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	}

	if (CurrentWeapon)
	{
		CurrentWeapon->DropToGround();
		CurrentWeapon = nullptr;
	}
}

void ABaseCharacter::AddWeapon(TSubclassOf<ABaseWeapon> WeaponClass)
{
	if (!WeaponClass)
	{
		return;
	}

	ABaseWeapon* Existing = FindWeaponOfClass(WeaponClass);
	if (Existing)
	{
		SwitchToWeapon(Existing);
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABaseWeapon* NewWeapon = GetWorld()->SpawnActor<ABaseWeapon>(WeaponClass, GetActorTransform(), SpawnParams);
	if (!NewWeapon)
	{
		return;
	}

	OwnedWeapons.Add(NewWeapon);

	if (CurrentWeapon)
	{
		CurrentWeapon->DeactivateWeapon();
	}

	CurrentWeapon = NewWeapon;
	CurrentWeapon->ActivateWeapon();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->SetCurrentWeapon(CurrentWeapon);
	}

	SyncAttributeSetFromWeapon();
}

void ABaseCharacter::SwitchToWeapon(ABaseWeapon* NewWeapon)
{
	if (!NewWeapon || NewWeapon == CurrentWeapon)
	{
		return;
	}

	if (CurrentWeapon)
	{
		CurrentWeapon->DeactivateWeapon();
	}

	CurrentWeapon = NewWeapon;
	CurrentWeapon->ActivateWeapon();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->SetCurrentWeapon(CurrentWeapon);
	}

	SyncAttributeSetFromWeapon();
}

ABaseWeapon* ABaseCharacter::FindWeaponOfClass(TSubclassOf<ABaseWeapon> WeaponClass) const
{
	for (ABaseWeapon* Weapon : OwnedWeapons)
	{
		if (Weapon && Weapon->IsA(WeaponClass))
		{
			return Weapon;
		}
	}
	return nullptr;
}
