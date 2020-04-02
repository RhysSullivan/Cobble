// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GearActivatedActor.generated.h"

UCLASS()
class COBBLE_API AGearActivatedActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AGearActivatedActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform & Transform) override;
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	bool IsPowered();
protected:
	UPROPERTY(VisibleDefaultsOnly)
	class UChildActorComponent* GearHolderActor;
	UPROPERTY(VisibleDefaultsOnly)
	class USceneComponent* SceneRoot;
};
