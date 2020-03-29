// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InteractInterface.h"
#include "GameFramework/Actor.h"
#include "Hose.generated.h"

/**
 * 
 */
UCLASS()
class COBBLE_API AHose : public AActor, public IInteractInterface
{
	GENERATED_BODY()

public:
	AHose();

	virtual void Highlight() override;
	virtual void Unhighlight() override;
	virtual void Interact() override;
	virtual void Tick(float DeltaSeconds) override;
	/** Cable component that performs simulation and rendering */
	UPROPERTY(Category = Cable, VisibleAnywhere, BlueprintReadWrite)
	class UCableComponent* Cable;
	class UBoxComponent* EndCollision;

	UPROPERTY(EditAnywhere)
	float CableStartOffset = 300;
	
};
