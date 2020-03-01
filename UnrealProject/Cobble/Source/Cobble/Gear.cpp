// Fill out your copyright notice in the Description page of Project Settings.


#include "Gear.h"

// Sets default values
AGear::AGear()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	GearSpriteComponent = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("GearSpriteComponent"));
	SetRootComponent(GearSpriteComponent);
	GearHighlightComponent = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("GearHighlight"));
	GearHighlightComponent->SetupAttachment(GearSpriteComponent);
	GearHighlightComponent->SetHiddenInGame(true);
}

void AGear::Highlight()
{
	GearSpriteComponent->SetHiddenInGame(true);
	GearHighlightComponent->SetHiddenInGame(false);
}

void AGear::Unhighlight()
{
	GearSpriteComponent->SetHiddenInGame(false);
	GearHighlightComponent->SetHiddenInGame(true);
}

// Called when the game starts or when spawned
void AGear::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AGear::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

