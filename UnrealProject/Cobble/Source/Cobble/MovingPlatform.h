// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GearActivatedActor.h"
#include "MovingPlatform.generated.h"

/**
 * 
 */
UCLASS()
class COBBLE_API AMovingPlatform : public AGearActivatedActor
{
	GENERATED_BODY()
public:
	// Sets default values for this actor's properties
	AMovingPlatform();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
private:
	UPROPERTY(VisibleAnywhere)
	class UStaticMeshComponent* PlatformMesh;
};
