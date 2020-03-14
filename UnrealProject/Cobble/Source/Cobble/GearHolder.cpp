// Fill out your copyright notice in the Description page of Project Settings.


#include "GearHolder.h"

void AGearHolder::Highlight()
{
	// if the player has a gear
	if (Player != nullptr && Player->IsPlayerHoldingGear() && CanRecieveGear()) // this if statement has early execution on the left side so we wont dereference a nullptr
	{
		HighlightedSpriteComponent->SetHiddenInGame(false); // display gear input highlight
	}
}

void AGearHolder::Unhighlight()
{
	HighlightedSpriteComponent->SetHiddenInGame(true);
}

void AGearHolder::Interact()
{
	if (CanRecieveGear())
	{
		Player->TakeGear(GearInHolder);
		if(GearInHolder != nullptr)
		{
			FTransform GearTransform = HighlightedSpriteComponent->GetComponentTransform();
			GearTransform.SetScale3D(GearInHolder->GetActorScale3D());
			GearInHolder->SetActorTransform(GearTransform);
		}
	}
}

void AGearHolder::BeginPlay()
{
	Super::BeginPlay();
}

void AGearHolder::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (GearInHolder != nullptr)
	{
		GearInHolder->AddActorLocalRotation(GearRotation * DeltaTime);
	}
}

AGearHolder::AGearHolder()
{

}

bool AGearHolder::CanRecieveGear()
{
	return GearInHolder == nullptr;
}
