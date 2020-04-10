// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InteractInterface.h"
#include "GameFramework/Actor.h"
#include "PickupInterface.h"
#include "Hose.generated.h"

/**
 *
 */
UCLASS()
class COBBLE_API AHose : public AActor, public IInteractInterface, public IPickupInterface
{
	GENERATED_BODY()

public:
	AHose();
	//Interact Interface
	virtual void Highlight() override;
	virtual void Unhighlight() override;
	virtual void Interact() override;
	// Pickup Interface
	virtual void Drop() override;


	virtual void Tick(float DeltaSeconds) override;
	/** Cable component that performs simulation and rendering */
	UPROPERTY(Category = Cable, VisibleAnywhere, BlueprintReadWrite)
		class UCableComponent* Cable;
	class UBoxComponent* EndCollision;

	UPROPERTY(EditAnywhere)
		float CableStartOffset = 300;
	UPROPERTY(EditAnywhere)
		bool bUseGoodCable = true;
};