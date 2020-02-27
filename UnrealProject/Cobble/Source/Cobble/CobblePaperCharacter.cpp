// Fill out your copyright notice in the Description page of Project Settings.


#include "CobblePaperCharacter.h"
#include "Components/InputComponent.h"
#include "PaperFlipbookComponent.h"

ACobblePaperCharacter::ACobblePaperCharacter() : Super()
{	
	PrimaryActorTick.bCanEverTick = true;
	auto Flipbook = GetSprite();
	Flipbook->CastShadow = true;
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
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACobblePaperCharacter::DoJump);
}

void ACobblePaperCharacter::MoveHorizontal(float Value)
{
	AddMovementInput(GetActorForwardVector(), Value);
}

void ACobblePaperCharacter::DoJump()
{
	Jump();
}
