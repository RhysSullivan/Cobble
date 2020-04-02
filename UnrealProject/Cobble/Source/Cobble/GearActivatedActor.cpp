// Fill out your copyright notice in the Description page of Project Settings.


#include "GearActivatedActor.h"
#include "Cobble//GearHolder.h"
#include "Components/ChildActorComponent.h"

// Sets default values
AGearActivatedActor::AGearActivatedActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);
	GearHolderActor = CreateDefaultSubobject<UChildActorComponent>(TEXT("GearHolderChildActor"));

	GearHolderActor->SetupAttachment(SceneRoot);
	GearHolderActor->SetChildActorClass(AGearHolder::StaticClass());
}

void AGearActivatedActor::OnConstruction(const FTransform & Transform)
{
	Super::OnConstruction(Transform);
}

// Called when the game starts or when spawned
void AGearActivatedActor::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AGearActivatedActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

bool AGearActivatedActor::IsPowered()
{
	return Cast<AGearHolder>(GearHolderActor->GetChildActor())->GetIsGearTurning();
}

