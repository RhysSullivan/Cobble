// Fill out your copyright notice in the Description page of Project Settings.


#include "MovingPlatform.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
AMovingPlatform::AMovingPlatform()
{
	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Platform Object"));
	PlatformMesh->SetupAttachment(GetRootComponent());

	MovingPlatformPath = CreateDefaultSubobject<USplineComponent>(TEXT("SplineForThePlatform"));
	MovingPlatformPath->SetupAttachment(GetRootComponent());
	GearHolderActor->SetupAttachment(PlatformMesh);
}

void AMovingPlatform::BeginPlay()
{
	Super::BeginPlay();
}

void AMovingPlatform::OnConstruction(const FTransform & Transform)
{
	Super::OnConstruction(Transform);
	if (bIsStraightPath)
	{
		for (int i = 0; i < MovingPlatformPath->GetNumberOfSplinePoints(); i++)
		{
			MovingPlatformPath->SetSplinePointType(i, ESplinePointType::Linear);
		}
	}
	else
	{
		for (int i = 0; i < MovingPlatformPath->GetNumberOfSplinePoints(); i++)
		{
			MovingPlatformPath->SetSplinePointType(i, ESplinePointType::Curve);
		}
	}
}

void AMovingPlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (IsPowered())
	{
		if (TimeWaited <= 0)
		{
			if (!bReverse)
			{
				if (AmountOfSplineTraversed < MovingPlatformPath->GetSplineLength() && Direction)
				{
					AmountOfSplineTraversed += MovementSpeed * DeltaTime * Direction;
					PlatformMesh->SetWorldLocation(MovingPlatformPath->GetLocationAtDistanceAlongSpline(AmountOfSplineTraversed, ESplineCoordinateSpace::World));
				}
				else
				{
					bReverse = true;
					TimeWaited = TimeToWaitAtEndPoint;
				}
			}
			else
			{
				if (AmountOfSplineTraversed > 0)
				{
					AmountOfSplineTraversed += MovementSpeed * DeltaTime * -1;
					PlatformMesh->SetWorldLocation(MovingPlatformPath->GetLocationAtDistanceAlongSpline(AmountOfSplineTraversed, ESplineCoordinateSpace::World));
				}
				else
				{
					bReverse = false;
					TimeWaited = TimeToWaitAtEndPoint;
				}
			}
		}
		else
		{
			TimeWaited -= DeltaTime;
		}
	}
}