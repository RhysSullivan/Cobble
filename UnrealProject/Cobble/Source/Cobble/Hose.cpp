// Fill out your copyright notice in the Description page of Project Set	tings.


#include "Hose.h"
#include "CableComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "CobblePaperCharacter.h"
AHose::AHose()
{
	PrimaryActorTick.bCanEverTick = true;
	Cable = CreateDefaultSubobject<UCableComponent>(TEXT("CableComponent"));
	SetRootComponent(Cable);
	Cable->bEnableCollision = true;
	Cable->SetGenerateOverlapEvents(true);
	EndCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("EndCollision"));
	EndCollision->SetupAttachment(GetRootComponent());
	EndCollision->SetBoxExtent(FVector(80, 80, 80));
	EndCollision->SetHiddenInGame(false);
	/*
	Expensive settings with good results
	Cable->SubstepTime = 0.005;
	Cable->NumSegments = 20;
	Cable->SolverIterations = 16;
	Cable->bEnableStiffness = true;
	*/
}

void AHose::Highlight()
{

}

void AHose::Unhighlight()
{
	
}

void AHose::Interact()
{
	Cable->SetAttachEndTo(UGameplayStatics::GetPlayerPawn(GetWorld(), 0), TEXT(""), TEXT(""));
	Cable->bAttachEnd = true;
	ACobblePaperCharacter* Cobble = Cast<ACobblePaperCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	Cobble->SetHeldActor(this);
}

void AHose::Drop()
{
	Cable->bAttachEnd = false;
}

void AHose::Tick(float DeltaSeconds)
{
	TArray<FVector> ParticleLocs{};
	Cable->GetCableParticleLocations(ParticleLocs);
	EndCollision->SetWorldLocation(ParticleLocs[ParticleLocs.Num() - 1]);
	if (Cable->AttachEndTo.OtherActor != nullptr)
	{
		Cable->CableLength = (Cable->AttachEndTo.OtherActor->GetActorLocation() - Cable->GetComponentLocation()).Size() + 50;
	}
}