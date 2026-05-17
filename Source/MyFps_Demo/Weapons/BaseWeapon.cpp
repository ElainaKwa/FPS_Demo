// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseWeapon.h"
#include "BaseWeaponAttachment.h"
#include "BaseDroppedMagazine.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DamageType.h"
#include "Kismet/KismetMathLibrary.h"
#include "Animation/AnimInstance.h"
#include "Engine/DamageEvents.h"
#include "Net/UnrealNetwork.h"

ABaseWeapon::ABaseWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

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

void ABaseWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABaseWeapon, CurrentBullets);
}

void ABaseWeapon::OnRep_CurrentBullets()
{
	UE_LOG(LogTemp, Warning, TEXT("[OnRep] Weapon::CurrentBullets 复制: %d"), CurrentBullets);

	if (!WeaponOwner)
	{
		WeaponOwner = Cast<IBaseWeaponHolder>(GetOwner());
	}
	if (WeaponOwner)
	{
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, GetEffectiveMagazineSize());
	}
}

void ABaseWeapon::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner())
	{
		GetOwner()->OnDestroyed.AddDynamic(this, &ABaseWeapon::OnOwnerDestroyed);
	}

	WeaponOwner = Cast<IBaseWeaponHolder>(GetOwner());

	CurrentBullets = MagazineSize;

	if (WeaponOwner)
	{
		WeaponOwner->AttachWeaponMeshes(this);
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, GetEffectiveMagazineSize());
	}
}

void ABaseWeapon::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	ClearAllAttachments();
}

void ABaseWeapon::OnOwnerDestroyed(AActor* DestroyedActor)
{
	Destroy();
}

void ABaseWeapon::ActivateWeapon()
{
	SetActorHiddenInGame(false);
	bIsReloading = false;

	if (WeaponOwner)
	{
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, GetEffectiveMagazineSize());
	}
}

void ABaseWeapon::DeactivateWeapon()
{
	SetActorHiddenInGame(true);
}

void ABaseWeapon::DropToGround()
{
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->OnDestroyed.RemoveDynamic(this, &ABaseWeapon::OnOwnerDestroyed);
		SetOwner(nullptr);
	}

	WeaponOwner = nullptr;

	if (HasAuthority())
	{
		SetLifeSpan(60.0f);
	}

	MulticastDropToGroundVisuals();
}

void ABaseWeapon::MulticastDropToGroundVisuals_Implementation()
{
	FirstPersonMesh->SetVisibility(false);

	ThirdPersonMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	ThirdPersonMesh->SetSimulatePhysics(true);
	ThirdPersonMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ThirdPersonMesh->SetCollisionProfileName(FName("PhysicsActor"));
	ThirdPersonMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}

FVector ABaseWeapon::GetMuzzleLocation() const
{
	return FirstPersonMesh->GetSocketLocation(MuzzleSocketName);
}

FVector ABaseWeapon::CalculateSpread(const FVector& AimDirection) const
{
	const float Variance = GetEffectiveAimVariance();
	if (Variance <= 0.0f)
	{
		return AimDirection;
	}

	return UKismetMathLibrary::RandomUnitVectorInConeInDegrees(AimDirection, Variance);
}

FName ABaseWeapon::GetSocketNameForSlot(EBaseAttachmentSlot Slot) const
{
	switch (Slot)
	{
	case EBaseAttachmentSlot::Muzzle:		return MuzzleSocket;
	case EBaseAttachmentSlot::Optic:		return OpticSocket;
	case EBaseAttachmentSlot::Tactical:	return TacticalSocket;
	case EBaseAttachmentSlot::Foregrip:	return ForegripSocket;
	case EBaseAttachmentSlot::Magazine:	return MagazineSocket;
	default:								return NAME_None;
	}
}

int32 ABaseWeapon::GetMaxAmmo() const
{
	return GetEffectiveMagazineSize();
}

// ---- Attachment System ----

void ABaseWeapon::EquipAttachment(EBaseAttachmentSlot Slot, TSubclassOf<ABaseWeaponAttachment> AttachmentClass)
{
	if (!AttachmentClass)
	{
		return;
	}

	RemoveAttachment(Slot);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABaseWeaponAttachment* NewAttachment = GetWorld()->SpawnActor<ABaseWeaponAttachment>(AttachmentClass, SpawnParams);
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

void ABaseWeapon::RemoveAttachment(EBaseAttachmentSlot Slot)
{
	TObjectPtr<ABaseWeaponAttachment> Existing;
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

ABaseWeaponAttachment* ABaseWeapon::GetAttachment(EBaseAttachmentSlot Slot) const
{
	const TObjectPtr<ABaseWeaponAttachment>* Found = EquippedAttachments.Find(Slot);
	return Found ? Found->Get() : nullptr;
}

bool ABaseWeapon::HasAttachment(EBaseAttachmentSlot Slot) const
{
	return EquippedAttachments.Contains(Slot);
}

void ABaseWeapon::ClearAllAttachments()
{
	TArray<EBaseAttachmentSlot> Slots;
	EquippedAttachments.GetKeys(Slots);
	for (const EBaseAttachmentSlot& Slot : Slots)
	{
		RemoveAttachment(Slot);
	}
}

float ABaseWeapon::GetEffectiveDamage() const
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

float ABaseWeapon::GetEffectiveRecoil() const
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

float ABaseWeapon::GetEffectiveAimVariance() const
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

float ABaseWeapon::GetEffectiveReloadTime() const
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

int32 ABaseWeapon::GetEffectiveMagazineSize() const
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

// ---- Reload Helpers ----

void ABaseWeapon::DropMagazine()
{
	const FName SocketName = GetSocketNameForSlot(EBaseAttachmentSlot::Magazine);
	const FVector SpawnLocation = FirstPersonMesh->GetSocketLocation(SocketName);
	const FVector SpawnVelocity = FirstPersonMesh->GetComponentVelocity() + FVector(FMath::FRandRange(-50.0f, 50.0f), FMath::FRandRange(-50.0f, 50.0f), FMath::FRandRange(-100.0f, -50.0f));

	UStaticMesh* MeshToDrop = DroppedMagazineMesh;

	ABaseWeaponAttachment* MagAttachment = GetAttachment(EBaseAttachmentSlot::Magazine);
	if (MagAttachment && MagAttachment->GetAttachmentMesh()->GetStaticMesh())
	{
		MeshToDrop = MagAttachment->GetAttachmentMesh()->GetStaticMesh();
	}

	if (MeshToDrop && HasAuthority())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ABaseDroppedMagazine* DroppedMag = GetWorld()->SpawnActor<ABaseDroppedMagazine>(
			ABaseDroppedMagazine::StaticClass(), SpawnLocation, FRotator::ZeroRotator, SpawnParams);

		if (DroppedMag)
		{
			DroppedMag->Initialize(MeshToDrop, SpawnVelocity);
			DroppedMag->GetMagazineMesh()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		}
	}

	MulticastDropMagazineVisuals(MeshToDrop);
}

void ABaseWeapon::InsertMagazine()
{
	MulticastInsertMagazineVisuals();
}

void ABaseWeapon::CancelReloadVisuals()
{
	MulticastCancelReloadVisuals();
}

void ABaseWeapon::MulticastDropMagazineVisuals_Implementation(UStaticMesh* Mesh)
{
	USceneComponent* ArmMesh = FirstPersonMesh->GetAttachParent();
	if (ArmMesh && Mesh)
	{
		LeftHandMagazineMesh->SetStaticMesh(Mesh);
		LeftHandMagazineMesh->SetVisibility(true);

		const FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, false);
		LeftHandMagazineMesh->AttachToComponent(ArmMesh, AttachRules, LeftHandSocketName);
	}

	ABaseWeaponAttachment* MagAttachment = GetAttachment(EBaseAttachmentSlot::Magazine);
	if (MagAttachment)
	{
		MagAttachment->GetAttachmentMesh()->SetVisibility(false);
	}
}

void ABaseWeapon::MulticastInsertMagazineVisuals_Implementation()
{
	LeftHandMagazineMesh->SetVisibility(false);
	LeftHandMagazineMesh->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepWorld, false));
	LeftHandMagazineMesh->AttachToComponent(RootComponent, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, false), NAME_None);

	ABaseWeaponAttachment* MagAttachment = GetAttachment(EBaseAttachmentSlot::Magazine);
	if (MagAttachment)
	{
		MagAttachment->GetAttachmentMesh()->SetVisibility(true);
	}
}

void ABaseWeapon::MulticastCancelReloadVisuals_Implementation()
{
	LeftHandMagazineMesh->SetVisibility(false);
	LeftHandMagazineMesh->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepWorld, false));
	LeftHandMagazineMesh->AttachToComponent(RootComponent, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, false), NAME_None);
}
