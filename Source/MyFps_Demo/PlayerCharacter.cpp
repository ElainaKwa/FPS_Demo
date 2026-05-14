// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerCharacter.h"
#include "Weapons/BaseWeapon.h"
#include "GameAbilitySystem/BaseAbilitySystemComponent.h"
#include "GameAbilitySystem/BaseStaminaAttributeSet.h"
#include "GameAbilitySystem/BaseHealthAttributeSet.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

APlayerCharacter::APlayerCharacter()
{
	bUseControllerRotationPitch = true;
	bUseControllerRotationYaw = true;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 420.0f;
	GetCharacterMovement()->AirControl = 0.2f;

	GetMesh()->bOwnerNoSee = true;
	GetMesh()->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(GetCapsuleComponent());
	FirstPersonMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	FirstPersonMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	FirstPersonMesh->bOnlyOwnerSee = true;
	FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->bCastHiddenShadow = false;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 10.0f, 64.0f));
	FirstPersonCamera->bUsePawnControlRotation = true;

	StaminaAttributeSet = CreateDefaultSubobject<UBaseStaminaAttributeSet>(TEXT("StaminaAttributeSet"));
}

void APlayerCharacter::BeginPlay()
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
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::OnMove);
		}
		if (LookAction)
		{
			EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &APlayerCharacter::OnLook);
		}
		if (JumpAction)
		{
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &APlayerCharacter::OnJumpStarted);
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &APlayerCharacter::OnJumpEnded);
		}
		if (FireAction)
		{
			EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &APlayerCharacter::OnStartFiring);
			EnhancedInput->BindAction(FireAction, ETriggerEvent::Completed, this, &APlayerCharacter::OnStopFiring);
		}
		if (ReloadAction)
		{
			EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Triggered, this, &APlayerCharacter::OnReload);
		}
		if (SwitchWeaponAction)
		{
			EnhancedInput->BindAction(SwitchWeaponAction, ETriggerEvent::Triggered, this, &APlayerCharacter::OnSwitchWeapon);
		}
	}
}

void APlayerCharacter::OnMove(const FInputActionValue& Value)
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

void APlayerCharacter::OnLook(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();

	if (Controller)
	{
		AddControllerYawInput(LookVector.X);
		AddControllerPitchInput(LookVector.Y);
	}
}

void APlayerCharacter::OnJumpStarted()
{
	Jump();
}

void APlayerCharacter::OnJumpEnded()
{
	StopJumping();
}

void APlayerCharacter::OnStartFiring()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->TryActivateAbility(AbilitySystemComponent->GetFireAbilityHandle());
	}
}

void APlayerCharacter::OnStopFiring()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityLocalInputReleased(static_cast<int32>(EAbilityInputID::Fire));
	}
}

void APlayerCharacter::OnReload()
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

void APlayerCharacter::OnSwitchWeapon()
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

void APlayerCharacter::AttachWeaponMeshes(ABaseWeapon* Weapon)
{
	Super::AttachWeaponMeshes(Weapon);

	if (!Weapon)
	{
		return;
	}

	Weapon->GetFirstPersonMesh()->SetVisibility(true);

	const FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, false);

	Weapon->GetFirstPersonMesh()->AttachToComponent(FirstPersonMesh, AttachRules, FirstPersonWeaponSocket);
	Weapon->GetFirstPersonMesh()->AddLocalRotation(Weapon->FirstPersonMeshRotationOffset);
}

FVector APlayerCharacter::GetWeaponTargetLocation() const
{
	const FVector Start = FirstPersonCamera->GetComponentLocation();
	const FVector End = Start + (FirstPersonCamera->GetForwardVector() * MaxAimDistance);

	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams);

	return Hit.bBlockingHit ? Hit.ImpactPoint : Hit.TraceEnd;
}

void APlayerCharacter::PlayFiringMontage(UAnimMontage* Montage)
{
	if (Montage && FirstPersonMesh->GetAnimInstance())
	{
		FirstPersonMesh->GetAnimInstance()->Montage_Play(Montage);
	}
}

void APlayerCharacter::PlayReloadMontage(UAnimMontage* Montage)
{
	if (Montage && FirstPersonMesh->GetAnimInstance())
	{
		FirstPersonMesh->GetAnimInstance()->Montage_Play(Montage);
	}
}

void APlayerCharacter::AddWeaponRecoil(float RecoilAmount)
{
	AddControllerPitchInput(RecoilAmount);
}

void APlayerCharacter::UpdateHealthHUD()
{
	if (HealthAttributeSet)
	{
		OnHealthUpdated.Broadcast(HealthAttributeSet->GetHealth(), HealthAttributeSet->GetMaxHealth());
	}
}

void APlayerCharacter::OnDeath()
{
	Super::OnDeath();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->DisableInput(PC);
	}
}

void APlayerCharacter::UpdateStaminaHUD()
{
	if (StaminaAttributeSet)
	{
		OnStaminaUpdated.Broadcast(StaminaAttributeSet->GetStamina(), StaminaAttributeSet->GetMaxStamina());
	}
}
