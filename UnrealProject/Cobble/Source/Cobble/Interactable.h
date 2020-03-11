// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InteractInterface.h"
#include "PaperSpriteComponent.h"
#include "GameFramework/Actor.h"
#include "Interactable.generated.h"

UCLASS()
class COBBLE_API AInteractable : public AActor, public IInteractInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AInteractable();
	virtual void Highlight() override;
	virtual void Unhighlight() override;
	virtual void Interact() override;
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
protected:
	UPROPERTY(VisibleAnywhere)
	class UPaperSpriteComponent* RegularSpriteComponent;
	UPROPERTY(VisibleAnywhere)
	class UPaperSpriteComponent* HighlightedSpriteComponent;
	class USceneComponent* SceneRoot;
};
