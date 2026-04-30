// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyFpsWeapon.h"
#include "MyFpsWeaponAttachment.h"
#include "MyFpsDroppedMagazine.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DamageType.h"
#include "Kismet/KismetMathLibrary.h"
#include "Animation/AnimInstance.h"
#include "Engine/DamageEvents.h"

AMyFpsWeapon::AMyFpsWeapon()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	FirstPersonMesh->SetupAttachment(RootComponent);
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonMesh->bSelfShadowOnly = true;

	ThirdPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Third Person Mesh"));
	ThirdPersonMesh->SetupAttachment(RootComponent);
	ThirdPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	ThirdPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	ThirdPersonMesh->bOwnerNoSee = true;

	LeftHandMagazineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Left Hand Magazine"));
	LeftHandMagazineMesh->SetupAttachment(RootComponent);
	LeftHandMagazineMesh->SetCollisionProfileName(FName("NoCollision"));
	LeftHandMagazineMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	LeftHandMagazineMesh->SetVisibility(false);

	SetActorHiddenInGame(true);
}

void AMyFpsWeapon::BeginPlay()
{
	Super::BeginPlay();

	GetOwner()->OnDestroyed.AddDynamic(this, &AMyFpsWeapon::OnOwnerDestroyed);

	WeaponOwner = Cast<IMyFpsWeaponHolder>(GetOwner());

	CurrentBullets = MagazineSize;

	if (WeaponOwner)
	{
		WeaponOwner->AttachWeaponMeshes(this);
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, GetEffectiveMagazineSize());
	}
}

void AMyFpsWeapon::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
	GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
	GetWorld()->GetTimerManager().ClearTimer(MagDropTimer);
	GetWorld()->GetTimerManager().ClearTimer(MagInsertTimer);

	ClearAllAttachments();
}

void AMyFpsWeapon::OnOwnerDestroyed(AActor* DestroyedActor)
{
	Destroy();
}

void AMyFpsWeapon::ActivateWeapon()
{
	SetActorHiddenInGame(false);
	bIsFiring = false;
	bIsReloading = false;

	GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
	GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);

	if (WeaponOwner)
	{
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, GetEffectiveMagazineSize());
	}
}

void AMyFpsWeapon::DeactivateWeapon()
{
	StopFiring();
	CancelReload();
	SetActorHiddenInGame(true);
}

void AMyFpsWeapon::StartFiring()
{
	if (bIsReloading)
	{
		return;
	}

	bIsFiring = true;

	const float TimeSinceLastShot = GetWorld()->GetTimeSeconds() - TimeOfLastShot;

	if (TimeSinceLastShot >= RefireRate)
	{
		Fire();
	}
	else if (bFullAuto)
	{
		const float Delay = RefireRate - TimeSinceLastShot;
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AMyFpsWeapon::Fire, Delay, false);
	}
}

void AMyFpsWeapon::StopFiring()
{
	bIsFiring = false;
	GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
}

void AMyFpsWeapon::Fire()
{
	if (!bIsFiring || bIsReloading)
	{
		return;
	}

	if (CurrentBullets <= 0)
	{
		bIsFiring = false;
		StartReload();
		return;
	}

	TimeOfLastShot = GetWorld()->GetTimeSeconds();

	--CurrentBullets;

	if (WeaponOwner)
	{
		const FVector TargetLocation = WeaponOwner->GetWeaponTargetLocation();
		const FVector MuzzleLoc = GetMuzzleLocation();
		const FVector AimDir = (TargetLocation - MuzzleLoc).GetSafeNormal();
		const FVector SpreadDir = CalculateSpread(AimDir);
		const FVector TraceEnd = MuzzleLoc + SpreadDir * MaxRange;

		FHitResult Hit;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);
		QueryParams.AddIgnoredActor(GetOwner());

		if (AActor* OwnerActor = GetOwner())
		{
			QueryParams.AddIgnoredActor(OwnerActor->GetOwner());
		}

		bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, MuzzleLoc, TraceEnd, ECC_Visibility, QueryParams);

		if (bHit && Hit.GetActor())
		{
			const float Damage = GetEffectiveDamage();
			FPointDamageEvent DamageEvent(Damage, Hit, SpreadDir, DamageTypeClass);
			Hit.GetActor()->TakeDamage(Damage, DamageEvent, GetInstigatorController(), this);
		}

		WeaponOwner->PlayFiringMontage(FiringMontage);
		WeaponOwner->AddWeaponRecoil(GetEffectiveRecoil());
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, GetEffectiveMagazineSize());
	}

	if (CurrentBullets <= 0)
	{
		bIsFiring = false;
		StartReload();
	}
	else if (bIsFiring && bFullAuto)
	{
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AMyFpsWeapon::Fire, RefireRate, false);
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AMyFpsWeapon::OnRefireCooldown, RefireRate, false);
	}
}

void AMyFpsWeapon::OnRefireCooldown()
{
}

FVector AMyFpsWeapon::GetMuzzleLocation() const
{
	return FirstPersonMesh->GetSocketLocation(MuzzleSocketName);
}

FVector AMyFpsWeapon::CalculateSpread(const FVector& AimDirection) const
{
	const float Variance = GetEffectiveAimVariance();
	if (Variance <= 0.0f)
	{
		return AimDirection;
	}

	return UKismetMathLibrary::RandomUnitVectorInConeInDegrees(AimDirection, Variance);
}

FName AMyFpsWeapon::GetSocketNameForSlot(EMyFpsAttachmentSlot Slot) const
{
	switch (Slot)
	{
	case EMyFpsAttachmentSlot::Muzzle:		return MuzzleSocket;
	case EMyFpsAttachmentSlot::Optic:		return OpticSocket;
	case EMyFpsAttachmentSlot::Tactical:	return TacticalSocket;
	case EMyFpsAttachmentSlot::Foregrip:	return ForegripSocket;
	case EMyFpsAttachmentSlot::Magazine:	return MagazineSocket;
	default:								return NAME_None;
	}
}

int32 AMyFpsWeapon::GetMaxAmmo() const
{
	return GetEffectiveMagazineSize();
}

// ---- Attachment System ----

void AMyFpsWeapon::EquipAttachment(EMyFpsAttachmentSlot Slot, TSubclassOf<AMyFpsWeaponAttachment> AttachmentClass)
{
	if (!AttachmentClass)
	{
		return;
	}

	RemoveAttachment(Slot);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AMyFpsWeaponAttachment* NewAttachment = GetWorld()->SpawnActor<AMyFpsWeaponAttachment>(AttachmentClass, SpawnParams);
	if (!NewAttachment)
	{
		return;
	}

	const FName SocketName = GetSocketNameForSlot(Slot);

	FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, false);
	NewAttachment->AttachToComponent(FirstPersonMesh, AttachRules, SocketName);

	NewAttachment->GetAttachmentMesh()->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	NewAttachment->GetAttachmentMesh()->bOnlyOwnerSee = true;

	EquippedAttachments.Add(Slot, NewAttachment);

	NewAttachment->BP_OnEquipped(GetOwner());

	if (WeaponOwner)
	{
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, GetEffectiveMagazineSize());
	}
}

void AMyFpsWeapon::RemoveAttachment(EMyFpsAttachmentSlot Slot)
{
	TObjectPtr<AMyFpsWeaponAttachment> Existing;
	if (EquippedAttachments.RemoveAndCopyValue(Slot, Existing) && Existing)
	{
		Existing->BP_OnUnequipped(GetOwner());
		Existing->Destroy();

		if (WeaponOwner)
		{
			WeaponOwner->UpdateWeaponHUD(CurrentBullets, GetEffectiveMagazineSize());
		}
	}
}

AMyFpsWeaponAttachment* AMyFpsWeapon::GetAttachment(EMyFpsAttachmentSlot Slot) const
{
	const TObjectPtr<AMyFpsWeaponAttachment>* Found = EquippedAttachments.Find(Slot);
	return Found ? Found->Get() : nullptr;
}

bool AMyFpsWeapon::HasAttachment(EMyFpsAttachmentSlot Slot) const
{
	return EquippedAttachments.Contains(Slot);
}

void AMyFpsWeapon::ClearAllAttachments()
{
	TArray<EMyFpsAttachmentSlot> Slots;
	EquippedAttachments.GetKeys(Slots);
	for (const EMyFpsAttachmentSlot& Slot : Slots)
	{
		RemoveAttachment(Slot);
	}
}

float AMyFpsWeapon::GetEffectiveDamage() const
{
	float Mult = 1.0f;
	for (const auto& Pair : EquippedAttachments)
	{
		if (Pair.Value)
		{
			Mult *= Pair.Value->GetDamageMultiplier();
		}
	}
	return HitDamage * Mult;
}

float AMyFpsWeapon::GetEffectiveRecoil() const
{
	float Mult = 1.0f;
	for (const auto& Pair : EquippedAttachments)
	{
		if (Pair.Value)
		{
			Mult *= Pair.Value->GetRecoilMultiplier();
		}
	}
	return FiringRecoil * Mult;
}

float AMyFpsWeapon::GetEffectiveAimVariance() const
{
	float Mult = 1.0f;
	for (const auto& Pair : EquippedAttachments)
	{
		if (Pair.Value)
		{
			Mult *= Pair.Value->GetAimVarianceMultiplier();
		}
	}
	return AimVariance * Mult;
}

float AMyFpsWeapon::GetEffectiveReloadTime() const
{
	float Mult = 1.0f;
	for (const auto& Pair : EquippedAttachments)
	{
		if (Pair.Value)
		{
			Mult *= Pair.Value->GetReloadTimeMultiplier();
		}
	}
	return ReloadTime * Mult;
}

int32 AMyFpsWeapon::GetEffectiveMagazineSize() const
{
	int32 Bonus = 0;
	for (const auto& Pair : EquippedAttachments)
	{
		if (Pair.Value)
		{
			Bonus += Pair.Value->GetMagazineSizeBonus();
		}
	}
	return MagazineSize + Bonus;
}

// ---- Reload System ----

void AMyFpsWeapon::StartReload()
{
	if (bIsReloading)
	{
		return;
	}

	if (CurrentBullets >= GetEffectiveMagazineSize())
	{
		return;
	}

	bIsReloading = true;
	bIsFiring = false;
	GetWorld()->GetTimerManager().ClearTimer(RefireTimer);

	if (WeaponOwner)
	{
		WeaponOwner->PlayReloadMontage(ReloadMontage);
	}

	const float EffectiveTime = GetEffectiveReloadTime();
	GetWorld()->GetTimerManager().SetTimer(ReloadTimer, this, &AMyFpsWeapon::OnReloadComplete, EffectiveTime, false);

	if (MagazineDropDelay > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(MagDropTimer, this, &AMyFpsWeapon::DropMagazine, MagazineDropDelay, false);
	}
	else
	{
		DropMagazine();
	}

	const float InsertTime = FMath::Max(0.0f, EffectiveTime - MagazineInsertBeforeEnd);
	if (InsertTime > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(MagInsertTimer, this, &AMyFpsWeapon::InsertMagazine, InsertTime, false);
	}
	else
	{
		InsertMagazine();
	}
}

void AMyFpsWeapon::CancelReload()
{
	if (!bIsReloading)
	{
		return;
	}

	bIsReloading = false;
	GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
	GetWorld()->GetTimerManager().ClearTimer(MagDropTimer);
	GetWorld()->GetTimerManager().ClearTimer(MagInsertTimer);

	LeftHandMagazineMesh->SetVisibility(false);
	LeftHandMagazineMesh->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepWorld, false));
	LeftHandMagazineMesh->AttachToComponent(RootComponent, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, false), NAME_None);
}

void AMyFpsWeapon::OnReloadComplete()
{
	bIsReloading = false;
	GetWorld()->GetTimerManager().ClearTimer(MagInsertTimer);
	InsertMagazine();
	CurrentBullets = GetEffectiveMagazineSize();

	if (WeaponOwner)
	{
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, GetEffectiveMagazineSize());
	}
}

void AMyFpsWeapon::DropMagazine()
{
	const FName SocketName = GetSocketNameForSlot(EMyFpsAttachmentSlot::Magazine);
	const FVector SpawnLocation = FirstPersonMesh->GetSocketLocation(SocketName);
	const FVector SpawnVelocity = FirstPersonMesh->GetComponentVelocity() + FVector(FMath::FRandRange(-50.0f, 50.0f), FMath::FRandRange(-50.0f, 50.0f), FMath::FRandRange(-100.0f, -50.0f));

	UStaticMesh* MeshToDrop = DroppedMagazineMesh;

	AMyFpsWeaponAttachment* MagAttachment = GetAttachment(EMyFpsAttachmentSlot::Magazine);
	if (MagAttachment && MagAttachment->GetAttachmentMesh()->GetStaticMesh())
	{
		MeshToDrop = MagAttachment->GetAttachmentMesh()->GetStaticMesh();
	}

	if (MeshToDrop)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AMyFpsDroppedMagazine* DroppedMag = GetWorld()->SpawnActor<AMyFpsDroppedMagazine>(
			AMyFpsDroppedMagazine::StaticClass(), SpawnLocation, FRotator::ZeroRotator, SpawnParams);

		if (DroppedMag)
		{
			DroppedMag->Initialize(MeshToDrop, SpawnVelocity);
		}

		USceneComponent* ArmMesh = FirstPersonMesh->GetAttachParent();
		if (ArmMesh)
		{
			LeftHandMagazineMesh->SetStaticMesh(MeshToDrop);
			LeftHandMagazineMesh->SetVisibility(true);

			const FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, false);
			LeftHandMagazineMesh->AttachToComponent(ArmMesh, AttachRules, LeftHandSocketName);
		}
	}

	if (MagAttachment)
	{
		MagAttachment->GetAttachmentMesh()->SetVisibility(false);
	}
}

void AMyFpsWeapon::InsertMagazine()
{
	LeftHandMagazineMesh->SetVisibility(false);
	LeftHandMagazineMesh->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepWorld, false));
	LeftHandMagazineMesh->AttachToComponent(RootComponent, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, false), NAME_None);

	AMyFpsWeaponAttachment* MagAttachment = GetAttachment(EMyFpsAttachmentSlot::Magazine);
	if (MagAttachment)
	{
		MagAttachment->GetAttachmentMesh()->SetVisibility(true);
	}
}
