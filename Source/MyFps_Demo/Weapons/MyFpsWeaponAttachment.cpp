// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyFpsWeaponAttachment.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"

AMyFpsWeaponAttachment::AMyFpsWeaponAttachment()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	AttachmentMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Attachment Mesh"));
	AttachmentMesh->SetupAttachment(RootComponent);
	AttachmentMesh->SetCollisionProfileName(FName("NoCollision"));
}
