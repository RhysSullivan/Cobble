// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PaperSpriteComponent.h"
#include "InteractInterface.h"
#include "Gear.generated.h"

UCLASS()
class COBBLE_API AGear : public AActor, public IInteractInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AGear();
	virtual void Highlight() override;
	virtual void Unhighlight() override;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
private:
	UPROPERTY(VisibleAnywhere)
	class UPaperSpriteComponent* GearSpriteComponent;
	UPROPERTY(VisibleAnywhere)
	class UPaperSpriteComponent* GearHighlightComponent;
	UPROPERTY(EditAnywhere)
	class UPaperSprite* GearSprite;
};
