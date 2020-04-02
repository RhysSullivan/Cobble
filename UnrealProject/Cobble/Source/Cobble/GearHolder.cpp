// Fill out your copyright notice in the Description page of Project Settings.


#include "GearHolder.h"

void AGearHolder::Highlight()
{
	// if the player has a gear
	if (Player != nullptr) // this if statement has early execution on the left side so we wont dereference a nullptr
	{
		if (HasGearInHolder())
		{
			if (!Player->IsPlayerHoldingGear())
			{
				Player->ShowGearHighlight();
			}
		}
		else if(Player->IsPlayerHoldingGear())
		{
			HighlightedSpriteComponent->SetHiddenInGame(false); // display gear input highlight
		}
	}
}

void AGearHolder::Unhighlight()
{
	if (HasGearInHolder())
	{
		if (Player != nullptr && !Player->IsPlayerHoldingGear())
		{
			Player->HideGearHighlight();
		}
	}
	else
	{
		HighlightedSpriteComponent->SetHiddenInGame(true);
	}
}

void AGearHolder::Interact()
{
	if (HasGearInHolder())
	{
		if (Player != nullptr)
		{
			if (Player->ReceiveGear(GearInHolder))
			{
				GearInHolder->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				GearInHolder->SetActorLocation(FVector(0, -40000, 0));
				GearInHolder = nullptr;
				Player->HideGearHighlight();
			}	
		}
	}
	else
	{
		Player->TakeGear(GearInHolder);
		if (GearInHolder != nullptr)
		{
			GearInHolder->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			FTransform GearTransform = HighlightedSpriteComponent->GetComponentTransform();
			GearTransform.SetScale3D(GearInHolder->GetActorScale3D());
			GearInHolder->SetActorTransform(GearTransform);
			HighlightedSpriteComponent->SetHiddenInGame(true);
			bIsGearTurning = true;
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
	if (GetIsGearTurning())
	{
		GearInHolder->AddActorLocalRotation(GearRotation * DeltaTime);
	}
}

AGearHolder::AGearHolder()
{

}

bool AGearHolder::HasGearInHolder()
{
	return GearInHolder != nullptr;
}

bool AGearHolder::GetIsGearTurning()
{
	if(GearInHolder == nullptr)
		return false;
	return bIsGearTurning;
	
}