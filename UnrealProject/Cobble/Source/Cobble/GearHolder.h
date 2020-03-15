// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interactable.h"
#include "GearHolder.generated.h"

/**
 * 
 */
UCLASS()
class COBBLE_API AGearHolder : public AInteractable
{
	GENERATED_BODY()
	

/*
Interact Interface 
*/
public:
	virtual void Highlight() override;
	virtual void Unhighlight() override;
	virtual void Interact() override;

public:
	UPROPERTY(EditAnywhere)
	FRotator GearRotation = FRotator(-200,0,0);
protected:
	virtual void BeginPlay() override; 	// Called when the game starts or when spawned

public:
	virtual void Tick(float DeltaTime) override; 	// Called every frame
	AGearHolder();	// Sets default values for this actor's properties
private:
	AActor* GearInHolder = nullptr;

private:
	bool HasGearInHolder();
};
