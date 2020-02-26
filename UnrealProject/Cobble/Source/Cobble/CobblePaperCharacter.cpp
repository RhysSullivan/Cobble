// Fill out your copyright notice in the Description page of Project Settings.


#include "CobblePaperCharacter.h"

#include "Components/InputComponent.h"

ACobblePaperCharacter::ACobblePaperCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ACobblePaperCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ACobblePaperCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ACobblePaperCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerInputComponent->BindAxis("MoveHorizontal", this, &ACobblePaperCharacter::MoveHorizontal);
}

void ACobblePaperCharacter::MoveHorizontal(float Value)
{
	AddMovementInput(GetActorForwardVector(), Value);
}
