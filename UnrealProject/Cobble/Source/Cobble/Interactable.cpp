// Fill out your copyright notice in the Description page of Project Settings.


#include "Interactable.h"

// Sets default values
AInteractable::AInteractable()
{
	PrimaryActorTick.bCanEverTick = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Scene Root"));
	SetRootComponent(SceneRoot);
	
	RegularSpriteComponent = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Regular Sprite"));
	RegularSpriteComponent->SetCollisionProfileName("OverlapAll");
	RegularSpriteComponent->SetupAttachment(SceneRoot);

	HighlightedSpriteComponent = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Highlighted Sprite"));
	HighlightedSpriteComponent->SetupAttachment(SceneRoot);
	HighlightedSpriteComponent->SetHiddenInGame(true);
	HighlightedSpriteComponent->SetCollisionProfileName("OverlapAll");
}

// Called when the game starts or when spawned
void AInteractable::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AInteractable::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AInteractable::Highlight()
{
	RegularSpriteComponent->SetHiddenInGame(true);
	HighlightedSpriteComponent->SetHiddenInGame(false);
}

void AInteractable::Unhighlight()
{
	RegularSpriteComponent->SetHiddenInGame(false);
	HighlightedSpriteComponent->SetHiddenInGame(true);
}

