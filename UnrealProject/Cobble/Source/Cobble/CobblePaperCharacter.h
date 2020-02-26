// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperCharacter.h"
#include "CobblePaperCharacter.generated.h"

/**
 * 
 */
UCLASS()
class COBBLE_API ACobblePaperCharacter : public APaperCharacter
{
	GENERATED_BODY()
public:
	// Sets default values for this character's properties
	ACobblePaperCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	void MoveHorizontal(float Value);
};
