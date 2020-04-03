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
	UPROPERTY(EditAnywhere)
	float MovementSpeed = 100;
	UPROPERTY(EditAnywhere)
	float TimeToWaitAtEndPoint = 3;
	UPROPERTY(EditAnywhere)
	bool bIsStraightPath = false;
	// Sets default values for this actor's properties
	AMovingPlatform();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	void OnConstruction(const FTransform& Transform) override;
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
private:
	UPROPERTY(VisibleAnywhere)
	class UStaticMeshComponent* PlatformMesh;
	UPROPERTY(VisibleAnywhere)
	class USplineComponent* MovingPlatformPath;

/*Spline variables*/
protected:
	float AmountOfSplineTraversed = 0;
	int CurrentPoint = 0;
	
	float Direction = 1;
	bool bReverse = false;
	bool bIsWaiting = false;
	float TimeWaited = 0;
};
