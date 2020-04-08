// Fill out your copyright notice in the Description page of Project Settings.


#include "CobblePaperCharacter.h"
#include "Components/InputComponent.h"
#include "PaperFlipbookComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PaperFlipbook.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/BoxComponent.h"
#include "PaperSpriteComponent.h"
#include "InteractInterface.h"
#include "PickupInterface.h"
#include "Gear.h"
ACobblePaperCharacter::ACobblePaperCharacter()
{	
	PrimaryActorTick.bCanEverTick = true;
	FlipbookComponent = GetSprite();
	FlipbookComponent->CastShadow = true;
	
	InteractCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractCollision"));
	InteractCollision->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	
	HighlightedGearComponent = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("HighlightedGear"));
	HighlightedGearComponent->SetHiddenInGame(true);
	HighlightedGearComponent->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

	PickedUpGearComponent = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("PickedUpGear"));
	PickedUpGearComponent->SetHiddenInGame(true);
	PickedUpGearComponent->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform); // TODO: Attach this to a scene root and use that to mirror the sprite around when the player is facing a different direction.
}

void ACobblePaperCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerInputComponent->BindAxis("MoveHorizontal", this, &ACobblePaperCharacter::MoveHorizontal);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACobblePaperCharacter::PreJump);
	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &ACobblePaperCharacter::Interact);
}

void ACobblePaperCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ACobblePaperCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{	
	GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
}

void ACobblePaperCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	SearchForOverlappedInteractables();
	RotateToMatchMovementDirection();
	DoCobbleStateMachine();
}

void ACobblePaperCharacter::SearchForOverlappedInteractables()
{
	TArray<AActor*> OverlappingActors;
	InteractCollision->GetOverlappingActors(OverlappingActors);
	bool bIsActorWithInteractInterfaceInRange = false;
	if (!OverlappingActors.Contains(OverlappedActor))
	{
		for (const auto& a : OverlappingActors)
		{
			if (a->GetClass()->ImplementsInterface(UInteractInterface::StaticClass()))
			{
				bIsActorWithInteractInterfaceInRange = true;
				if (a != OverlappedActor)
				{
					IInteractInterface* NewOverlappedActorInteractInterface = Cast<IInteractInterface>(a);
					IInteractInterface* OldOverlappedActorInteractInterface = Cast<IInteractInterface>(OverlappedActor);

					NewOverlappedActorInteractInterface->Highlight();
					if (OldOverlappedActorInteractInterface != nullptr)
						OldOverlappedActorInteractInterface->Unhighlight();
					OverlappedActor = a;
					break;
				}
			}
		}
	}
	else
	{
		bIsActorWithInteractInterfaceInRange = true;
	}
	if (!bIsActorWithInteractInterfaceInRange && OverlappedActor != nullptr)
	{
		IInteractInterface* OldOverlappedActorInteractInterface = Cast<IInteractInterface>(OverlappedActor);
		if (OldOverlappedActorInteractInterface != nullptr)
			OldOverlappedActorInteractInterface->Unhighlight();
		OverlappedActor = nullptr;
	}
}


/*
ANIMATIONS
*/
void ACobblePaperCharacter::DoCobbleStateMachine()
{
	if (ShouldDoStateMachine()) // If we are not waiting on an animation to finish
	{
		if (GetCharacterMovement()->IsFalling())
		{
			if (GetVelocity().Z > 0)
			{
				FlipbookComponent->SetFlipbook(RisingFlipbook);
			}
			else
			{
				FlipbookComponent->SetFlipbook(FallingFlipbook);
			}
		}
		else
		{
			if (GetVelocity().Size() > 0)
			{
				FlipbookComponent->SetFlipbook(RunFlipbook);
			}
			else
			{
				FlipbookComponent->SetFlipbook(IdleFlipbook);
			}
		}
	}
}

bool ACobblePaperCharacter::ShouldDoStateMachine()
{
	// If we have active timer handles then an animation is playing.
	return !JumpTimerHandle.IsValid() && !InteractTimerHandle.IsValid();
}

void ACobblePaperCharacter::RotateToMatchMovementDirection()
{
	if (FMath::Square(GetVelocity().X) + FMath::Square(GetVelocity().Y) > 0)
	{
		float FacingDirection = FVector::DotProduct(GetActorForwardVector(), GetVelocity().GetSafeNormal());
		if (FacingDirection < -.5)
			FlipbookComponent->SetRelativeRotation(FRotator(0, 180, 0));
		else
			FlipbookComponent->SetRelativeRotation(FRotator(0, 0, 0));
	}
}

void ACobblePaperCharacter::MoveHorizontal(float Value)
{
	if (InteractTimerHandle.IsValid())
		return;
	AddMovementInput(GetActorForwardVector(), Value);
}

/*
JUMPING
*/
void ACobblePaperCharacter::DoJump()
{
	Jump();
	GetWorld()->GetTimerManager().ClearTimer(JumpTimerHandle);
}

void ACobblePaperCharacter::PreJump()
{
	if (!CanJump())
		return;
	FlipbookComponent->SetFlipbook(PreJumpFlipbook);
	GetWorld()->GetTimerManager().SetTimer(JumpTimerHandle, this, &ACobblePaperCharacter::DoJump, FlipbookComponent->GetFlipbookLength(), false);
}

bool ACobblePaperCharacter::CanJump()
{
	return Super::CanJump() && !GetMovementComponent()->IsFalling() && !InteractTimerHandle.IsValid();
}

void ACobblePaperCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	FlipbookComponent->SetFlipbook(LandingFlipbook);
	GetWorld()->GetTimerManager().SetTimer(JumpTimerHandle, this, &ACobblePaperCharacter::PostLandedAnimation, FlipbookComponent->GetFlipbookLength(), false);
}

void ACobblePaperCharacter::PostLandedAnimation() { GetWorld()->GetTimerManager().ClearTimer(JumpTimerHandle); }

/*
INTERACTION
*/
void ACobblePaperCharacter::Interact()
{
	if (HeldActor != nullptr)
	{
		IPickupInterface* HeldActorInterface = Cast<IPickupInterface>(HeldActor);
		if (HeldActorInterface != nullptr)
		{
			HeldActorInterface->Drop();
			HeldActor = nullptr;
			return;
		}

	}
	if (!CanInteract())
		return;
	FlipbookComponent->SetFlipbook(InteractFlipbook);
	GetWorld()->GetTimerManager().SetTimer(InteractTimerHandle, this, &ACobblePaperCharacter::PostInteract, FlipbookComponent->GetFlipbookLength(), false);
	IInteractInterface* OverlappedActorInteractInterface = Cast<IInteractInterface>(OverlappedActor);
	if (OverlappedActorInteractInterface != nullptr)
	{
		OverlappedActorInteractInterface->Interact();
	}
}

bool ACobblePaperCharacter::CanInteract()
{
	return !GetMovementComponent()->IsFalling() && !JumpTimerHandle.IsValid();
}

void ACobblePaperCharacter::PostInteract()
{
	GetWorld()->GetTimerManager().ClearTimer(InteractTimerHandle);
}

bool ACobblePaperCharacter::ReceiveGear(AActor * Gear)
{
	if (HeldActor == nullptr)
	{
		ShowHeldGear();
		HeldActor = Gear;
		return true;
	}
	return false;
}

bool ACobblePaperCharacter::TakeGear(AActor*& Gear)
{
	if (HeldActor != nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("TAKINGGEAR"));
		Gear = HeldActor;
		HeldActor = nullptr;
		HideHeldGear();
		return true;
	}
	return false;
}

bool ACobblePaperCharacter::SetHeldActor(AActor * Other)
{
	if(HeldActor == nullptr)
	{
		HeldActor = Other;
		return true;
	}
	return false;
}

/*
GEAR HIGHLIGHTING
*/
void ACobblePaperCharacter::ShowGearHighlight()
{
	if(HeldActor == nullptr)
	HighlightedGearComponent->SetHiddenInGame(false);
}

void ACobblePaperCharacter::HideGearHighlight()
{
	HighlightedGearComponent->SetHiddenInGame(true);
}

void ACobblePaperCharacter::ShowHeldGear()
{
	PickedUpGearComponent->SetHiddenInGame(false);
}

void ACobblePaperCharacter::HideHeldGear()
{
	PickedUpGearComponent->SetHiddenInGame(true);
}

bool ACobblePaperCharacter::IsPlayerHoldingGear()
{
	if (HeldActor == nullptr)
		return false;
	AGear* Gear = Cast<AGear>(HeldActor);
	return Gear != nullptr;
}

