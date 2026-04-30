// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyFpsCharacter.h"
#include "Weapons/MyFpsWeapon.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

AMyFpsCharacter::AMyFpsCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	bUseControllerRotationPitch = true;
	bUseControllerRotationYaw = true;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 420.0f;
	GetCharacterMovement()->AirControl = 0.2f;

	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->bOwnerNoSee = true;
	GetMesh()->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	FirstPersonMesh->SetupAttachment(GetCapsuleComponent());
	FirstPersonMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	FirstPersonMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	FirstPersonMesh->bOnlyOwnerSee = true;
	FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->bCastHiddenShadow = false;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 10.0f, 64.0f));
	FirstPersonCamera->bUsePawnControlRotation = true;
}

void AMyFpsCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}

	SpawnDefaultWeapon();
	
}

void AMyFpsCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void AMyFpsCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMyFpsCharacter::OnMove);
		}
		if (LookAction)
		{
			EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMyFpsCharacter::OnLook);
		}
		if (JumpAction)
		{
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &AMyFpsCharacter::OnJumpStarted);
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &AMyFpsCharacter::OnJumpEnded);
		}
		if (FireAction)
		{
			EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &AMyFpsCharacter::OnStartFiring);
			EnhancedInput->BindAction(FireAction, ETriggerEvent::Completed, this, &AMyFpsCharacter::OnStopFiring);
		}
		if (ReloadAction)
		{
			EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Triggered, this, &AMyFpsCharacter::OnReload);
		}
		if (SwitchWeaponAction)
		{
			EnhancedInput->BindAction(SwitchWeaponAction, ETriggerEvent::Triggered, this, &AMyFpsCharacter::OnSwitchWeapon);
		}
	}
}

void AMyFpsCharacter::OnMove(const FInputActionValue& Value)
{
	const FVector2D MoveVector = Value.Get<FVector2D>();

	if (Controller)
	{
		const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
		const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDir, MoveVector.Y);
		AddMovementInput(RightDir, MoveVector.X);
	}
}

void AMyFpsCharacter::OnLook(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();

	if (Controller)
	{
		AddControllerYawInput(LookVector.X);
		AddControllerPitchInput(LookVector.Y);
	}
}

void AMyFpsCharacter::OnJumpStarted()
{
	Jump();
}

void AMyFpsCharacter::OnJumpEnded()
{
	StopJumping();
}

void AMyFpsCharacter::OnStartFiring()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->StartFiring();
	}
}

void AMyFpsCharacter::OnStopFiring()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->StopFiring();
	}
}

void AMyFpsCharacter::OnReload()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->StartReload();
	}
}

void AMyFpsCharacter::OnSwitchWeapon()
{
	if (OwnedWeapons.Num() <= 1)
	{
		return;
	}

	int32 Index = OwnedWeapons.Find(CurrentWeapon);
	Index = (Index + 1) % OwnedWeapons.Num();

	SwitchToWeapon(OwnedWeapons[Index]);
}

void AMyFpsCharacter::SpawnDefaultWeapon()
{
	if (DefaultWeaponClass)
	{
		AddWeapon(DefaultWeaponClass);
	}
}

// ---- IMyFpsWeaponHolder ----

void AMyFpsCharacter::AttachWeaponMeshes(AMyFpsWeapon* Weapon)
{
	if (!Weapon)
	{
		return;
	}

	const FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, false);

	Weapon->GetFirstPersonMesh()->AttachToComponent(FirstPersonMesh, AttachRules, FirstPersonWeaponSocket);
	Weapon->GetFirstPersonMesh()->AddLocalRotation(Weapon->FirstPersonMeshRotationOffset);

	Weapon->GetThirdPersonMesh()->AttachToComponent(GetMesh(), AttachRules, ThirdPersonWeaponSocket);
}

FVector AMyFpsCharacter::GetWeaponTargetLocation() const
{
	const FVector Start = FirstPersonCamera->GetComponentLocation();
	const FVector End = Start + (FirstPersonCamera->GetForwardVector() * MaxAimDistance);

	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams);

	return Hit.bBlockingHit ? Hit.ImpactPoint : Hit.TraceEnd;
}

void AMyFpsCharacter::PlayFiringMontage(UAnimMontage* Montage)
{
	if (Montage && FirstPersonMesh->GetAnimInstance())
	{
		FirstPersonMesh->GetAnimInstance()->Montage_Play(Montage);
	}
}

void AMyFpsCharacter::PlayReloadMontage(UAnimMontage* Montage)
{
	if (Montage && FirstPersonMesh->GetAnimInstance())
	{
		FirstPersonMesh->GetAnimInstance()->Montage_Play(Montage);
	}
}

void AMyFpsCharacter::AddWeaponRecoil(float RecoilAmount)
{
	AddControllerPitchInput(RecoilAmount);
}

void AMyFpsCharacter::UpdateWeaponHUD(int32 CurrentAmmo, int32 MaxAmmo)
{
	OnBulletCountUpdated.Broadcast(CurrentAmmo, MaxAmmo);
}

// ---- Weapon Management ----

void AMyFpsCharacter::AddWeapon(TSubclassOf<AMyFpsWeapon> WeaponClass)
{
	if (!WeaponClass)
	{
		return;
	}

	AMyFpsWeapon* Existing = FindWeaponOfClass(WeaponClass);
	if (Existing)
	{
		SwitchToWeapon(Existing);
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AMyFpsWeapon* NewWeapon = GetWorld()->SpawnActor<AMyFpsWeapon>(WeaponClass, GetActorTransform(), SpawnParams);
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
}

void AMyFpsCharacter::SwitchToWeapon(AMyFpsWeapon* NewWeapon)
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
}

AMyFpsWeapon* AMyFpsCharacter::FindWeaponOfClass(TSubclassOf<AMyFpsWeapon> WeaponClass) const
{
	for (AMyFpsWeapon* Weapon : OwnedWeapons)
	{
		if (Weapon && Weapon->IsA(WeaponClass))
		{
			return Weapon;
		}
	}
	return nullptr;
}
