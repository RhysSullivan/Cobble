// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Cobble/Interactable.h"
#include "Gear.generated.h"

UCLASS()
class COBBLE_API AGear : public AInteractable
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AGear();
	virtual void Highlight() override;
	virtual void Unhighlight() override;
	virtual void Interact() override;
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
private:
};
