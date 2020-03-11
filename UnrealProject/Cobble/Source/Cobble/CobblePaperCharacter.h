// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PaperCharacter.h"
#include "CobblePaperCharacter.generated.h"

/**
 * 
 */
UCLASS()
class COBBLE_API ACobblePaperCharacter : public APaperCharacter
{
	GENERATED_BODY()
public:
	// Sets default values for this character's properties
	ACobblePaperCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	void RotateToMatchMovementDirection();

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void ShowGearHighlight();
	void HideGearHighlight();
public:
	UPROPERTY(EditAnywhere)
	class UPaperFlipbook* RunFlipbook;
	UPROPERTY(EditAnywhere)
	class UPaperFlipbook* IdleFlipbook;
	UPROPERTY(EditAnywhere)
	class UPaperFlipbook* PreJumpFlipbook;
	UPROPERTY(EditAnywhere)
	class UPaperFlipbook* RisingFlipbook;
	UPROPERTY(EditAnywhere)
	class UPaperFlipbook* FallingFlipbook;
	UPROPERTY(EditAnywhere)
	class UPaperFlipbook* LandingFlipbook;
	UPROPERTY(EditAnywhere)
	class UPaperFlipbook* InteractFlipbook;
	UPROPERTY(VisibleAnywhere)
	class UPaperSpriteComponent* HighlightedGearComponent;
private:
	void MoveHorizontal(float Value);

	void DoCobbleStateMachine();

	bool ShouldDoStateMachine();


	/*
	PreJump - Play prejump anim and start timer. 
	DoJump - Called after pre jump - clears timer, does jump
	Written by Rhys Sullivan
	*/
	void PreJump();
	bool CanJump();
	UFUNCTION()
	void DoJump();

	/*
	Landed - Plays the landing animation & starts a timer for when that ends. 
	PostLandedAnimation - After the landing timer ends, clear the timer. 
	Written by Rhys Sullivan
	*/ 
	void Landed(const FHitResult& Hit) override;
	UFUNCTION()
	void PostLandedAnimation();

	/*
	
	Written by Rhys Sullivan
	*/
	void Interact();
	bool CanInteract();
	void PostInteract();

private:
	class UPaperFlipbookComponent* FlipbookComponent; // Reference to the flipbook pointer so we don't have to call GetSprite() over and over
	FTimerHandle JumpTimerHandle; // Managers the timer for jumping related animations
	FTimerHandle InteractTimerHandle;
	UPROPERTY(VisibleAnywhere)
	class UBoxComponent* InteractCollision;
	AActor* OverlappedActor;
};
