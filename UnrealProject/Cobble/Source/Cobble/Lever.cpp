// Fill out your copyright notice in the Description page of Project Settings.


#include "Lever.h"

ALever::ALever()
{
	LeftPlaceholder = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("PlaceholderLeft"));
	LeftPlaceholder->SetupAttachment(GetRootComponent());
	RightPlaceholder = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("PlaceholderRight"));
	RightPlaceholder->SetupAttachment(GetRootComponent());
	RightPlaceholder->SetHiddenInGame(true);
	LeftPlaceholder->SetHiddenInGame(true);
	LeftPlaceholder->SetCollisionProfileName("OverlapAll");
	RightPlaceholder->SetCollisionProfileName("OverlapAll");
}

void ALever::Highlight()
{
	Super::Highlight();
}

void ALever::Unhighlight()
{
	Super::Unhighlight();
}

void ALever::Interact()
{
	Super::Interact();
	if (bIsFlippedToTheLeft)
	{
		bIsFlippedToTheLeft = false;
	}
	else
	{
		bIsFlippedToTheLeft = true;
	}
	RotateToMatchFlippedDirection();
}

void ALever::OnConstruction(const FTransform & Transform)
{
	RotateToMatchFlippedDirection();
}

void ALever::RotateToMatchFlippedDirection()
{
	if (bIsFlippedToTheLeft)
	{
		HighlightedSpriteComponent->SetRelativeTransform(LeftPlaceholder->GetRelativeTransform());
		RegularSpriteComponent->SetRelativeTransform(LeftPlaceholder->GetRelativeTransform());
	}
	else
	{
		HighlightedSpriteComponent->SetRelativeTransform(RightPlaceholder->GetRelativeTransform());
		RegularSpriteComponent->SetRelativeTransform(RightPlaceholder->GetRelativeTransform());
	}
}
