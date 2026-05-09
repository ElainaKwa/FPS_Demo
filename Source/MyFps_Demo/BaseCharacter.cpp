// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseCharacter.h"
#include "Weapons/BaseWeapon.h"
#include "GameAbilitySystem/BaseAbilitySystemComponent.h"
#include "GameAbilitySystem/BaseWeaponAttributeSet.h"
#include "GameAbilitySystem/BaseGameplayTags.h"
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
#include "AbilitySystemComponent.h"

ABaseCharacter::ABaseCharacter()
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

	AbilitySystemComponent = CreateDefaultSubobject<UBaseAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	WeaponAttributeSet = CreateDefaultSubobject<UBaseWeaponAttributeSet>(TEXT("WeaponAttributeSet"));
}

void ABaseCharacter::BeginPlay()
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

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		AbilitySystemComponent->GrantDefaultAbilities();
	}

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

void ABaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABaseCharacter::OnMove);
		}
		if (LookAction)
		{
			EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABaseCharacter::OnLook);
		}
		if (JumpAction)
		{
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ABaseCharacter::OnJumpStarted);
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ABaseCharacter::OnJumpEnded);
		}
		if (FireAction)
		{
			EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &ABaseCharacter::OnStartFiring);
			EnhancedInput->BindAction(FireAction, ETriggerEvent::Completed, this, &ABaseCharacter::OnStopFiring);
		}
		if (ReloadAction)
		{
			EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Triggered, this, &ABaseCharacter::OnReload);
		}
		if (SwitchWeaponAction)
		{
			EnhancedInput->BindAction(SwitchWeaponAction, ETriggerEvent::Triggered, this, &ABaseCharacter::OnSwitchWeapon);
		}
	}
}

void ABaseCharacter::OnMove(const FInputActionValue& Value)
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

void ABaseCharacter::OnLook(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();

	if (Controller)
	{
		AddControllerYawInput(LookVector.X);
		AddControllerPitchInput(LookVector.Y);
	}
}

void ABaseCharacter::OnJumpStarted()
{
	Jump();
}

void ABaseCharacter::OnJumpEnded()
{
	StopJumping();
}

void ABaseCharacter::OnStartFiring()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->TryActivateAbility(AbilitySystemComponent->GetFireAbilityHandle());
	}
}

void ABaseCharacter::OnStopFiring()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityLocalInputReleased(static_cast<int32>(EAbilityInputID::Fire));
	}
}

void ABaseCharacter::OnReload()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->CancelFireAbility();

	FGameplayAbilitySpecHandle Handle = AbilitySystemComponent->GetReloadAbilityHandle();
	if (!Handle.IsValid())
	{
		AbilitySystemComponent->GrantDefaultAbilities();
		Handle = AbilitySystemComponent->GetReloadAbilityHandle();
	}

	if (Handle.IsValid())
	{
		AbilitySystemComponent->TryActivateAbility(Handle);
	}
}

void ABaseCharacter::OnSwitchWeapon()
{
	if (OwnedWeapons.Num() <= 1)
	{
		return;
	}

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();
	}

	int32 Index = OwnedWeapons.Find(CurrentWeapon);
	Index = (Index + 1) % OwnedWeapons.Num();

	SwitchToWeapon(OwnedWeapons[Index]);
}

void ABaseCharacter::SpawnDefaultWeapon()
{
	if (DefaultWeaponClass)
	{
		AddWeapon(DefaultWeaponClass);
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

// ---- IBaseWeaponHolder ----

void ABaseCharacter::AttachWeaponMeshes(ABaseWeapon* Weapon)
{
	if (!Weapon)
	{
		return;
	}

	const FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, false);

	Weapon->GetFirstPersonMesh()->AttachToComponent(FirstPersonMesh, AttachRules, FirstPersonWeaponSocket);
	Weapon->GetFirstPersonMesh()->AddLocalRotation(Weapon->FirstPersonMeshRotationOffset);

	if (GetMesh()->GetSkeletalMeshAsset())
	{
		Weapon->GetThirdPersonMesh()->AttachToComponent(GetMesh(), AttachRules, ThirdPersonWeaponSocket);
	}
	else
	{
		Weapon->GetThirdPersonMesh()->AttachToComponent(GetMesh(), AttachRules);
	}
}

FVector ABaseCharacter::GetWeaponTargetLocation() const
{
	const FVector Start = FirstPersonCamera->GetComponentLocation();
	const FVector End = Start + (FirstPersonCamera->GetForwardVector() * MaxAimDistance);

	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams);

	return Hit.bBlockingHit ? Hit.ImpactPoint : Hit.TraceEnd;
}

void ABaseCharacter::PlayFiringMontage(UAnimMontage* Montage)
{
	if (Montage && FirstPersonMesh->GetAnimInstance())
	{
		FirstPersonMesh->GetAnimInstance()->Montage_Play(Montage);
	}
}

void ABaseCharacter::PlayReloadMontage(UAnimMontage* Montage)
{
	if (Montage && FirstPersonMesh->GetAnimInstance())
	{
		FirstPersonMesh->GetAnimInstance()->Montage_Play(Montage);
	}
}

void ABaseCharacter::AddWeaponRecoil(float RecoilAmount)
{
	AddControllerPitchInput(RecoilAmount);
}

void ABaseCharacter::UpdateWeaponHUD(int32 CurrentAmmo, int32 MaxAmmo)
{
	OnBulletCountUpdated.Broadcast(CurrentAmmo, MaxAmmo);
}

// ---- Weapon Management ----

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
