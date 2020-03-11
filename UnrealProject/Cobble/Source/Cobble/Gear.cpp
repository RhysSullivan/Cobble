// Fill out your copyright notice in the Description page of Project Settings.


#include "Gear.h"
#include "Kismet/GameplayStatics.h"
#include "Cobble/CobblePaperCharacter.h"

// Sets default values
AGear::AGear()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

void AGear::Highlight()
{
	ACobblePaperCharacter* Player = Cast<ACobblePaperCharacter>(GetWorld()->GetFirstPlayerController()->GetPawn());
	if (Player != nullptr && !Player->IsPlayerHoldingGear())
	{
		Player->ShowGearHighlight();
		RegularSpriteComponent->SetHiddenInGame(true);
	}
	//GearHighlightComponent->SetHiddenInGame(false);
}

void AGear::Unhighlight()
{
	RegularSpriteComponent->SetHiddenInGame(false);
	ACobblePaperCharacter* Player = Cast<ACobblePaperCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (Player != nullptr)
	{
		Player->HideGearHighlight();
	}
	//GearHighlightComponent->SetHiddenInGame(true);
}

void AGear::Interact()
{
	ACobblePaperCharacter* Player = Cast<ACobblePaperCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (Player != nullptr)
	{
		if (Player->ReceiveGear(this))
		{
			UE_LOG(LogTemp, Warning, TEXT("PICKUP"));
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

