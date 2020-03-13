// Fill out your copyright notice in the Description page of Project Settings.


#include "Gear.h"

// Sets default values
AGear::AGear()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

void AGear::Highlight()
{
	if (Player != nullptr && !Player->IsPlayerHoldingGear())
	{
		Player->ShowGearHighlight();
		RegularSpriteComponent->SetHiddenInGame(true);
	}
}

void AGear::Unhighlight()
{
	RegularSpriteComponent->SetHiddenInGame(false);
	if (Player != nullptr)
	{
		Player->HideGearHighlight();
	}
}

void AGear::Interact()
{
	if (Player != nullptr)
	{
		if (Player->ReceiveGear(this))
		{
			SetActorLocation(FVector(0, -40000, 0));
		}
	}
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

