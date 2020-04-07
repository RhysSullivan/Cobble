// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interactable.h"
#include "Lever.generated.h"

/**
 * 
 */
UCLASS()
class COBBLE_API ALever : public AInteractable
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere)
		bool bIsFlippedToTheLeft = false;
public:
	// Sets default values for this actor's properties
	ALever();
	//Interactable interface
	virtual void Highlight() override;
	virtual void Unhighlight() override;
	virtual void Interact() override;
	
	virtual void BeginPlay() override;
protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	
	
	// Could be deleted to save memory
	UPROPERTY(VisibleAnywhere)
	class UPaperSpriteComponent* LeftPlaceholder;
	UPROPERTY(VisibleAnywhere)
	class UPaperSpriteComponent* RightPlaceholder;
private:
	void RotateToMatchFlippedDirection();
private:
	UPROPERTY(VisibleDefaultsOnly)
		class UChildActorComponent* LeftHose;
};
