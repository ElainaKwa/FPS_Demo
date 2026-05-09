// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseWeaponAttachment.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"

ABaseWeaponAttachment::ABaseWeaponAttachment()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	AttachmentMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Attachment Mesh"));
	AttachmentMesh->SetupAttachment(RootComponent);
	AttachmentMesh->SetCollisionProfileName(FName("NoCollision"));
}
