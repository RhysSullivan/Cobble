// Fill out your copyright notice in the Description page of Project Settings.


#include "Lever.h"
#include "Hose.h"
#include "CableComponent.h"

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

	LeftHose = CreateDefaultSubobject<UChildActorComponent>(TEXT("GearHolderChildActor"));

	LeftHose->SetupAttachment(SceneRoot);
	LeftHose->SetChildActorClass(AHose::StaticClass());
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

void ALever::BeginPlay()
{
	AHose* Hose = Cast<AHose>(LeftHose->GetChildActor());
	if (Hose != nullptr)
	{
		Hose->Cable->bAttachEnd = false;
		Hose->Cable->EndLocation = FVector::ZeroVector;
	}
}

void ALever::OnConstruction(const FTransform & Transform)
{
	AHose* Hose = Cast<AHose>(LeftHose->GetChildActor());
	if (Hose != nullptr)
	{
		//Hose->SetActorLocation(GetActorLocation() + FVector(0, 0, 10));
		Hose->Cable->EndLocation = GetActorForwardVector()* -Hose->CableStartOffset;
		Hose->Cable->bAttachEnd = true;
		//Hose->Cable->EndLocation += FVector(0, 0, 40);
	}
	RotateToMatchFlippedDirection();
}

void ALever::RotateToMatchFlippedDirection()
{
	if (bIsFlippedToTheLeft)
	{
		HighlightedSpriteComponent->SetRelativeTransform(LeftPlaceholder->GetRelativeTransform());
		RegularSpriteComponent->SetRelativeTransform(LeftPlaceholder->GetRelativeTransform());
		AHose* Hose = Cast<AHose>(LeftHose->GetChildActor());
		if (Hose != nullptr)
		{
			Hose->Cable->CableGravityScale = -1;
		}
	}
	else
	{
		AHose* Hose = Cast<AHose>(LeftHose->GetChildActor());
		if (Hose != nullptr)
		{
			Hose->Cable->CableGravityScale = 1;
		}
		HighlightedSpriteComponent->SetRelativeTransform(RightPlaceholder->GetRelativeTransform());
		RegularSpriteComponent->SetRelativeTransform(RightPlaceholder->GetRelativeTransform());
	}
}
