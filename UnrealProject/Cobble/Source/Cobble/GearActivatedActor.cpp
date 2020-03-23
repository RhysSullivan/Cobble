// Fill out your copyright notice in the Description page of Project Settings.


#include "GearActivatedActor.h"
#include "Cobble//GearHolder.h"
#include "Components/ChildActorComponent.h"
// Sets default values
AGearActivatedActor::AGearActivatedActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultRoot"));
	SetRootComponent(SceneRoot);
	GearHolderActor = CreateDefaultSubobject<UChildActorComponent>(TEXT("InventoryCamera"));
	GearHolderActor->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepWorldTransform);
	GearHolderActor->SetChildActorClass(AGearHolder::StaticClass());
	
	
	
}

// Called when the game starts or when spawned
void AGearActivatedActor::BeginPlay()
{
	Super::BeginPlay();
	GearHolderActor->CreateChildActor();
}

// Called every frame
void AGearActivatedActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

