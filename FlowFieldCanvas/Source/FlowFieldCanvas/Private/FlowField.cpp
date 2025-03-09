// LeroyWorks 2024 All Rights Reserved.

#include "FlowField.h"
#include <queue>
#include <vector>
#include "DrawDebugHelpers.h"
#include "Kismet/KismetSystemLibrary.h"

AFlowField::AFlowField()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* DefaultRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	RootComponent = DefaultRoot;

	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	Volume->SetupAttachment(DefaultRoot); 
	Volume->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Volume->SetGenerateOverlapEvents(false);

	ISM_DirArrows = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("NormalArrows"));
	ISM_DirArrows->SetupAttachment(DefaultRoot);
	ISM_DirArrows->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	ISM_DirArrows->SetGenerateOverlapEvents(false);
	ISM_DirArrows->SetCastShadow(false);
	ISM_DirArrows->SetReceivesDecals(false);

	ISM_NullArrows = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("NullArrows"));
	ISM_NullArrows->SetupAttachment(DefaultRoot);
	ISM_NullArrows->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	ISM_NullArrows->SetGenerateOverlapEvents(false);
	ISM_NullArrows->SetCastShadow(false);
	ISM_NullArrows->SetReceivesDecals(false);

	Decal_Cells = CreateDefaultSubobject<UDecalComponent>(TEXT("Decal"));
	Decal_Cells->SetupAttachment(DefaultRoot);
	Decal_Cells->SetRelativeRotation(FRotator(90,0,0));

	Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("BillBoard"));
	Billboard->SetupAttachment(DefaultRoot);
}

void AFlowField::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (bEditorLiveUpdate)
	{ 
		DrawDebug();
	}
	else
	{
		InitFlowFieldMinimal(EInitMode::Construction);
	}
}

void AFlowField::BeginPlay()
{
	Super::BeginPlay();

	InitFlowField(EInitMode::BeginPlay);
}

void AFlowField::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AFlowField::DrawDebug()
{
	InitFlowField(EInitMode::Construction);

	UpdateGoalLocation();

	CreateGrid();

	CalculateFlowField();

	DrawCells(EInitMode::Construction);

	DrawArrows(EInitMode::Construction);
}

void AFlowField::TickFlowField()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FlowFieldTick");

	InitFlowField(EInitMode::Ticking);

	UpdateGoalLocation();

	CreateGrid();

	CalculateFlowField();

	DrawCells(EInitMode::Ticking);

	DrawArrows(EInitMode::Ticking);

	UpdateTimer();
}

void AFlowField::UpdateGoalLocation()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UpdateGoalLocation");

	if (nextTickTimeLeft != updateInterval) return;
	
	if (IsValid(goalActor.Get()))
	{
		goalLocation = goalActor->GetActorLocation();
		WorldToGrid(goalLocation, bIsValidGoalCoord, goalGridCoord);
	}
	else
	{
		WorldToGrid(goalLocation, bIsValidGoalCoord, goalGridCoord);
	}
}

void AFlowField::InitFlowField(EInitMode InitMode)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("InitFlowField");

	if (InitMode == EInitMode::Construction || InitMode == EInitMode::BeginPlay)
	{
		nextTickTimeLeft = FMath::Clamp(updateInterval,0.f,FLT_MAX);
		bIsGridDirty = true;

		// decal dynamic material

		if (!IsValid(goalActor.Get()))
		{
			goalLocation = GetActorLocation();
		}

		if (IsValid(DecalBaseMat))
		{
			DecalDMI = UMaterialInstanceDynamic::Create(DecalBaseMat, this);

			if (IsValid(DecalDMI))
			{
				Decal_Cells->SetMaterial(0, DecalDMI);
			}
		}

		if (IsValid(arrowBaseMat))
		{
			arrowDMI = UMaterialInstanceDynamic::Create(arrowBaseMat, this);

			if (IsValid(arrowDMI))
			{
				ISM_DirArrows->SetMaterial(0, arrowDMI);
			}
		}
	}

	cellSize = FMath::Clamp(cellSize, 0.1f, FLT_MAX);
	flowFieldSize = FVector(FMath::Clamp(flowFieldSize.X, cellSize, FLT_MAX), FMath::Clamp(flowFieldSize.Y, cellSize, FLT_MAX), flowFieldSize.Z);

	xNum = FMath::RoundToInt(flowFieldSize.X / cellSize);
	yNum = FMath::RoundToInt(flowFieldSize.Y / cellSize);

	actorLoc = GetActorLocation();
	actorRot = GetActorRotation();

	offsetLoc = FVector(xNum * cellSize / 2, yNum * cellSize / 2, 0);
	relativeLoc = actorLoc - offsetLoc;

	FVector halfSize = FVector(xNum * cellSize, yNum * cellSize, flowFieldSize.Z) / 2;

	Volume->SetBoxExtent(halfSize);
	Volume->SetRelativeLocation(FVector(0.f, 0.f, halfSize.Z));

	initialCost = FMath::Clamp(initialCost, 0, 255);

	// update decal material
	if ((InitMode == EInitMode::Construction && drawCellsInEditor) || (InitMode != EInitMode::Construction && drawCellsInGame))
	{
		Decal_Cells->DecalSize = FVector(halfSize.Z, halfSize.Y, halfSize.X);
		Decal_Cells->SetRelativeLocation(FVector(0.f, 0.f, halfSize.Z));

		if (IsValid(DecalDMI))
		{
			DecalDMI->SetScalarParameterValue("XNum", xNum);
			DecalDMI->SetScalarParameterValue("YNum", yNum);
			DecalDMI->SetScalarParameterValue("CellSize", cellSize);
			DecalDMI->SetScalarParameterValue("OffsetX", offsetLoc.X);
			DecalDMI->SetScalarParameterValue("OffsetY", offsetLoc.Y);
			DecalDMI->SetScalarParameterValue("Yaw", actorRot.Yaw);
		}
	}
}

void AFlowField::InitFlowFieldMinimal(EInitMode InitMode)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("InitFlowField");

	if (InitMode == EInitMode::Construction || InitMode == EInitMode::BeginPlay)
	{
		nextTickTimeLeft = FMath::Clamp(updateInterval, 0.f, FLT_MAX);
		bIsGridDirty = true;
	}

	cellSize = FMath::Clamp(cellSize, 0.1f, FLT_MAX);
	flowFieldSize = FVector(FMath::Clamp(flowFieldSize.X, cellSize, FLT_MAX), FMath::Clamp(flowFieldSize.Y, cellSize, FLT_MAX), flowFieldSize.Z);

	xNum = FMath::RoundToInt(flowFieldSize.X / cellSize);
	yNum = FMath::RoundToInt(flowFieldSize.Y / cellSize);

	actorLoc = GetActorLocation();
	actorRot = GetActorRotation();

	offsetLoc = FVector(xNum * cellSize / 2, yNum * cellSize / 2, 0);
	relativeLoc = actorLoc - offsetLoc;

	FVector halfSize = FVector(xNum * cellSize, yNum * cellSize, flowFieldSize.Z) / 2;

	Volume->SetBoxExtent(halfSize);
	Volume->SetRelativeLocation(FVector(0.f, 0.f, halfSize.Z));

	initialCost = FMath::Clamp(initialCost, 0, 255);
}

void AFlowField::CreateGrid()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("CreateGrid");

	if (!bIsGridDirty) return;

	InitialCellsMap.Empty();
	CurrentCellsMap.Empty();

	//GridModifiedSet.Empty();
	//DirModifiedSet.Empty();

	//GridModifiers.Empty();
	//CostModifiers.Empty();
	//DirModifiers.Empty();

	for (int x = 0; x < xNum; ++x)
	{
		for (int y = 0; y < yNum; ++y)
		{
			FVector2D gridCoord = FVector2D(x, y);
			FCellStruct newCell = EnvQuery(gridCoord);
			InitialCellsMap.Add(gridCoord,newCell);
		}
	}

	bIsGridDirty = false;
}

void AFlowField::CalculateFlowField() 
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("CalculateFlowField");

	if (nextTickTimeLeft != updateInterval) return;

	CurrentCellsMap = InitialCellsMap;
	CurrentCellsMap[goalGridCoord].cost = 0;

	TMap<FVector2D, FCellStruct> NewCellsMap = CurrentCellsMap;
	int32 numCells = NewCellsMap.Num();

	auto IsValidCoord = [&](FVector2D gridCoord) -> bool { return gridCoord.X >= 0 && gridCoord.X < xNum && gridCoord.Y >= 0 && gridCoord.Y < yNum; };
	auto IsValidDiagonal = [&](std::vector<FVector2D> neighborCoords, int32 indexA, int32 indexB) -> bool
		{
			bool result = true;

			if (IsValidCoord(neighborCoords[indexA]))
			{
				if (NewCellsMap[neighborCoords[indexA]].cost == 255)
				{
					result = false;
				}
			}
			if (IsValidCoord(neighborCoords[indexB]))
			{
				if (NewCellsMap[neighborCoords[indexB]].cost == 255)
				{
					result = false;
				}
			}
			return result;
		};

	struct CostCompare {
		bool operator()(const FCellStruct& lhs, const FCellStruct& rhs) const {
			return lhs.dist > rhs.dist;
		}
	};

	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("CreateIntegrationField");

		FCellStruct& targetCell = NewCellsMap[goalGridCoord];
		targetCell.cost = 0;
		targetCell.dist = 0;

		std::priority_queue<FCellStruct, std::vector<FCellStruct>, CostCompare> CellsToCheck;
		CellsToCheck.push(targetCell);

		while (!CellsToCheck.empty())
		{
			FCellStruct currentCell = CellsToCheck.top();
			FVector2D currentCoord = currentCell.gridCoord;
			CellsToCheck.pop();

			std::vector<FVector2D> neighborCoords;

			if (Style == EStyle::AdjacentFirst)
			{
				neighborCoords = {
					currentCoord + FVector2D(1,-1),
					currentCoord + FVector2D(1,1),
					currentCoord + FVector2D(-1,1),
					currentCoord + FVector2D(-1,-1),
					currentCoord + FVector2D(0,-1),
					currentCoord + FVector2D(1,0),
					currentCoord + FVector2D(0,1),
					currentCoord + FVector2D(-1,0)
				};
			}
			else if (Style == EStyle::DiagonalFirst)
			{
				neighborCoords = {
					currentCoord + FVector2D(0,-1),
					currentCoord + FVector2D(1,0),
					currentCoord + FVector2D(0,1),
					currentCoord + FVector2D(-1,0)
				};
			}

			for (int32 i = 0; i < neighborCoords.size(); ++i)
			{
				FVector2D neighborCoord = neighborCoords[i];

				if (!IsValidCoord(neighborCoord)) continue;

				FCellStruct& neighborCell = NewCellsMap[neighborCoord];

				if (neighborCell.cost == 255) continue;

				if (Style == EStyle::AdjacentFirst)
				{
					if (i == 0 && !IsValidDiagonal(neighborCoords, 4, 5)) continue;
					if (i == 1 && !IsValidDiagonal(neighborCoords, 5, 6)) continue;
					if (i == 2 && !IsValidDiagonal(neighborCoords, 6, 7)) continue;
					if (i == 3 && !IsValidDiagonal(neighborCoords, 7, 4)) continue;
				}

				float heightDifference = FMath::Abs(currentCell.worldLoc.Z - neighborCell.worldLoc.Z);
				float horizontalDistance = FVector2D::Distance(FVector2D(currentCell.worldLoc.X, currentCell.worldLoc.Y), FVector2D(neighborCell.worldLoc.X, neighborCell.worldLoc.Y));
				float slopeAngle = FMath::RadiansToDegrees(FMath::Atan(heightDifference / horizontalDistance));

				if (slopeAngle > maxWalkableAngle && currentCell.cost != 255) continue;

				int32 newDist = neighborCell.cost + currentCell.dist;

				if (newDist < neighborCell.dist)
				{
					neighborCell.dist = newDist;
					CellsToCheck.push(neighborCell);
				}
			}
		}
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("CreateFlowField");

		ParallelFor(numCells, [&](int32 j)
			{
				int32 currentIndex = j;
				FVector2D currentCoord = FVector2D(currentIndex / yNum, currentIndex % yNum);
				FCellStruct& currentCell = NewCellsMap[currentCoord];

				bool hasBestCell = false;
				FCellStruct bestCell;
				int32 bestDist = currentCell.dist;

				std::vector<FVector2D> neighborCoords =
				{
					currentCoord + FVector2D(0,-1),
					currentCoord + FVector2D(1,0),
					currentCoord + FVector2D(0,1),
					currentCoord + FVector2D(-1,0),
					currentCoord + FVector2D(1,-1),
					currentCoord + FVector2D(1,1),
					currentCoord + FVector2D(-1,1),
					currentCoord + FVector2D(-1,-1),
				};

				for (int32 i = 0; i < 8; i++)
				{
					FVector2D neighborCoord = neighborCoords[i];

					if (!IsValidCoord(neighborCoord)) continue;

					FCellStruct& neighborCell = NewCellsMap[neighborCoord];

					if (neighborCell.cost == 255) continue;

					if (currentCell.cost != 255)
					{
						if (i == 4 && !IsValidDiagonal(neighborCoords, 0, 1)) continue;
						if (i == 5 && !IsValidDiagonal(neighborCoords, 1, 2)) continue;
						if (i == 6 && !IsValidDiagonal(neighborCoords, 2, 3)) continue;
						if (i == 7 && !IsValidDiagonal(neighborCoords, 3, 0)) continue;
					}

					float heightDifference = FMath::Abs(currentCell.worldLoc.Z - neighborCell.worldLoc.Z);
					float horizontalDistance = FVector2D::Distance(FVector2D(currentCell.worldLoc.X, currentCell.worldLoc.Y), FVector2D(neighborCell.worldLoc.X, neighborCell.worldLoc.Y));
					float slopeAngle = FMath::RadiansToDegrees(FMath::Atan(heightDifference / horizontalDistance));

					if (slopeAngle > maxWalkableAngle && currentCell.cost != 255) continue;

					if (neighborCell.dist < bestDist)
					{
						hasBestCell = true;
						bestCell = neighborCell;
						bestDist = neighborCell.dist;
					}
				}

				if (hasBestCell)
				{
					currentCell.dir = UKismetMathLibrary::GetDirectionUnitVector(currentCell.worldLoc, bestCell.worldLoc);
				}
				else
				{
					currentCell.dir = FVector::ZeroVector;
				}
			});
	}

	CurrentCellsMap = NewCellsMap;
}

void AFlowField::DrawCells(EInitMode InitMode)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("DrawCells");

	if (nextTickTimeLeft != updateInterval) return;

	if ((InitMode == EInitMode::Construction && drawCellsInEditor) || (InitMode != EInitMode::Construction && drawCellsInGame))
	{
		if (!Decal_Cells->IsVisible()) 
		{ 
			Decal_Cells->SetVisibility(true);
		}

		float largestCellDist = 0.0001f;

		for (const TPair<FVector2D, FCellStruct>& Element : CurrentCellsMap)
		{
			const FCellStruct& Cell = Element.Value;

			if (Cell.dist > largestCellDist && Cell.dist != 65535)
			{
				largestCellDist = Cell.dist;
			}
		}

		// Create transient texture
		TransientTexture = UTexture2D::CreateTransient(xNum, yNum);

		FTexture2DMipMap* MipMap = &TransientTexture->GetPlatformData()->Mips[0];// for version api compatibility
		FByteBulkData* ImageData = &MipMap->BulkData;
		uint8* RawImageData = (uint8*)ImageData->Lock(LOCK_READ_WRITE);

		for (const TPair<FVector2D, FCellStruct>& Element : CurrentCellsMap)
		{
			const FCellStruct& Cell = Element.Value;

			// Calculate pixel index based on grid coordinates
			int32 PixelX = xNum - Cell.gridCoord.X - 1;
			int32 PixelY = Cell.gridCoord.Y;
			int32 PixelIndex = ((PixelY * xNum) + PixelX) * 4;

			RawImageData[PixelIndex + 3] = Cell.type != ECellType::Empty ? 255 : 0; // Set the Alpha channel
			RawImageData[PixelIndex + 2] = Cell.cost; // Set the R channel with cost value
			RawImageData[PixelIndex + 1] = 0; // Set the G channel
			RawImageData[PixelIndex + 0] = FMath::Clamp(FMath::RoundToInt(FMath::Clamp(float(Cell.dist), 0.f, largestCellDist) / largestCellDist * 255), 0, 255); // Set the B channel with dist
		}

		ImageData->Unlock();
		TransientTexture->UpdateResource();

		// update decal material
		if (IsValid(DecalDMI))
		{
			DecalDMI->SetTextureParameterValue("TransientTexture", TransientTexture);
		}
	}
	else
	{
		if (Decal_Cells->IsVisible())
		{
			Decal_Cells->SetVisibility(false);
		}
	}
}

void AFlowField::DrawArrows(EInitMode InitMode)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("DrawArrows");

	if (nextTickTimeLeft != updateInterval) return;

	// Clear all instances first
	ISM_DirArrows->ClearInstances();
	ISM_NullArrows->ClearInstances();

	if ((InitMode == EInitMode::Construction && drawArrowsInEditor) || (InitMode != EInitMode::Construction && drawArrowsInGame))
	{
		for (const TPair<FVector2D, FCellStruct>& Element : CurrentCellsMap)
		{
			const FCellStruct& Cell = Element.Value;

			FVector Dir = Cell.dir;
			FVector Normal = Cell.normal.GetSafeNormal();

			// Project the Dir onto the plane defined by Normal
			Dir = FVector::VectorPlaneProject(Dir, Normal).GetSafeNormal();

			// Calculate initial rotation to align arrow to normal
			FQuat AlignToNormalQuat = FQuat::FindBetweenNormals(FVector::UpVector, Normal);

			// Rotate the arrow in the plane to point in the direction of projected Dir
			FVector AlignedDir = AlignToNormalQuat.RotateVector(FVector::ForwardVector);
			FQuat RotateInPlaneQuat = FQuat::FindBetweenNormals(AlignedDir, Dir);

			// Combine both rotations
			FQuat CombinedQuat = RotateInPlaneQuat * AlignToNormalQuat;
			FRotator AlignToNormalRot = CombinedQuat.Rotator();

			// Create the final transformation
			FTransform Trans(AlignToNormalRot, Cell.worldLoc + Cell.normal, FVector(cellSize / 100.0f));

			if (Cell.type != ECellType::Empty)
			{
				int32 InstanceIndex;

				if (Dir.Size() != 0)
				{
					// If it was not modified but has a direction, use DirArrows and set custom data to 0
					InstanceIndex = AddWorldSpaceInstance(ISM_DirArrows, Trans);
					ISM_DirArrows->SetCustomDataValue(InstanceIndex, 0, 0, true);
				}
				else
				{
					// If it has no direction, use NullArrows without any custom data
					AddWorldSpaceInstance(ISM_NullArrows, Trans);
				}
			}
		}
	}
}

void AFlowField::UpdateTimer()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UpdateTimer");

	if (nextTickTimeLeft <= 0)
	{
		nextTickTimeLeft = FMath::Clamp(updateInterval, 0.f, FLT_MAX);
	}
	else
	{
		nextTickTimeLeft -= GetWorld()->GetDeltaSeconds();
	}
}
   
FORCEINLINE void AFlowField::WorldToGrid(const FVector Location, bool& bIsValid, FVector2D& gridCoord)
{
	//TRACE_CPUPROFILER_EVENT_SCOPE_STR("WorldToGrid");

	FVector relativeLocation = (Location - actorLoc).RotateAngleAxis(-actorRot.Yaw, FVector(0, 0, 1)) + offsetLoc;

	float cellRadius = cellSize / 2;

	int32 gridCoordX = FMath::RoundToInt((relativeLocation.X - cellRadius) / cellSize);
	int32 gridCoordY = FMath::RoundToInt((relativeLocation.Y - cellRadius) / cellSize);

	// inside grid?
	if ((gridCoordX >= 0 && gridCoordX < xNum) && (gridCoordY >= 0 && gridCoordY < yNum)){bIsValid = true;}
	else{bIsValid = false;}

	gridCoord = FVector2D(FMath::Clamp(gridCoordX, 0, xNum - 1), FMath::Clamp(gridCoordY, 0, yNum - 1));
}

void AFlowField::GetCellAtLocation(const FVector Location, bool& bIsValid, FCellStruct& CurrentCell)
{
	//TRACE_CPUPROFILER_EVENT_SCOPE_STR("GetCellAtLocation");

	FVector relativeLocation = (Location - actorLoc).RotateAngleAxis(-actorRot.Yaw, FVector(0, 0, 1)) + offsetLoc;

	float cellRadius = cellSize / 2;

	int32 gridCoordX = FMath::RoundToInt((relativeLocation.X - cellRadius) / cellSize);
	int32 gridCoordY = FMath::RoundToInt((relativeLocation.Y - cellRadius) / cellSize);

	// inside grid?
	if ((gridCoordX >= 0 && gridCoordX < xNum) && (gridCoordY >= 0 && gridCoordY < yNum)) 
	{ 
		bIsValid = true; 
	}
	else 
	{ 
		bIsValid = false; 
	}

	// clamp to nearest
	FVector2D gridCoord = FVector2D(FMath::Clamp(gridCoordX, 0, xNum - 1), FMath::Clamp(gridCoordY, 0, yNum - 1));
	CurrentCell = CurrentCellsMap[gridCoord];
}

FORCEINLINE int32 AFlowField::AddWorldSpaceInstance(UInstancedStaticMeshComponent* ISM_Component, const FTransform& InstanceTransform)
{
	//TRACE_CPUPROFILER_EVENT_SCOPE_STR("AddWorldSpaceInstance");
	FTransform LocalTransform = InstanceTransform.GetRelativeTransform(ISM_Component->GetComponentTransform());
	return ISM_Component->AddInstance(LocalTransform);
}

FORCEINLINE FCellStruct AFlowField::EnvQuery(const FVector2D gridCoord) 
{
	//TRACE_CPUPROFILER_EVENT_SCOPE_STR("EnvQuery");

	FCellStruct newCell;

	newCell.gridCoord = gridCoord;

	FVector2D worldLoc2D = FVector2D(gridCoord.X * cellSize + relativeLoc.X + (cellSize / 2.f), gridCoord.Y * cellSize + relativeLoc.Y + (cellSize / 2.f));
	FVector worldLoc = FVector(worldLoc2D, actorLoc.Z);
	worldLoc = (worldLoc - actorLoc).RotateAngleAxis(actorRot.Yaw, FVector(0, 0, 1)) + actorLoc;

	// Array of actors for trace to ignore
	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(GetOwner());

	if (traceGround)
	{
		FHitResult GroundHitResult;

		bool hitGround = UKismetSystemLibrary::LineTraceSingleForObjects(
			GetWorld(),
			FVector(worldLoc.X, worldLoc.Y, actorLoc.Z + flowFieldSize.Z),
			FVector(worldLoc.X, worldLoc.Y, actorLoc.Z),
			groundObjectType,
			true,
			IgnoreActors,
			EDrawDebugTrace::None,
			GroundHitResult,
			true,
			FLinearColor::Gray,
			FLinearColor::Blue,
			1);

		if (hitGround)
		{
			worldLoc.Z = GroundHitResult.ImpactPoint.Z;

			newCell.normal = GroundHitResult.ImpactNormal;
			newCell.type = ECellType::Ground;

			float cellAngle = UKismetMathLibrary::Acos(FVector::DotProduct(GroundHitResult.ImpactNormal, FVector(0, 0, 1)));
			float maxAngleRadians = FMath::DegreesToRadians(maxWalkableAngle);

			if (cellAngle > maxAngleRadians)
			{
				newCell.cost = 255;
				newCell.costBP = 255;

				newCell.type = ECellType::Obstacle;
			}
			else if (traceObstacles)
			{
				FHitResult ObstacleHitResult;
				bool hitObstacle = UKismetSystemLibrary::SphereTraceSingleForObjects(
					GetWorld(),
					FVector(worldLoc.X, worldLoc.Y, actorLoc.Z + flowFieldSize.Z),
					FVector(worldLoc.X, worldLoc.Y, actorLoc.Z),
					cellSize / 2.f,
					obstacleObjectType,
					true,
					IgnoreActors,
					EDrawDebugTrace::None,
					ObstacleHitResult,
					true,
					FLinearColor::Gray,
					FLinearColor::Red,
					1);

				if (hitObstacle)
				{
					if (ObstacleHitResult.ImpactPoint.Z > GroundHitResult.ImpactPoint.Z)
					{
						newCell.cost = 255;
						newCell.costBP = 255;

						newCell.type = ECellType::Obstacle;
					}
					else
					{
						newCell.cost = initialCost;
						newCell.costBP = initialCost;

						if (initialCost == 255)
						{
							newCell.type = ECellType::Obstacle;
						}
					}
				}
				else
				{
					newCell.cost = initialCost;
					newCell.costBP = initialCost;

					if (initialCost == 255)
					{
						newCell.type = ECellType::Obstacle;
					}
				}
			}
			else
			{
				newCell.cost = initialCost;
				newCell.costBP = initialCost;

				if (initialCost == 255)
				{
					newCell.type = ECellType::Obstacle;
				}
			}

		}
		else 
		{
			worldLoc.Z = -FLT_MAX;
			newCell.normal = FVector(0, 0, 1);
			newCell.type = ECellType::Empty;
		}
	}
	else
	{
		worldLoc.Z += flowFieldSize.Z / 2;
		newCell.normal = FVector(0, 0, 1);
		newCell.type = ECellType::Ground;

		if (traceObstacles)
		{
			FHitResult ObstacleHitResult;
			bool hitObstacle = UKismetSystemLibrary::SphereTraceSingleForObjects(
				GetWorld(),
				FVector(worldLoc.X, worldLoc.Y, actorLoc.Z + flowFieldSize.Z),
				FVector(worldLoc.X, worldLoc.Y, actorLoc.Z),
				cellSize / 2.f,
				obstacleObjectType,
				true,
				IgnoreActors,
				EDrawDebugTrace::None,
				ObstacleHitResult,
				true,
				FLinearColor::Gray,
				FLinearColor::Red,
				1);

			if (hitObstacle)
			{
				if (ObstacleHitResult.ImpactPoint.Z > worldLoc.Z)
				{
					newCell.cost = 255;
					newCell.costBP = 255;

					newCell.type = ECellType::Obstacle;
				}
				else
				{
					newCell.cost = initialCost;
					newCell.costBP = initialCost;

					if (initialCost == 255)
					{
						newCell.type = ECellType::Obstacle;
					}
				}
			}
			else
			{
				newCell.cost = initialCost;
				newCell.costBP = initialCost;

				if (initialCost == 255)
				{
					newCell.type = ECellType::Obstacle;
				}
			}
		}
		else
		{
			newCell.cost = initialCost;
			newCell.costBP = initialCost;

			if (initialCost == 255)
			{
				newCell.type = ECellType::Obstacle;
			}
		}
	}

	newCell.worldLoc = worldLoc;
	return newCell;
}
