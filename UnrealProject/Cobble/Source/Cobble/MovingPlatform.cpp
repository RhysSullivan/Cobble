// Fill out your copyright notice in the Description page of Project Settings.


#include "MovingPlatform.h"
#include "Components/StaticMeshComponent.h"
AMovingPlatform::AMovingPlatform()
{
	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Platform"));
	PlatformMesh->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	GearHolderActor->AttachToComponent(PlatformMesh, FAttachmentTransformRules::KeepWorldTransform);
}

void AMovingPlatform::BeginPlay()
{
	Super::BeginPlay();
}

void AMovingPlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}