// Level Zero - seeded procedural "Level 0" maze built from instanced greybox modules.
#include "LevelZeroMazeGenerator.h"
#include "LevelZeroTheme.h"

#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/App.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#include "AssetCompilingManager.h"
#include "ActorFactories/ActorFactory.h"
#include "Builders/CubeBuilder.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLevelZeroMaze, Log, All);

namespace
{
	const FIntPoint GDirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };

	UInstancedStaticMeshComponent* MakeISM(AActor* Owner, USceneComponent* Parent, const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* ISM = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		ISM->SetupAttachment(Parent);
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->SetCanEverAffectNavigation(false);
		ISM->SetCastShadow(false);
		ISM->SetMobility(EComponentMobility::Movable);
		return ISM;
	}
}

ALevelZeroMazeGenerator::ALevelZeroMazeGenerator()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	FloorISM = MakeISM(this, Root, TEXT("FloorISM"));
	AltFloorISM = MakeISM(this, Root, TEXT("AltFloorISM"));
	CeilingISM = MakeISM(this, Root, TEXT("CeilingISM"));
	WallISM = MakeISM(this, Root, TEXT("WallISM"));
	DoorFrameISM = MakeISM(this, Root, TEXT("DoorFrameISM"));
	PillarISM = MakeISM(this, Root, TEXT("PillarISM"));
	LightPanelISM = MakeISM(this, Root, TEXT("LightPanelISM"));
	DeadPanelISM = MakeISM(this, Root, TEXT("DeadPanelISM"));

	CollisionISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CollisionISM"));
	CollisionISM->SetupAttachment(Root);
	CollisionISM->SetVisibility(false);
	CollisionISM->SetHiddenInGame(true);
	CollisionISM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	CollisionISM->SetCanEverAffectNavigation(true);
	CollisionISM->SetCastShadow(false);
	CollisionISM->SetMobility(EComponentMobility::Movable);

	LintelCollisionISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LintelCollisionISM"));
	LintelCollisionISM->SetupAttachment(Root);
	LintelCollisionISM->SetVisibility(false);
	LintelCollisionISM->SetHiddenInGame(true);
	LintelCollisionISM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	LintelCollisionISM->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	LintelCollisionISM->SetCanEverAffectNavigation(false);
	LintelCollisionISM->SetCastShadow(false);
	LintelCollisionISM->SetMobility(EComponentMobility::Movable);

	FloorCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("FloorCollision"));
	FloorCollision->SetupAttachment(Root);
	FloorCollision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	FloorCollision->SetCanEverAffectNavigation(true);
	FloorCollision->SetMobility(EComponentMobility::Movable);

	ExitMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExitMarker"));
	ExitMarker->SetupAttachment(Root);
	ExitMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ExitMarker->SetCanEverAffectNavigation(false);
	ExitMarker->SetCastShadow(false);
	ExitMarker->SetMobility(EComponentMobility::Movable);

	ExitTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ExitTrigger"));
	ExitTrigger->SetupAttachment(Root);
	ExitTrigger->SetCollisionProfileName(UCollisionProfile::DefaultProjectile_ProfileName);
	ExitTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ExitTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	ExitTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ExitTrigger->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	ExitTrigger->SetGenerateOverlapEvents(true);
	ExitTrigger->SetCanEverAffectNavigation(false);
	ExitTrigger->SetMobility(EComponentMobility::Movable);

	FogShell = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FogShell"));
	FogShell->SetupAttachment(Root);
	FogShell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FogShell->SetCanEverAffectNavigation(false);
	FogShell->SetCastShadow(false);
	FogShell->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		CubeMesh = CubeFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatFinder.Succeeded())
	{
		FallbackMaterial = MatFinder.Object;
	}
}

void ALevelZeroMazeGenerator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Generate();
}

void ALevelZeroMazeGenerator::BeginPlay()
{
	Super::BeginPlay();

	if (bRandomSeedOnBeginPlay)
	{
		Seed = FMath::Rand();
	}
	Generate();
	if (bBuildNavOnBeginPlay)
	{
		BuildRuntimeNav();
	}
	ExitTrigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &ALevelZeroMazeGenerator::HandleExitOverlap);

	SetActorTickInterval(StreamUpdateInterval);
	UpdateStreaming(true);

	if (bTeleportPlayerToStart)
	{
		PlacePlayerAtStart();
		// The local pawn can be spawned after us depending on load order; retry next tick.
		GetWorldTimerManager().SetTimerForNextTick(this, &ALevelZeroMazeGenerator::PlacePlayerAtStart);
	}

	BeginShotModeIfRequested();
}

void ALevelZeroMazeGenerator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bFlying)
	{
		TickFlythrough(DeltaSeconds);
	}
	UpdateStreaming(false);
}

// ------------------------------------------------------------------ Layout

void ALevelZeroMazeGenerator::Generate()
{
	const double T0 = FPlatformTime::Seconds();
	ApplyTheme();
	Width = FMath::Clamp(Width, 4, 64);
	Height = FMath::Clamp(Height, 4, 64);

	FRandomStream Rng(Seed);
	BuildLayout(Rng);
	BuildMeshes(Rng);
	BuildProps();

	if (PlayerStartActor)
	{
		PlayerStartActor->SetActorLocationAndRotation(GetCellCenterWorld(StartCell, 100.f), FRotator(0.f, GetFacingYaw(StartCell), 0.f));
	}
	LastStreamCell = FIntPoint(-9999, -9999);
	LastGenerateMs = (FPlatformTime::Seconds() - T0) * 1000.0;
	UE_LOG(LogLevelZeroMaze, Log, TEXT("%s"), *GetStatsString());
}

// ------------------------------------------------------------------ Theme

void ALevelZeroMazeGenerator::ApplyTheme()
{
	if (!Theme) return;
	const ULevelZeroTheme& T = *Theme;
	if (!bRandomSeedOnBeginPlay) { Seed = T.Seed; }
	NextLevel = T.NextLevel;
	Width = T.Width; Height = T.Height; CellSize = T.CellSize;
	WallHeight = T.CeilingHeight; PartitionHeight = T.PartitionHeight; WallThickness = T.WallThickness;
	bWideCorridors = T.CorridorWidthCells >= 2; CorridorWidthCells = FMath::Clamp(T.CorridorWidthCells, 2, 4);
	NumRooms = T.NumRooms; RoomSizeMin = T.RoomSizeMin; RoomSizeMax = T.RoomSizeMax;
	NumPillarHalls = T.NumPillarHalls; PillarHallSizeMin = T.PillarHallSizeMin; PillarHallSizeMax = T.PillarHallSizeMax; PillarSize = T.PillarSize;
	LoopChance = T.LoopChance; StubWallChance = T.StubWallChance;
	RoomDoorwayChance = T.RoomDoorwayChance; CorridorDoorwayChance = T.CorridorDoorwayChance; DoorWidth = T.DoorWidth; DoorHeight = T.DoorHeight;
	LitPanelChance = T.LitPanelChance; DeadPanelChance = T.DeadPanelChance; LightSpacingCells = T.LightSpacingCells; LightFixtureScale = T.LightFixtureScale;
	StreamRadiusCells = T.StreamRadiusCells;
	LayoutStyle = T.LayoutStyle; CrossAisleEvery = T.CrossAisleEvery; CrossAisleWidth = T.CrossAisleWidth;
	RoomCeilingHeight = T.RoomCeilingHeight; HallCeilingHeight = T.HallCeilingHeight; BalconyHeight = T.BalconyHeight;
	bSeatTiersInRooms = T.bSeatTiersInRooms; AltFloorPattern = T.AltFloorPattern; bAllLightCellsLit = T.bAllLightCellsLit;
	bPlaceProps = T.bPlaceProps; CeilingLightMesh = T.CeilingLightMesh; WallVentMesh = T.WallVentMesh; WallVentChance = T.WallVentChance; WallVentZ = T.WallVentZ;
	ClutterWallMeshes = T.ClutterWallMeshes; ClutterFreeMeshes = T.ClutterFreeMeshes; NumClutterClusters = T.NumClutterClusters;
	ClusterWallItems = T.ClusterWallItems; ClusterFreeItems = T.ClusterFreeItems; bClustersInCorridors = T.bClustersInCorridors;
	LandmarkMeshes = T.LandmarkMeshes; LandmarksPerMesh = T.LandmarksPerMesh; LandmarkMinStartDistance = T.LandmarkMinStartDistance;
	LandmarkSpacing = T.LandmarkSpacing; bLandmarksInCorridors = T.bLandmarksInCorridors;
	ThemedPropMIDs.Reset();
}

FName ALevelZeroMazeGenerator::GetLevelId() const
{
	return Theme ? Theme->LevelId : FName(TEXT("level0"));
}

UMaterialInterface* ALevelZeroMazeGenerator::ThemedSurface(UMaterialInterface* Base, const FLinearColor* Color, float Emissive)
{
	if (!Theme || !Theme->bApplyPalette || !Base) return Base;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
	if (Color) { MID->SetVectorParameterValue(TEXT("Color"), *Color); }
	if (Emissive >= 0.f) { MID->SetScalarParameterValue(TEXT("EmissiveStrength"), Emissive); }
	MID->SetVectorParameterValue(TEXT("LightTint"), Theme->LightTint);
	MID->SetScalarParameterValue(TEXT("Ambient"), Theme->Ambient);
	MID->SetScalarParameterValue(TEXT("Bounce"), Theme->Bounce);
	MID->SetScalarParameterValue(TEXT("FloorLight"), Theme->FloorLight);
	MID->SetScalarParameterValue(TEXT("CeilLight"), Theme->CeilLight);
	MID->SetScalarParameterValue(TEXT("FogStart"), Theme->FogStart);
	MID->SetScalarParameterValue(TEXT("FogEnd"), Theme->FogEnd);
	MID->SetVectorParameterValue(TEXT("FogColor"), Theme->FogColor);
	return MID;
}

UMaterialInterface* ALevelZeroMazeGenerator::GetThemedMaterial(UMaterialInterface* Base, bool bProp)
{
	if (!Theme || !Theme->bApplyPalette || !Base) return Base;
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = ThemedPropMIDs.Find(Base)) { return Found->Get(); }
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
	MID->SetScalarParameterValue(TEXT("FogStart"), Theme->FogStart);
	MID->SetScalarParameterValue(TEXT("FogEnd"), Theme->FogEnd);
	MID->SetVectorParameterValue(TEXT("FogColor"), Theme->FogColor);
	if (bProp && !FMath::IsNearlyEqual(Theme->PropBrightness, 1.f))
	{
		for (const TCHAR* Name : { TEXT("Ambient"), TEXT("Bounce") })
		{
			float V = 0.f;
			if (Base->GetScalarParameterValue(FHashedMaterialParameterInfo(Name), V)) { MID->SetScalarParameterValue(Name, V * Theme->PropBrightness); }
		}
	}
	ThemedPropMIDs.Add(Base, MID);
	return MID;
}

float ALevelZeroMazeGenerator::GetCellCeiling(int32 CellIdx) const
{
	if (!CellTypes.IsValidIndex(CellIdx)) return WallHeight;
	const uint8 T = CellTypes[CellIdx];
	if (T == static_cast<uint8>(ELZCellType::Room) && RoomCeilingHeight > 0.f) return RoomCeilingHeight;
	if (T == static_cast<uint8>(ELZCellType::PillarHall) && HallCeilingHeight > 0.f) return HallCeilingHeight;
	return WallHeight;
}

bool ALevelZeroMazeGenerator::IsAltFloor(int32 X, int32 Y) const
{
	const uint8 T = CellTypes[Idx(X, Y)];
	switch (AltFloorPattern)
	{
	case ELZFloorPattern::Rooms: return T == static_cast<uint8>(ELZCellType::Room);
	case ELZFloorPattern::Halls: return T == static_cast<uint8>(ELZCellType::PillarHall);
	case ELZFloorPattern::Checker: return ((X + Y) & 1) == 1;
	case ELZFloorPattern::Stripes: return (Y % 3) == 1;
	default: return false;
	}
}

void ALevelZeroMazeGenerator::CarveAisles()
{
	// Every row is an E-W aisle; rack walls separate rows except in the N-S cross-aisle columns.
	const int32 Every = FMath::Max(2, CrossAisleEvery), CW = FMath::Clamp(CrossAisleWidth, 1, Every - 1);
	for (int32 Y = 0; Y < Height; ++Y)
		for (int32 X = 0; X < Width; ++X)
		{
			if (X + 1 < Width) { EastOpen[Idx(X, Y)] = true; }
			if (Y + 1 < Height && (X % Every) < CW) { SouthOpen[Idx(X, Y)] = true; }
		}
	// Close the cross-aisle columns' interior E-W boundary? No: a 2-wide cross aisle stays one open lane.
}

void ALevelZeroMazeGenerator::SetOpen(FIntPoint A, FIntPoint B, bool bOpen)
{
	if (B.X == A.X + 1) { EastOpen[Idx(A)] = bOpen; }
	else if (A.X == B.X + 1) { EastOpen[Idx(B)] = bOpen; }
	else if (B.Y == A.Y + 1) { SouthOpen[Idx(A)] = bOpen; }
	else if (A.Y == B.Y + 1) { SouthOpen[Idx(B)] = bOpen; }
}

void ALevelZeroMazeGenerator::CarveRect(int32 X0, int32 Y0, int32 W, int32 H, ELZCellType Type, bool bAddPillars)
{
	for (int32 Y = Y0; Y < Y0 + H; ++Y)
	{
		for (int32 X = X0; X < X0 + W; ++X)
		{
			CellTypes[Idx(X, Y)] = static_cast<uint8>(Type);
			if (X + 1 < X0 + W) { EastOpen[Idx(X, Y)] = true; }
			if (Y + 1 < Y0 + H) { SouthOpen[Idx(X, Y)] = true; }
		}
	}
	if (bAddPillars)
	{
		for (int32 Y = Y0 + 1; Y < Y0 + H; ++Y)
		{
			for (int32 X = X0 + 1; X < X0 + W; ++X)
			{
				PillarCorners.AddUnique(FIntPoint(X, Y));
			}
		}
	}
}

void ALevelZeroMazeGenerator::BuildLayout(FRandomStream& Rng)
{
	const int32 N = Width * Height;
	CellTypes.Init(static_cast<uint8>(ELZCellType::Corridor), N);
	EastOpen.Init(false, N);
	SouthOpen.Init(false, N);
	EastDoor.Init(false, N);
	SouthDoor.Init(false, N);
	PillarCorners.Reset();

	// 1. Perfect maze: iterative recursive backtracker (on a half-res grid in wide mode).
	if (LayoutStyle == ELZLayoutStyle::Aisles)
	{
		CarveAisles();
	}
	else if (bWideCorridors && Width >= 4 && Height >= 4)
	{
		CarveWideMaze(Rng);
	}
	else
	{
	TArray<bool> Visited;
	Visited.Init(false, N);
	TArray<FIntPoint> Stack;
	const FIntPoint First(Rng.RandRange(0, Width - 1), Rng.RandRange(0, Height - 1));
	Stack.Push(First);
	Visited[Idx(First)] = true;
	while (Stack.Num() > 0)
	{
		const FIntPoint Cur = Stack.Last();
		FIntPoint Options[4];
		int32 NumOptions = 0;
		for (const FIntPoint& D : GDirs)
		{
			const FIntPoint Nb = Cur + D;
			if (IsValidCell(Nb) && !Visited[Idx(Nb)])
			{
				Options[NumOptions++] = Nb;
			}
		}
		if (NumOptions == 0)
		{
			Stack.Pop(EAllowShrinking::No);
			continue;
		}
		const FIntPoint Next = Options[Rng.RandRange(0, NumOptions - 1)];
		SetOpen(Cur, Next, true);
		Visited[Idx(Next)] = true;
		Stack.Push(Next);
	}
	}

	// 2. Merged rooms and pillar halls (only remove walls, so connectivity is preserved).
	auto PlaceRect = [&](const FIntPoint& MinSize, const FIntPoint& MaxSize, ELZCellType Type, bool bPillars)
	{
		const int32 W = FMath::Clamp(Rng.RandRange(MinSize.X, MaxSize.X), 2, Width - 2);
		const int32 H = FMath::Clamp(Rng.RandRange(MinSize.Y, MaxSize.Y), 2, Height - 2);
		const int32 X0 = Rng.RandRange(1, FMath::Max(1, Width - W - 1));
		const int32 Y0 = Rng.RandRange(1, FMath::Max(1, Height - H - 1));
		CarveRect(X0, Y0, W, H, Type, bPillars);
	};
	for (int32 i = 0; i < NumRooms; ++i) { PlaceRect(RoomSizeMin, RoomSizeMax, ELZCellType::Room, false); }
	for (int32 i = 0; i < NumPillarHalls; ++i) { PlaceRect(PillarHallSizeMin, PillarHallSizeMax, ELZCellType::PillarHall, true); }
	// A room carved over a hall later must not keep pillars in its interior; halls are placed last so this is rare,
	// but drop any pillar whose four surrounding cells are not all pillar-hall cells.
	PillarCorners.RemoveAll([this](const FIntPoint& C)
	{
		for (int32 dy = -1; dy <= 0; ++dy)
			for (int32 dx = -1; dx <= 0; ++dx)
				if (CellTypes[Idx(C.X + dx, C.Y + dy)] != static_cast<uint8>(ELZCellType::PillarHall)) return true;
		return false;
	});

	// 3. Braid: knock out some corridor walls to create loops.
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const bool bCorr = CellTypes[Idx(X, Y)] == static_cast<uint8>(ELZCellType::Corridor);
			if (X + 1 < Width && !EastOpen[Idx(X, Y)] && bCorr && CellTypes[Idx(X + 1, Y)] == static_cast<uint8>(ELZCellType::Corridor) && Rng.FRand() < LoopChance)
			{
				EastOpen[Idx(X, Y)] = true;
			}
			if (Y + 1 < Height && !SouthOpen[Idx(X, Y)] && bCorr && CellTypes[Idx(X, Y + 1)] == static_cast<uint8>(ELZCellType::Corridor) && Rng.FRand() < LoopChance)
			{
				SouthOpen[Idx(X, Y)] = true;
			}
		}
	}

	// 4. Doorways on open boundaries: mostly where corridors meet rooms/halls, sometimes mid-corridor.
	auto DoorChance = [&](int32 A, int32 B) -> float
	{
		if (CellTypes[A] != CellTypes[B]) return RoomDoorwayChance;
		if (CellTypes[A] == static_cast<uint8>(ELZCellType::Corridor)) return CorridorDoorwayChance;
		return 0.f;
	};
	NumDoorways = 0;
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 I = Idx(X, Y);
			if (X + 1 < Width && EastOpen[I] && Rng.FRand() < DoorChance(I, Idx(X + 1, Y))) { EastDoor[I] = true; ++NumDoorways; }
			if (Y + 1 < Height && SouthOpen[I] && Rng.FRand() < DoorChance(I, Idx(X, Y + 1))) { SouthDoor[I] = true; ++NumDoorways; }
		}
	}

	// 5. Start/exit as far apart as possible: double BFS (approximate graph diameter).
	const FIntPoint Probe(Rng.RandRange(0, Width - 1), Rng.RandRange(0, Height - 1));
	auto Farthest = [&](const TArray<int32>& Dist, int32& OutDist) -> FIntPoint
	{
		int32 Best = 0;
		for (int32 i = 1; i < Dist.Num(); ++i) { if (Dist[i] > Dist[Best]) Best = i; }
		OutDist = Dist[Best];
		return FIntPoint(Best % Width, Best / Width);
	};
	int32 D = 0;
	StartCell = Farthest(BFS(Probe), D);
	ExitCell = Farthest(BFS(StartCell), StartToExitDistance);
}

void ALevelZeroMazeGenerator::CarveWideMaze(FRandomStream& Rng)
{
	const int32 K = FMath::Clamp(CorridorWidthCells, 2, 4);
	if (K != 2 && Width >= 2 * K && Height >= 2 * K) { CarveWideMazeK(Rng, K); return; }
	const int32 MW = Width / 2, MH = Height / 2;
	TArray<bool> Visited;
	Visited.Init(false, MW * MH);
	TArray<FIntPoint> Stack;
	const FIntPoint First(Rng.RandRange(0, MW - 1), Rng.RandRange(0, MH - 1));
	Stack.Push(First);
	Visited[First.Y * MW + First.X] = true;
	while (Stack.Num() > 0)
	{
		const FIntPoint Cur = Stack.Last();
		FIntPoint Options[4];
		int32 NumOptions = 0;
		for (const FIntPoint& D : GDirs)
		{
			const FIntPoint Nb = Cur + D;
			if (Nb.X >= 0 && Nb.Y >= 0 && Nb.X < MW && Nb.Y < MH && !Visited[Nb.Y * MW + Nb.X]) { Options[NumOptions++] = Nb; }
		}
		if (NumOptions == 0) { Stack.Pop(EAllowShrinking::No); continue; }
		const FIntPoint Next = Options[Rng.RandRange(0, NumOptions - 1)];
		// Open both fine boundaries along the shared edge of the two 2x2 blocks.
		const FIntPoint A(FMath::Min(Cur.X, Next.X), FMath::Min(Cur.Y, Next.Y));
		for (int32 k = 0; k < 2; ++k)
		{
			if (Next.X != Cur.X) { SetOpen(FIntPoint(2 * A.X + 1, 2 * A.Y + k), FIntPoint(2 * A.X + 2, 2 * A.Y + k), true); }
			else { SetOpen(FIntPoint(2 * A.X + k, 2 * A.Y + 1), FIntPoint(2 * A.X + k, 2 * A.Y + 2), true); }
		}
		Visited[Next.Y * MW + Next.X] = true;
		Stack.Push(Next);
	}
	// Open each block's interior; sometimes keep one internal wall as a stub (a 2x2 ring minus one edge stays connected).
	for (int32 BY = 0; BY < MH; ++BY)
	{
		for (int32 BX = 0; BX < MW; ++BX)
		{
			const FIntPoint P00(2 * BX, 2 * BY), P10(2 * BX + 1, 2 * BY), P01(2 * BX, 2 * BY + 1), P11(2 * BX + 1, 2 * BY + 1);
			const FIntPoint Pairs[4][2] = { { P00, P10 }, { P01, P11 }, { P00, P01 }, { P10, P11 } };
			const int32 Stub = (Rng.FRand() < StubWallChance) ? Rng.RandRange(0, 3) : -1;
			for (int32 i = 0; i < 4; ++i) { SetOpen(Pairs[i][0], Pairs[i][1], i != Stub); }
		}
	}
	// Odd sizes: attach the leftover last column/row to its neighbour.
	for (int32 Y = 0; Y < Height; ++Y) { if (Width % 2 == 1) SetOpen(FIntPoint(Width - 2, Y), FIntPoint(Width - 1, Y), true); }
	for (int32 X = 0; X < Width; ++X) { if (Height % 2 == 1) SetOpen(FIntPoint(X, Height - 2), FIntPoint(X, Height - 1), true); }
}

void ALevelZeroMazeGenerator::CarveWideMazeK(FRandomStream& Rng, int32 K)
{
	// Same as the 2-wide carve on a 1/K grid: open all K boundaries along each carved macro edge, open block interiors
	// (optionally keeping one interior wall stub: a KxK grid minus one edge stays connected).
	const int32 MW = Width / K, MH = Height / K;
	TArray<bool> Visited;
	Visited.Init(false, MW * MH);
	TArray<FIntPoint> Stack;
	const FIntPoint First(Rng.RandRange(0, MW - 1), Rng.RandRange(0, MH - 1));
	Stack.Push(First);
	Visited[First.Y * MW + First.X] = true;
	while (Stack.Num() > 0)
	{
		const FIntPoint Cur = Stack.Last();
		FIntPoint Options[4];
		int32 NumOptions = 0;
		for (const FIntPoint& D : GDirs)
		{
			const FIntPoint Nb = Cur + D;
			if (Nb.X >= 0 && Nb.Y >= 0 && Nb.X < MW && Nb.Y < MH && !Visited[Nb.Y * MW + Nb.X]) { Options[NumOptions++] = Nb; }
		}
		if (NumOptions == 0) { Stack.Pop(EAllowShrinking::No); continue; }
		const FIntPoint Next = Options[Rng.RandRange(0, NumOptions - 1)];
		const FIntPoint A(FMath::Min(Cur.X, Next.X), FMath::Min(Cur.Y, Next.Y));
		for (int32 k = 0; k < K; ++k)
		{
			if (Next.X != Cur.X) { SetOpen(FIntPoint(K * A.X + K - 1, K * A.Y + k), FIntPoint(K * A.X + K, K * A.Y + k), true); }
			else { SetOpen(FIntPoint(K * A.X + k, K * A.Y + K - 1), FIntPoint(K * A.X + k, K * A.Y + K), true); }
		}
		Visited[Next.Y * MW + Next.X] = true;
		Stack.Push(Next);
	}
	const int32 NumInterior = 2 * K * (K - 1);
	for (int32 BY = 0; BY < MH; ++BY)
	{
		for (int32 BX = 0; BX < MW; ++BX)
		{
			const int32 Stub = (Rng.FRand() < StubWallChance) ? Rng.RandRange(0, NumInterior - 1) : -1;
			int32 e = 0;
			for (int32 y = 0; y < K; ++y)
				for (int32 x = 0; x < K; ++x)
				{
					const FIntPoint P(K * BX + x, K * BY + y);
					if (x + 1 < K) { SetOpen(P, P + FIntPoint(1, 0), e++ != Stub); }
					if (y + 1 < K) { SetOpen(P, P + FIntPoint(0, 1), e++ != Stub); }
				}
		}
	}
	// Leftover columns/rows: a strip corridor attached to its neighbour.
	for (int32 X = MW * K; X < Width; ++X)
		for (int32 Y = 0; Y < Height; ++Y) { SetOpen(FIntPoint(X - 1, Y), FIntPoint(X, Y), true); if (Y + 1 < Height) SetOpen(FIntPoint(X, Y), FIntPoint(X, Y + 1), true); }
	for (int32 Y = MH * K; Y < Height; ++Y)
		for (int32 X = 0; X < Width; ++X) { SetOpen(FIntPoint(X, Y - 1), FIntPoint(X, Y), true); if (X + 1 < Width) SetOpen(FIntPoint(X, Y), FIntPoint(X + 1, Y), true); }
}

TArray<int32> ALevelZeroMazeGenerator::BFS(FIntPoint From) const
{
	TArray<int32> Dist;
	Dist.Init(-1, Width * Height);
	if (!IsValidCell(From)) return Dist;
	TArray<FIntPoint> Queue;
	Queue.Reserve(Width * Height);
	Queue.Add(From);
	Dist[Idx(From)] = 0;
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const FIntPoint Cur = Queue[Head];
		for (const FIntPoint& Dir : GDirs)
		{
			const FIntPoint Nb = Cur + Dir;
			if (IsPassable(Cur, Nb) && Dist[Idx(Nb)] < 0)
			{
				Dist[Idx(Nb)] = Dist[Idx(Cur)] + 1;
				Queue.Add(Nb);
			}
		}
	}
	return Dist;
}

TArray<FIntPoint> ALevelZeroMazeGenerator::FindPath(FIntPoint From, FIntPoint To) const
{
	TArray<FIntPoint> Path;
	if (!IsValidCell(From) || !IsValidCell(To) || CellTypes.Num() != Width * Height) return Path;
	const TArray<int32> Dist = BFS(To); // distances to the goal; walk downhill from From
	if (Dist[Idx(From)] < 0) return Path;
	FIntPoint Cur = From;
	Path.Add(Cur);
	while (Cur != To)
	{
		for (const FIntPoint& Dir : GDirs)
		{
			const FIntPoint Nb = Cur + Dir;
			if (IsPassable(Cur, Nb) && Dist[Idx(Nb)] == Dist[Idx(Cur)] - 1)
			{
				Cur = Nb;
				break;
			}
		}
		Path.Add(Cur);
	}
	return Path;
}

bool ALevelZeroMazeGenerator::IsValidCell(FIntPoint Cell) const
{
	return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < Width && Cell.Y < Height;
}

bool ALevelZeroMazeGenerator::IsPassable(FIntPoint A, FIntPoint B) const
{
	if (!IsValidCell(A) || !IsValidCell(B) || EastOpen.Num() != Width * Height) return false;
	if (B.X == A.X + 1 && B.Y == A.Y) return EastOpen[Idx(A)];
	if (A.X == B.X + 1 && B.Y == A.Y) return EastOpen[Idx(B)];
	if (B.Y == A.Y + 1 && B.X == A.X) return SouthOpen[Idx(A)];
	if (A.Y == B.Y + 1 && B.X == A.X) return SouthOpen[Idx(B)];
	return false;
}

ELZCellType ALevelZeroMazeGenerator::GetCellType(FIntPoint Cell) const
{
	return (IsValidCell(Cell) && CellTypes.Num() == Width * Height) ? static_cast<ELZCellType>(CellTypes[Idx(Cell)]) : ELZCellType::Corridor;
}

FVector ALevelZeroMazeGenerator::GetCellCenterWorld(FIntPoint Cell, float Z) const
{
	return GetActorTransform().TransformPosition(FVector((Cell.X + 0.5f) * CellSize, (Cell.Y + 0.5f) * CellSize, Z));
}

FIntPoint ALevelZeroMazeGenerator::WorldToCell(FVector WorldLocation) const
{
	const FVector L = GetActorTransform().InverseTransformPosition(WorldLocation);
	return FIntPoint(FMath::Clamp(FMath::FloorToInt(L.X / CellSize), 0, Width - 1), FMath::Clamp(FMath::FloorToInt(L.Y / CellSize), 0, Height - 1));
}

float ALevelZeroMazeGenerator::GetFacingYaw(FIntPoint Cell) const
{
	// Face the open direction with the longest straight run of sight.
	int32 BestRun = -1;
	float BestYaw = 0.f;
	for (const FIntPoint& Dir : GDirs)
	{
		int32 Run = 0;
		FIntPoint Cur = Cell;
		while (IsPassable(Cur, Cur + Dir)) { Cur += Dir; ++Run; }
		if (Run > BestRun)
		{
			BestRun = Run;
			BestYaw = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Dir.Y), static_cast<float>(Dir.X)));
		}
	}
	return BestYaw;
}

// ------------------------------------------------------------------ Meshes

UMaterialInterface* ALevelZeroMazeGenerator::ResolveMaterial(UMaterialInterface* Mat, const FLinearColor& FallbackColor)
{
	if (Mat) return Mat;
	if (!FallbackMaterial) return nullptr;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(FallbackMaterial, this);
	MID->SetVectorParameterValue(TEXT("Color"), FallbackColor);
	return MID;
}

void ALevelZeroMazeGenerator::ClearMeshes()
{
	for (UInstancedStaticMeshComponent* ISM : { FloorISM.Get(), AltFloorISM.Get(), CeilingISM.Get(), WallISM.Get(), DoorFrameISM.Get(), PillarISM.Get(), LightPanelISM.Get(), DeadPanelISM.Get(), CollisionISM.Get(), LintelCollisionISM.Get() })
	{
		ISM->ClearInstances();
	}
	Instances.Reset();
	for (UInstancedStaticMeshComponent* ISM : PropISMs)
	{
		if (IsValid(ISM)) { ISM->DestroyComponent(); }
	}
	PropISMs.Reset();
	LightCells.Reset();
	DeadLightCells.Reset();
}

void ALevelZeroMazeGenerator::AddBox(UInstancedStaticMeshComponent* ISM, const FVector& Center, const FVector& SizeCm, std::initializer_list<int32> OwnerCells, bool bCollision)
{
	const FTransform Xf(FQuat::Identity, Center, SizeCm / 100.f); // engine cube is 100 cm, centred
	FInstanceRef Ref;
	Ref.Comp = ISM;
	Ref.Index = ISM->AddInstance(Xf);
	Ref.Xf = Xf;
	for (int32 C : OwnerCells)
	{
		if (C != INDEX_NONE && Ref.NumCells < 4) { Ref.Cells[Ref.NumCells++] = C; }
	}
	Instances.Add(Ref);
	if (bCollision)
	{
		CollisionISM->AddInstance(Xf);
	}
}

void ALevelZeroMazeGenerator::BuildMeshes(FRandomStream& Rng)
{
	ClearMeshes();
	if (!CubeMesh) return;

	struct FSlot { UInstancedStaticMeshComponent* ISM; UMaterialInterface* Mat; FLinearColor Fallback; };
	UMaterialInterface* WallM = WallMaterial, *FloorM = FloorMaterial, *CeilM = CeilingMaterial, *TrimM = DoorFrameMaterial, *PillarM = WallMaterial;
	UMaterialInterface* AltFloorM = FloorMaterial;
	UMaterialInterface* LightM = LightPanelMaterial, *DeadM = DeadPanelMaterial, *ExitM = ExitMaterial, *ShellM = FogShellMaterial;
	if (Theme && Theme->bApplyPalette)
	{
		const ULevelZeroTheme& T = *Theme;
		const FLinearColor PillarC = T.PillarColor.A > 0.f ? T.PillarColor : T.WallColor;
		WallM = ThemedSurface(WallMaterial, &T.WallColor, -1.f);
		FloorM = ThemedSurface(FloorMaterial, &T.FloorColor, -1.f);
		AltFloorM = ThemedSurface(FloorMaterial, &T.AltFloorColor, -1.f);
		CeilM = ThemedSurface(CeilingMaterial, &T.CeilingColor, -1.f);
		TrimM = ThemedSurface(DoorFrameMaterial, &T.TrimColor, -1.f);
		PillarM = ThemedSurface(WallMaterial, &PillarC, -1.f);
		LightM = ThemedSurface(LightPanelMaterial, &T.LightColor, T.LightEmissive);
		DeadM = ThemedSurface(DeadPanelMaterial, nullptr, -1.f);
		ExitM = ThemedSurface(ExitMaterial, nullptr, -1.f);
		ShellM = ThemedSurface(FogShellMaterial, &T.FogColor, -1.f);
	}
	const FSlot Slots[] = {
		{ FloorISM, FloorM, FLinearColor(0.42f, 0.36f, 0.18f) },
		{ AltFloorISM, AltFloorM, FLinearColor(0.2f, 0.2f, 0.2f) },
		{ CeilingISM, CeilM, FLinearColor(0.78f, 0.75f, 0.62f) },
		{ WallISM, WallM, FLinearColor(0.78f, 0.68f, 0.36f) },
		{ DoorFrameISM, TrimM, FLinearColor(0.62f, 0.53f, 0.28f) },
		{ PillarISM, PillarM, FLinearColor(0.78f, 0.68f, 0.36f) },
		{ LightPanelISM, LightM, FLinearColor(1.f, 0.97f, 0.8f) },
		{ DeadPanelISM, DeadM, FLinearColor(0.25f, 0.24f, 0.2f) },
		{ CollisionISM, nullptr, FLinearColor::White },
		{ LintelCollisionISM, nullptr, FLinearColor::White },
	};
	for (const FSlot& S : Slots)
	{
		S.ISM->SetStaticMesh(CubeMesh);
		if (S.ISM != CollisionISM && S.ISM != LintelCollisionISM)
		{
			S.ISM->SetMaterial(0, ResolveMaterial(S.Mat, S.Fallback));
		}
	}

	const float CS = CellSize, T = WallThickness;

	// Floor + ceiling tiles, ceiling light panels.
	NumLitPanels = NumDeadPanels = 0;
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 C = Idx(X, Y);
			const FVector Center((X + 0.5f) * CS, (Y + 0.5f) * CS, 0.f);
			const float CH = GetCellCeiling(C);
			AddBox(IsAltFloor(X, Y) ? AltFloorISM.Get() : FloorISM.Get(), Center + FVector(0, 0, -5.f), FVector(CS, CS, 10.f), { C }, false);
			AddBox(CeilingISM, Center + FVector(0, 0, CH + 5.f), FVector(CS, CS, 10.f), { C }, false);

			const bool bHall = CellTypes[C] == static_cast<uint8>(ELZCellType::PillarHall);
			const float Roll = Rng.FRand();
			const FVector PanelSize = (Rng.FRand() < 0.5f) ? FVector(120.f, 60.f, 2.f) : FVector(60.f, 120.f, 2.f);
			if (!IsLightCell(X, Y)) { continue; } // rolls above are always drawn, so spacing never changes the layout stream
			if (CeilingLightMesh && bPlaceProps)
			{
				// Regular fixture grid: every cell gets a light; a few stay dead.
				if (Roll < DeadPanelChance) { DeadLightCells.Add(FIntPoint(X, Y)); ++NumDeadPanels; }
				else if (bAllLightCellsLit || Roll < DeadPanelChance + LitPanelChance) { LightCells.Add(FIntPoint(X, Y)); ++NumLitPanels; }
			}
			else if (bHall || Roll < LitPanelChance)
			{
				AddBox(LightPanelISM, Center + FVector(0, 0, CH - 1.f), PanelSize, { C }, false);
				++NumLitPanels;
			}
			else if (Roll < LitPanelChance + DeadPanelChance)
			{
				AddBox(DeadPanelISM, Center + FVector(0, 0, CH - 1.f), PanelSize, { C }, false);
				++NumDeadPanels;
			}
		}
	}

	// Walls and doorways. A boundary module belongs to both adjacent cells (visible if either is).
	NumWallSegments = 0;
	const float Post = FMath::Max((CS - DoorWidth) * 0.5f, T);
	auto AddBoundary = [&](bool bAlongY, float Fixed, float SpanStart, int32 CellA, int32 CellB, bool bOpen, bool bDoor)
	{
		// bAlongY: wall runs along Y at X = Fixed, spanning [SpanStart, SpanStart + CS]
		auto MakeCenter = [&](float AlongCenter, float Z) { return bAlongY ? FVector(Fixed, AlongCenter, Z) : FVector(AlongCenter, Fixed, Z); };
		auto MakeSize = [&](float AlongLen, float ZLen) { return bAlongY ? FVector(T, AlongLen, ZLen) : FVector(AlongLen, T, ZLen); };
		// Ceilings can differ per cell type: walls rise to the taller side; open boundaries get a header over the step.
		const float CA = GetCellCeiling(CellA), CB = CellB != INDEX_NONE ? GetCellCeiling(CellB) : CA;
		const float Top = FMath::Max(CA, CB), Low = FMath::Min(CA, CB);
		const float HW = PartitionHeight > 0.f ? FMath::Min(PartitionHeight, Top) : Top;
		if (bOpen && PartitionHeight <= 0.f && Top - Low > 1.f)
		{
			AddBox(WallISM, MakeCenter(SpanStart + CS * 0.5f, (Low + Top) * 0.5f), MakeSize(CS + T, Top - Low), { CellA, CellB }, false);
		}
		if (!bOpen)
		{
			AddBox(WallISM, MakeCenter(SpanStart + CS * 0.5f, HW * 0.5f), MakeSize(CS + T, HW), { CellA, CellB }, true);
			++NumWallSegments;
		}
		else if (bDoor)
		{
			const float PostLen = Post + T * 0.5f;
			AddBox(DoorFrameISM, MakeCenter(SpanStart - T * 0.5f + PostLen * 0.5f, HW * 0.5f), MakeSize(PostLen, HW), { CellA, CellB }, true);
			AddBox(DoorFrameISM, MakeCenter(SpanStart + CS + T * 0.5f - PostLen * 0.5f, HW * 0.5f), MakeSize(PostLen, HW), { CellA, CellB }, true);
			if (HW > DoorHeight + 1.f) // low partitions have open-topped gaps instead of lintels
			{
				const FVector LintelCenter = MakeCenter(SpanStart + CS * 0.5f, (DoorHeight + HW) * 0.5f);
				const FVector LintelSize = MakeSize(CS - 2.f * Post, HW - DoorHeight);
				AddBox(DoorFrameISM, LintelCenter, LintelSize, { CellA, CellB }, false);
				LintelCollisionISM->AddInstance(FTransform(FQuat::Identity, LintelCenter, LintelSize / 100.f));
			}
		}
	};
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 C = Idx(X, Y);
			if (X == 0) AddBoundary(true, 0.f, Y * CS, C, INDEX_NONE, false, false);
			if (Y == 0) AddBoundary(false, 0.f, X * CS, C, INDEX_NONE, false, false);
			if (X + 1 < Width) AddBoundary(true, (X + 1) * CS, Y * CS, C, Idx(X + 1, Y), EastOpen[C], EastDoor[C]);
			else AddBoundary(true, Width * CS, Y * CS, C, INDEX_NONE, false, false);
			if (Y + 1 < Height) AddBoundary(false, (Y + 1) * CS, X * CS, C, Idx(X, Y + 1), SouthOpen[C], SouthDoor[C]);
			else AddBoundary(false, Height * CS, X * CS, C, INDEX_NONE, false, false);
		}
	}

	// Pillars on interior corners of pillar halls.
	for (const FIntPoint& P : PillarCorners)
	{
		const float PH = GetCellCeiling(Idx(P.X, P.Y));
		AddBox(PillarISM, FVector(P.X * CS, P.Y * CS, PH * 0.5f), FVector(PillarSize, PillarSize, PH),
			{ Idx(P.X - 1, P.Y - 1), Idx(P.X, P.Y - 1), Idx(P.X - 1, P.Y), Idx(P.X, P.Y) }, true);
	}

	// Balcony ring on pillar-hall edge cells (greybox upper floor, not walkable), railing on the void side.
	if (BalconyHeight > 0.f)
	{
		auto IsHall = [&](const FIntPoint& P) { return IsValidCell(P) && CellTypes[Idx(P)] == static_cast<uint8>(ELZCellType::PillarHall); };
		for (int32 Y = 0; Y < Height; ++Y)
			for (int32 X = 0; X < Width; ++X)
			{
				const FIntPoint P(X, Y);
				if (!IsHall(P)) continue;
				bool bEdge = false;
				for (const FIntPoint& D : GDirs) { bEdge |= !IsHall(P + D); }
				for (int32 dy = -1; dy <= 1 && !bEdge; ++dy) for (int32 dx = -1; dx <= 1; ++dx) { bEdge |= !IsHall(P + FIntPoint(dx, dy)); }
				if (!bEdge) continue;
				const int32 C = Idx(P);
				const FVector Center((X + 0.5f) * CS, (Y + 0.5f) * CS, BalconyHeight);
				AddBox(FloorISM, Center, FVector(CS, CS, 25.f), { C }, false);
				for (const FIntPoint& D : GDirs)
				{
					const FIntPoint N = P + D;
					if (!IsHall(N)) continue;
					bool bNEdge = false;
					for (int32 dy = -1; dy <= 1; ++dy) for (int32 dx = -1; dx <= 1; ++dx) { bNEdge |= !IsHall(N + FIntPoint(dx, dy)); }
					if (bNEdge) continue; // neighbour is balcony too
					const FVector Edge = Center + FVector(D.X, D.Y, 0) * (CS * 0.5f) + FVector(0, 0, 55.f);
					AddBox(DoorFrameISM, Edge, D.X != 0 ? FVector(12.f, CS, 100.f) : FVector(CS, 12.f, 100.f), { C, Idx(N) }, false);
				}
			}
	}

	// Seat tiers in rooms (cinema): rows rising with distance from the room's south edge. Visual only.
	if (bSeatTiersInRooms)
	{
		for (int32 Y = 0; Y < Height; ++Y)
			for (int32 X = 0; X < Width; ++X)
			{
				const int32 C = Idx(X, Y);
				if (CellTypes[C] != static_cast<uint8>(ELZCellType::Room)) continue;
				int32 Depth = 0;
				for (int32 yy = Y + 1; yy < Height && CellTypes[Idx(X, yy)] == static_cast<uint8>(ELZCellType::Room); ++yy) { ++Depth; }
				if (Depth == 0) continue; // front row = screen side, left clear
				for (int32 r = 0; r < 2; ++r)
				{
					const float Hgt = FMath::Min(40.f + 22.f * (Depth * 2 + r), 280.f);
					AddBox(DoorFrameISM, FVector((X + 0.5f) * CS, (Y + 0.25f + 0.5f * r) * CS, Hgt * 0.5f), FVector(CS * 0.92f, 55.f, Hgt), { C }, false);
				}
			}
	}

	// Floor collision slab.
	FloorCollision->SetRelativeLocation(FVector(Width * CS * 0.5f, Height * CS * 0.5f, -5.f));
	FloorCollision->SetBoxExtent(FVector(Width * CS * 0.5f, Height * CS * 0.5f, 5.f));

	// Exit: glowing slab + trigger (step 4 wires the win screen to OnExitReached).
	const FVector ExitCenter((ExitCell.X + 0.5f) * CS, (ExitCell.Y + 0.5f) * CS, 0.f);
	ExitMarker->SetStaticMesh(CubeMesh);
	ExitMarker->SetMaterial(0, ResolveMaterial(ExitM, FLinearColor(0.2f, 1.f, 0.35f)));
	ExitMarker->SetRelativeLocation(ExitCenter + FVector(0, 0, 110.f));
	ExitMarker->SetRelativeScale3D(FVector(0.9f, 0.9f, 2.2f));
	ExitTrigger->SetRelativeLocation(ExitCenter + FVector(0, 0, 150.f));
	ExitTrigger->SetBoxExtent(FVector(CS * 0.4f, CS * 0.4f, 150.f));

	// Haze shell: far enough out that distance fog has fully saturated on it.
	FogShell->SetStaticMesh(CubeMesh);
	FogShell->SetMaterial(0, ResolveMaterial(ShellM, FLinearColor(0.42f, 0.36f, 0.16f)));
	const float MaxH = GetMaxCeiling();
	FogShell->SetRelativeLocation(FVector(Width * CS * 0.5f, Height * CS * 0.5f, MaxH * 0.5f));
	FogShell->SetRelativeScale3D(FVector((Width * CS + 8000.f) / 100.f, (Height * CS + 8000.f) / 100.f, (MaxH + 8000.f) / 100.f));
}

void ALevelZeroMazeGenerator::SetExitMarkerVisible(bool bVisible)
{
	ExitMarker->SetVisibility(bVisible);
}

// ------------------------------------------------------------------ Props

UInstancedStaticMeshComponent* ALevelZeroMazeGenerator::MakePropISM(UStaticMesh* Mesh, bool bCollision)
{
	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
	ISM->SetupAttachment(Root);
	ISM->SetMobility(EComponentMobility::Movable);
	ISM->SetStaticMesh(Mesh);
	ISM->SetCastShadow(false);
	if (!bCollision && Theme && Mesh)
	{
		for (int32 i = 0; i < Mesh->GetStaticMaterials().Num(); ++i) { ISM->SetMaterial(i, GetThemedMaterial(Mesh->GetMaterial(i), true)); }
	}
	if (bCollision)
	{
		// Never streamed, so moving around never dirties the nav mesh.
		ISM->SetVisibility(false);
		ISM->SetHiddenInGame(true);
		ISM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		ISM->SetCanEverAffectNavigation(true);
	}
	else
	{
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->SetCanEverAffectNavigation(false);
	}
	ISM->RegisterComponent();
	PropISMs.Add(ISM);
	return ISM;
}

void ALevelZeroMazeGenerator::BuildProps()
{
	NumPropInstances = 0;
	PropViews.Reset();
	PropSummary = TEXT("props off");
	if (!bPlaceProps || CellTypes.Num() != Width * Height) return;
	FRandomStream R(Seed * 7919 + 1013); // independent of the layout stream
	const float CS = CellSize, T = WallThickness, HW = GetWallTop();
	struct FPair { UInstancedStaticMeshComponent* Render = nullptr; UInstancedStaticMeshComponent* Coll = nullptr; };
	TMap<UStaticMesh*, FPair> ISMs;
	auto Add = [&](UStaticMesh* Mesh, const FTransform& Xf, const FIntPoint& Cell, bool bCollision)
	{
		FPair& P = ISMs.FindOrAdd(Mesh);
		if (!P.Render) { P.Render = MakePropISM(Mesh, false); }
		if (bCollision && !P.Coll) { P.Coll = MakePropISM(Mesh, true); }
		FInstanceRef Ref;
		Ref.Comp = P.Render;
		Ref.Index = P.Render->AddInstance(Xf);
		Ref.Xf = Xf;
		Ref.Cells[0] = Idx(Cell);
		Ref.NumCells = 1;
		Instances.Add(Ref);
		if (bCollision) { P.Coll->AddInstance(Xf); }
		++NumPropInstances;
	};
	auto CellCenter = [&](const FIntPoint& C) { return FVector((C.X + 0.5f) * CS, (C.Y + 0.5f) * CS, 0.f); };
	auto Closed = [&](const FIntPoint& C, const FIntPoint& D) { const FIntPoint N = C + D; return !IsValidCell(N) || !IsPassable(C, N); };
	// The mesh's own front axis (local), from PropYawOffset: front = +X rotated by -PropYawOffset.
	const FVector LocalFront = FRotator(0.f, -PropYawOffset, 0.f).RotateVector(FVector::ForwardVector);
	const FVector LocalSide(-LocalFront.Y, LocalFront.X, 0.f);
	auto Extent = [](UStaticMesh* M, const FVector& Axis, bool bMin)
	{
		const FBox B = M->GetBoundingBox();
		float V = bMin ? FLT_MAX : -FLT_MAX;
		for (int32 i = 0; i < 8; ++i)
		{
			const FVector P((i & 1) ? B.Max.X : B.Min.X, (i & 2) ? B.Max.Y : B.Min.Y, 0.f);
			const float d = FVector::DotProduct(P, Axis);
			V = bMin ? FMath::Min(V, d) : FMath::Max(V, d);
		}
		return V;
	};
	auto HalfWidth = [&](UStaticMesh* M) { return FMath::Max(FMath::Abs(Extent(M, LocalSide, true)), FMath::Abs(Extent(M, LocalSide, false))); };
	// Back of the mesh against the wall face, front into the cell. Meshes are pivoted at the bottom centre.
	auto WallXf = [&](UStaticMesh* M, const FIntPoint& C, const FIntPoint& D, float Lateral, float Z, float YawJitter)
	{
		const float Back = -Extent(M, LocalFront, true); // pivot -> back face
		const FVector Dir(D.X, D.Y, 0.f), Side(-D.Y, D.X, 0.f);
		const FVector Loc = CellCenter(C) + Dir * (CS * 0.5f - T * 0.5f - Back - 3.f) + Side * Lateral + FVector(0, 0, Z);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-float(D.Y), -float(D.X))) + PropYawOffset + YawJitter;
		return FTransform(FRotator(0.f, Yaw, 0.f), Loc);
	};
	const TArray<int32> Dist = BFS(StartCell);
	TSet<int32> UsedWalls; // Idx * 4 + dir
	auto DirIndex = [](const FIntPoint& D) { for (int32 i = 0; i < 4; ++i) if (GDirs[i] == D) return i; return 0; };
	auto FreeWall = [&](const FIntPoint& C, FIntPoint& OutDir) -> bool
	{
		int32 Order[4] = { 0, 1, 2, 3 };
		for (int32 i = 3; i > 0; --i) { Swap(Order[i], Order[R.RandRange(0, i)]); }
		for (int32 k : Order)
		{
			if (Closed(C, GDirs[k]) && !UsedWalls.Contains(Idx(C) * 4 + k)) { OutDir = GDirs[k]; return true; }
		}
		return false;
	};
	auto Shuffle = [&](TArray<FIntPoint>& A) { for (int32 i = A.Num() - 1; i > 0; --i) { A.Swap(i, R.RandRange(0, i)); } };
	auto Cheb = [](const FIntPoint& A, const FIntPoint& B) { return FMath::Max(FMath::Abs(A.X - B.X), FMath::Abs(A.Y - B.Y)); };

	// 1. Ceiling fixtures on the regular grid (top flush with the ceiling).
	int32 NumLights = 0;
	if (CeilingLightMesh)
	{
		const float LS = FMath::Max(LightFixtureScale, 0.1f);
		const float Drop = CeilingLightMesh->GetBoundingBox().Max.Z * LS + 0.5f;
		for (const FIntPoint& C : LightCells) { Add(CeilingLightMesh, FTransform(FRotator::ZeroRotator, CellCenter(C) + FVector(0, 0, GetCellCeiling(Idx(C)) - Drop), FVector(LS)), C, false); ++NumLights; }
		// Dead fixtures: same panel, own ISM with a dimmed, non-glowing material (could flicker later).
		if (DeadLightCells.Num() > 0)
		{
			UInstancedStaticMeshComponent* Dead = MakePropISM(CeilingLightMesh, false);
			if (UMaterialInterface* Base = GetThemedMaterial(CeilingLightMesh->GetMaterial(0), true))
			{
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
				MID->SetScalarParameterValue(TEXT("Glow"), 0.f);
				MID->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.35f, 0.34f, 0.3f));
				Dead->SetMaterial(0, MID);
			}
			for (const FIntPoint& C : DeadLightCells)
			{
				const FTransform Xf(FRotator::ZeroRotator, CellCenter(C) + FVector(0, 0, GetCellCeiling(Idx(C)) - Drop), FVector(LS));
				FInstanceRef Ref; Ref.Comp = Dead; Ref.Index = Dead->AddInstance(Xf); Ref.Xf = Xf; Ref.Cells[0] = Idx(C); Ref.NumCells = 1;
				Instances.Add(Ref); ++NumPropInstances;
			}
		}
	}

	// 2. Wall vents high on a few closed walls.
	int32 NumVents = 0;
	const float VentZ = WallVentMesh ? FMath::Min(WallVentZ, HW - WallVentMesh->GetBoundingBox().Max.Z - 15.f) : 0.f;
	if (WallVentMesh && VentZ >= 120.f)
	{
		const float Lat = CS * 0.5f - HalfWidth(WallVentMesh) - 40.f;
		for (int32 Y = 0; Y < Height; ++Y)
			for (int32 X = 0; X < Width; ++X)
				for (int32 k = 0; k < 4; ++k)
				{
					const FIntPoint C(X, Y);
					if (Closed(C, GDirs[k]) && R.FRand() < WallVentChance)
					{
						Add(WallVentMesh, WallXf(WallVentMesh, C, GDirs[k], R.FRandRange(-Lat, Lat), VentZ, 0.f), C, false);
						UsedWalls.Add(Idx(C) * 4 + k);
						++NumVents;
					}
				}
	}

	// 3. Landmarks: rare set pieces in rooms/halls far from the start.
	TArray<FIntPoint> Taken;
	int32 NumLandmarks = 0;
	for (UStaticMesh* M : LandmarkMeshes)
	{
		if (!M) continue;
		TArray<FIntPoint> Cand;
		for (int32 Y = 0; Y < Height; ++Y)
			for (int32 X = 0; X < Width; ++X)
			{
				const FIntPoint C(X, Y);
				if ((bLandmarksInCorridors || CellTypes[Idx(C)] != static_cast<uint8>(ELZCellType::Corridor)) && Dist[Idx(C)] >= LandmarkMinStartDistance && C != ExitCell) { Cand.Add(C); }
			}
		Shuffle(Cand);
		const int32 Want = R.RandRange(FMath::Max(0, LandmarksPerMesh.X), FMath::Max(LandmarksPerMesh.X, LandmarksPerMesh.Y));
		int32 Placed = 0;
		for (const FIntPoint& C : Cand)
		{
			if (Placed >= Want) break;
			bool bFar = true;
			for (const FIntPoint& O : Taken) { bFar &= Cheb(C, O) >= LandmarkSpacing; }
			FIntPoint D;
			if (!bFar || !FreeWall(C, D)) continue;
			Add(M, WallXf(M, C, D, 0.f, 0.f, 0.f), C, true);
			PropViews.Add({ CellCenter(C) - FVector(D.X, D.Y, 0) * 150.f + FVector(0, 0, 165.f), WallXf(M, C, D, 0.f, 0.f, 0.f).GetLocation() + FVector(0, 0, 90.f) });
			UsedWalls.Add(Idx(C) * 4 + DirIndex(D));
			Taken.Add(C);
			++Placed;
			++NumLandmarks;
		}
	}

	// 4. Abandoned-office clusters in rooms and dead ends.
	int32 NumClutter = 0, NumClusters = 0;
	if (ClutterWallMeshes.Num() > 0 || ClutterFreeMeshes.Num() > 0)
	{
		TArray<FIntPoint> Cand;
		for (int32 Y = 0; Y < Height; ++Y)
			for (int32 X = 0; X < Width; ++X)
			{
				const FIntPoint C(X, Y);
				if (C == StartCell || C == ExitCell || Dist[Idx(C)] < 3) continue;
				int32 Open = 0;
				for (const FIntPoint& D : GDirs) { Open += Closed(C, D) ? 0 : 1; }
				const bool bRoom = CellTypes[Idx(C)] == static_cast<uint8>(ELZCellType::Room);
				if (bRoom || Open == 1 || (bClustersInCorridors && Open <= 2)) { Cand.Add(C); }
			}
		Shuffle(Cand);
		for (const FIntPoint& C : Cand)
		{
			if (NumClusters >= NumClutterClusters) break;
			bool bFar = true;
			for (const FIntPoint& O : Taken) { bFar &= Cheb(C, O) >= 3; }
			if (!bFar) continue;
			Taken.Add(C);
			++NumClusters;
			const int32 NWall = ClutterWallMeshes.Num() > 0 ? R.RandRange(ClusterWallItems.X, FMath::Max(ClusterWallItems.X, ClusterWallItems.Y)) : 0;
			const int32 NFree = ClutterFreeMeshes.Num() > 0 ? R.RandRange(ClusterFreeItems.X, FMath::Max(ClusterFreeItems.X, ClusterFreeItems.Y)) : 0;
			if (PropViews.Num() < 12)
			{
				// View from the cluster cell's centre, looking at the first closed wall (where the wall pieces go).
				for (const FIntPoint& D : GDirs)
				{
					if (Closed(C, D)) { PropViews.Add({ CellCenter(C) - FVector(D.X, D.Y, 0) * 150.f + FVector(0, 0, 165.f), CellCenter(C) + FVector(D.X, D.Y, 0) * 120.f + FVector(0, 0, 50.f) }); break; }
				}
			}
			for (int32 i = 0; i < NWall; ++i)
			{
				UStaticMesh* M = ClutterWallMeshes[R.RandRange(0, ClutterWallMeshes.Num() - 1)];
				FIntPoint D;
				if (!M || !FreeWall(C, D)) break;
				const float Lat = FMath::Max(CS * 0.5f - T * 0.5f - HalfWidth(M) - 70.f, 0.f);
				Add(M, WallXf(M, C, D, R.FRandRange(-Lat, Lat), 0.f, R.FRandRange(-6.f, 6.f)), C, true);
				UsedWalls.Add(Idx(C) * 4 + DirIndex(D));
				++NumClutter;
			}
			for (int32 i = 0; i < NFree; ++i)
			{
				UStaticMesh* M = ClutterFreeMeshes[R.RandRange(0, ClutterFreeMeshes.Num() - 1)];
				if (!M) continue;
				const float A = R.FRandRange(0.f, 2.f * PI), Rad = R.FRandRange(90.f, 140.f) * FMath::Max(1.f, CS / 400.f);
				const FVector Loc = CellCenter(C) + FVector(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad, 0.f);
				Add(M, FTransform(FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), Loc), C, true);
				++NumClutter;
			}
		}
	}
	PropSummary = FString::Printf(TEXT("props: lights=%d vents=%d clusters=%d clutter=%d landmarks=%d instances=%d meshes=%d"),
		NumLights, NumVents, NumClusters, NumClutter, NumLandmarks, NumPropInstances, ISMs.Num());
}

// ------------------------------------------------------------------ Navigation

void ALevelZeroMazeGenerator::EnsureNavMeshBounds()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (!World) return;
	ANavMeshBoundsVolume* Volume = nullptr;
	TActorIterator<ANavMeshBoundsVolume> It(World); // reuse the first bounds volume
	if (It) { Volume = *It; }
	const FVector Center = GetActorTransform().TransformPosition(FVector(Width * CellSize * 0.5f, Height * CellSize * 0.5f, GetMaxCeiling() * 0.5f));
	if (!Volume)
	{
		Volume = World->SpawnActor<ANavMeshBoundsVolume>(Center, FRotator::ZeroRotator);
	}
	if (!Volume) return;
	Volume->SetActorLocation(Center);
	UCubeBuilder* Builder = NewObject<UCubeBuilder>();
	Builder->X = Width * CellSize + 200.f;
	Builder->Y = Height * CellSize + 200.f;
	Builder->Z = GetMaxCeiling() + 200.f;
	UActorFactory::CreateBrushForVolumeActor(Volume, Builder);
	Volume->SetActorLabel(TEXT("LevelZeroNavBounds"));
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		Nav->OnNavigationBoundsUpdated(Volume);
	}
	UE_LOG(LogLevelZeroMaze, Log, TEXT("Nav bounds: %s size %.0f x %.0f x %.0f"), *Center.ToString(), Builder->X, Builder->Y, Builder->Z);
#endif
}

bool ALevelZeroMazeGenerator::BuildRuntimeNav()
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* Nav = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!Nav)
	{
		UE_LOG(LogLevelZeroMaze, Warning, TEXT("No navigation system; monster will use the cell-graph fallback"));
		return false;
	}
	const double T0 = FPlatformTime::Seconds();
	Nav->Build(); // blocks until all tiles are done
	LastNavBuildSeconds = float(FPlatformTime::Seconds() - T0);
	UE_LOG(LogLevelZeroMaze, Log, TEXT("Runtime nav mesh built in %.2fs"), LastNavBuildSeconds);
	return true;
}

// ------------------------------------------------------------------ Streaming

void ALevelZeroMazeGenerator::UpdateStreaming(bool bForce)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld()) return;
	if (!bStreamTiles)
	{
		if (bForce) ShowAllInstances();
		return;
	}

	FVector ViewLoc;
	if (APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		ViewLoc = PCM->GetCameraLocation();
	}
	else if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		ViewLoc = Pawn->GetActorLocation();
	}
	else
	{
		return;
	}

	const FIntPoint Cell = WorldToCell(ViewLoc);
	if (!bForce && Cell == LastStreamCell) return;
	LastStreamCell = Cell;

	TSet<UInstancedStaticMeshComponent*> Dirty;
	for (FInstanceRef& Ref : Instances)
	{
		bool bVisible = false;
		for (int32 i = 0; i < Ref.NumCells && !bVisible; ++i)
		{
			const int32 CX = Ref.Cells[i] % Width, CY = Ref.Cells[i] / Width;
			bVisible = FMath::Max(FMath::Abs(CX - Cell.X), FMath::Abs(CY - Cell.Y)) <= StreamRadiusCells;
		}
		if (bVisible != Ref.bVisible || bForce)
		{
			Ref.bVisible = bVisible;
			const FTransform Xf = bVisible ? Ref.Xf : FTransform(FQuat::Identity, Ref.Xf.GetLocation(), FVector::ZeroVector);
			Ref.Comp->UpdateInstanceTransform(Ref.Index, Xf, false, false, true);
			Dirty.Add(Ref.Comp);
		}
	}
	for (UInstancedStaticMeshComponent* ISM : Dirty)
	{
		ISM->MarkRenderStateDirty();
	}
}

void ALevelZeroMazeGenerator::ShowAllInstances()
{
	TSet<UInstancedStaticMeshComponent*> Dirty;
	for (FInstanceRef& Ref : Instances)
	{
		if (!Ref.bVisible)
		{
			Ref.bVisible = true;
			Ref.Comp->UpdateInstanceTransform(Ref.Index, Ref.Xf, false, false, true);
			Dirty.Add(Ref.Comp);
		}
	}
	for (UInstancedStaticMeshComponent* ISM : Dirty) { ISM->MarkRenderStateDirty(); }
}

int32 ALevelZeroMazeGenerator::GetNumVisibleInstances() const
{
	int32 Count = 0;
	for (const FInstanceRef& Ref : Instances) { Count += Ref.bVisible ? 1 : 0; }
	return Count;
}

// ------------------------------------------------------------------ Player / exit

void ALevelZeroMazeGenerator::PlacePlayerAtStart()
{
	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const FRotator Facing(0.f, GetFacingYaw(StartCell), 0.f);
		Pawn->TeleportTo(GetCellCenterWorld(StartCell, 2.f), Facing, false, true);
		if (AController* C = Pawn->GetController()) { C->SetControlRotation(Facing); }
		UpdateStreaming(true);
	}
}

void ALevelZeroMazeGenerator::HandleExitOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn->IsPlayerControlled())
	{
		UE_LOG(LogLevelZeroMaze, Log, TEXT("Exit reached by %s"), *OtherActor->GetName());
		OnExitReached.Broadcast(OtherActor);
	}
}

// ------------------------------------------------------------------ Debug

FString ALevelZeroMazeGenerator::GetDebugAscii() const
{
	if (CellTypes.Num() != Width * Height) return FString();
	const int32 Cols = Width * 2 + 1, Rows = Height * 2 + 1;
	TArray<FString> Lines;
	Lines.Reserve(Rows);
	auto Boundary = [&](int32 X, int32 Y, bool bEast) -> TCHAR // boundary on the east/south side of (X,Y); X/Y may be -1 for outer edges
	{
		if (bEast)
		{
			if (X < 0 || X >= Width - 1) return TEXT('#');
			return EastOpen[Idx(X, Y)] ? (EastDoor[Idx(X, Y)] ? TEXT('D') : TEXT(' ')) : TEXT('#');
		}
		if (Y < 0 || Y >= Height - 1) return TEXT('#');
		return SouthOpen[Idx(X, Y)] ? (SouthDoor[Idx(X, Y)] ? TEXT('D') : TEXT(' ')) : TEXT('#');
	};
	for (int32 R = 0; R < Rows; ++R)
	{
		FString Line;
		Line.Reserve(Cols);
		for (int32 Cc = 0; Cc < Cols; ++Cc)
		{
			const bool bEvenR = (R % 2) == 0, bEvenC = (Cc % 2) == 0;
			const int32 X = Cc / 2, Y = R / 2; // for odd indices this is the cell index
			TCHAR Ch = TEXT(' ');
			if (!bEvenR && !bEvenC)
			{
				const FIntPoint P(X, Y);
				if (P == StartCell) Ch = TEXT('S');
				else if (P == ExitCell) Ch = TEXT('E');
				else
				{
					const ELZCellType Type = static_cast<ELZCellType>(CellTypes[Idx(P)]);
					Ch = Type == ELZCellType::Room ? TEXT('.') : (Type == ELZCellType::PillarHall ? TEXT(':') : TEXT(' '));
				}
			}
			else if (!bEvenR && bEvenC)
			{
				Ch = Boundary(X - 1, Y, true);
			}
			else if (bEvenR && !bEvenC)
			{
				Ch = Boundary(X, Y - 1, false);
			}
			else
			{
				const FIntPoint Corner(X, Y);
				if (PillarCorners.Contains(Corner)) Ch = TEXT('o');
				else
				{
					const bool bAny =
						(Y > 0 && Boundary(X - 1, Y - 1, true) != TEXT(' ')) ||
						(Y < Height && Boundary(X - 1, Y, true) != TEXT(' ')) ||
						(X > 0 && Boundary(X - 1, Y - 1, false) != TEXT(' ')) ||
						(X < Width && Boundary(X, Y - 1, false) != TEXT(' '));
					Ch = bAny ? TEXT('+') : TEXT(' ');
				}
			}
			Line.AppendChar(Ch);
		}
		Lines.Add(Line);
	}
	return FString::Join(Lines, TEXT("\n"));
}

bool ALevelZeroMazeGenerator::ExportDebugAscii(const FString& FilePath) const
{
	const FString Text = FString::Printf(TEXT("# %s\n# Legend: # wall, D doorway, o pillar, . room, : pillar hall, S start, E exit\n%s\n"), *GetStatsString(), *GetDebugAscii());
	return FFileHelper::SaveStringToFile(Text, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

FString ALevelZeroMazeGenerator::GetStatsString() const
{
	int32 Rooms = 0, Halls = 0;
	for (uint8 T : CellTypes)
	{
		Rooms += T == static_cast<uint8>(ELZCellType::Room) ? 1 : 0;
		Halls += T == static_cast<uint8>(ELZCellType::PillarHall) ? 1 : 0;
	}
	return FString::Printf(TEXT("LevelZeroMaze seed=%d size=%dx%d start=(%d,%d) exit=(%d,%d) bfs=%d walls=%d doorways=%d pillars=%d roomCells=%d hallCells=%d litPanels=%d deadPanels=%d instances=%d genMs=%.1f"),
		Seed, Width, Height, StartCell.X, StartCell.Y, ExitCell.X, ExitCell.Y, StartToExitDistance, NumWallSegments, NumDoorways,
		PillarCorners.Num(), Rooms, Halls, NumLitPanels, NumDeadPanels, Instances.Num(), LastGenerateMs) + TEXT(" ") + PropSummary + TEXT(" level=") + GetLevelId().ToString();
}

void ALevelZeroMazeGenerator::BeginShotModeIfRequested()
{
	FString Path;
	if (FParse::Value(FCommandLine::Get(), TEXT("LZPropShots="), FlyDir) && PropViews.Num() > 0)
	{
		bPropShots = true;
		PropShotIndex = PropShotHold = 0;
		FlyPoints = { PropViews[0].Key, PropViews[0].Value };
		FApp::SetUseFixedTimeStep(true);
		FApp::SetFixedDeltaTime(1.0 / 30.0);
		SetActorTickInterval(0.f);
		bShotTopDown = false;
		ShotPhase = 0;
		ShotElapsed = ShotQuietTime = 0.f;
		GetWorldTimerManager().SetTimer(ShotTimer, this, &ALevelZeroMazeGenerator::TickShotMode, 0.5f, true, 1.0f);
		UE_LOG(LogLevelZeroMaze, Log, TEXT("Prop shot mode: %d views -> %s"), PropViews.Num(), *FlyDir);
		return;
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("LZFlythrough="), FlyDir))
	{
		int32 Cells = 24;
		FParse::Value(FCommandLine::Get(), TEXT("LZFlyCells="), Cells);
		FParse::Value(FCommandLine::Get(), TEXT("LZFlySpeed="), FlySpeed);
		FParse::Value(FCommandLine::Get(), TEXT("LZFlyFrames="), FlyMaxFrames);
		const TArray<FIntPoint> Path2 = FindPath(StartCell, ExitCell);
		for (int32 i = 0; i < Path2.Num() && i < Cells; ++i) { FlyPoints.Add(GetCellCenterWorld(Path2[i], 165.f)); }
		FApp::SetUseFixedTimeStep(true);
		FApp::SetFixedDeltaTime(1.0 / 30.0);
		SetActorTickInterval(0.f);
		PendingShotPath = FlyDir / TEXT("frame_00000.png");
		bShotTopDown = false;
		ShotPhase = 0;
		ShotElapsed = ShotQuietTime = 0.f;
		GetWorldTimerManager().SetTimer(ShotTimer, this, &ALevelZeroMazeGenerator::TickShotMode, 0.5f, true, 1.0f);
		UE_LOG(LogLevelZeroMaze, Log, TEXT("Flythrough mode: %d path cells -> %s"), FlyPoints.Num(), *FlyDir);
		return;
	}
	if (!FParse::Value(FCommandLine::Get(), TEXT("LZShot="), Path)) return;
	PendingShotPath = Path;
	bShotTopDown = FParse::Param(FCommandLine::Get(), TEXT("LZTopDown"));
	ShotPhase = 0;
	ShotElapsed = ShotQuietTime = 0.f;
	GetWorldTimerManager().SetTimer(ShotTimer, this, &ALevelZeroMazeGenerator::TickShotMode, 0.5f, true, 1.0f);
	UE_LOG(LogLevelZeroMaze, Log, TEXT("Shot mode: %s (%s)"), *PendingShotPath, bShotTopDown ? TEXT("top-down") : TEXT("POV"));
}

void ALevelZeroMazeGenerator::TickShotMode()
{
	ShotElapsed += 0.5f;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (ShotPhase == 0)
	{
		FVector Loc;
		FRotator Rot;
		float Fov = 90.f;
		if (bShotTopDown)
		{
			bStreamTiles = false;
			ShowAllInstances();
			CeilingISM->SetVisibility(false);
			LightPanelISM->SetVisibility(false);
			DeadPanelISM->SetVisibility(false);
			FogShell->SetVisibility(false);
			for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It) { It->GetComponent()->SetVisibility(false); }
			Fov = 30.f;
			const float Extent = FMath::Max(Width, Height) * CellSize * 0.55f;
			Loc = GetCellCenterWorld(FIntPoint(0, 0), 0.f) + FVector((Width - 1) * CellSize * 0.5f, (Height - 1) * CellSize * 0.5f, Extent / FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f)));
			Rot = FRotator(-90.f, 0.f, 0.f);
		}
		else
		{
			Loc = GetCellCenterWorld(StartCell, 165.f);
			Rot = FRotator(-3.f, GetFacingYaw(StartCell), 0.f);
		}
		if (FlyPoints.Num() > 1)
		{
			Loc = FlyPoints[0];
			const FVector Dir = (FlyPoints[1] - FlyPoints[0]).GetSafeNormal();
			Rot = FRotator(-2.f, Dir.Rotation().Yaw, 0.f);
			FlyRot = Rot;
		}
		ACameraActor* Cam = GetWorld()->SpawnActor<ACameraActor>(Loc, Rot);
		CaptureCamera = Cam;
		if (Cam && PC)
		{
			Cam->GetCameraComponent()->SetFieldOfView(Fov);
			Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
			PC->SetViewTarget(Cam);
		}
		UpdateStreaming(true);
		ShotPhase = 1;
		return;
	}
	if (ShotPhase == 1)
	{
		int32 Remaining = 0;
#if WITH_EDITOR
		if (GShaderCompilingManager) { Remaining += GShaderCompilingManager->GetNumRemainingJobs(); }
		Remaining += FAssetCompilingManager::Get().GetNumRemainingAssets();
#endif
		ShotQuietTime = (Remaining == 0) ? ShotQuietTime + 0.5f : 0.f;
		if (((ShotQuietTime >= 3.f && ShotElapsed >= 6.f) || ShotElapsed > 900.f) && FlyPoints.Num() > 1)
		{
			UE_LOG(LogLevelZeroMaze, Log, TEXT("Flythrough capture starting"));
			GetWorldTimerManager().ClearTimer(ShotTimer);
			bFlying = true;
			FlyDistance = 0.f;
			FlyFrame = 0;
			return;
		}
		if ((ShotQuietTime >= 3.f && ShotElapsed >= 6.f) || ShotElapsed > 900.f)
		{
			UE_LOG(LogLevelZeroMaze, Log, TEXT("Requesting screenshot %s (visible instances %d/%d)"), *PendingShotPath, GetNumVisibleInstances(), Instances.Num());
			FScreenshotRequest::RequestScreenshot(PendingShotPath, false, false);
			ShotPhase = 2;
			ShotExitAt = ShotElapsed + 3.f;
		}
		return;
	}
	if (ShotPhase == 2 && ShotElapsed >= ShotExitAt)
	{
		UE_LOG(LogLevelZeroMaze, Log, TEXT("Shot done, exiting"));
		GetWorldTimerManager().ClearTimer(ShotTimer);
		FPlatformMisc::RequestExit(false, TEXT("LevelZeroShot"));
	}
}

FVector ALevelZeroMazeGenerator::SampleFly(float Distance) const
{
	// Piecewise-linear path through cell centres with corners rounded by blending two segments.
	float Remaining = FMath::Max(Distance, 0.f);
	for (int32 i = 0; i + 1 < FlyPoints.Num(); ++i)
	{
		const float SegLen = FVector::Dist(FlyPoints[i], FlyPoints[i + 1]);
		if (Remaining <= SegLen)
		{
			return FMath::Lerp(FlyPoints[i], FlyPoints[i + 1], SegLen > 0.f ? Remaining / SegLen : 0.f);
		}
		Remaining -= SegLen;
	}
	return FlyPoints.Last();
}

void ALevelZeroMazeGenerator::TickFlythrough(float DeltaSeconds)
{
	if (bPropShots)
	{
		if (!CaptureCamera || PropShotIndex >= PropViews.Num())
		{
			UE_LOG(LogLevelZeroMaze, Log, TEXT("Prop shots done: %d"), PropShotIndex);
			bFlying = false;
			FPlatformMisc::RequestExit(false, TEXT("LevelZeroPropShots"));
			return;
		}
		const TPair<FVector, FVector>& V = PropViews[PropShotIndex];
		CaptureCamera->SetActorLocationAndRotation(V.Key, (V.Value - V.Key).Rotation());
		if (++PropShotHold == 15) // let streaming + texture mips settle
		{
			FScreenshotRequest::RequestScreenshot(FlyDir / FString::Printf(TEXT("prop_%02d.png"), PropShotIndex), false, false);
		}
		if (PropShotHold >= 18) { PropShotHold = 0; ++PropShotIndex; }
		return;
	}
	float Total = 0.f;
	for (int32 i = 0; i + 1 < FlyPoints.Num(); ++i) { Total += FVector::Dist(FlyPoints[i], FlyPoints[i + 1]); }
	const bool bOutOfFrames = FlyMaxFrames > 0 && FlyFrame >= FlyMaxFrames;
	const bool bPathDone = FlyDistance >= Total && !bFlyPaused && FlyMaxFrames <= 0;
	if (!CaptureCamera || bOutOfFrames || bPathDone)
	{
		UE_LOG(LogLevelZeroMaze, Log, TEXT("Flythrough done: %d frames"), FlyFrame);
		bFlying = false;
		FPlatformMisc::RequestExit(false, TEXT("LevelZeroFlythrough"));
		return;
	}
	const float Dt = 1.f / 30.f; // fixed step, independent of real frame time
	if (!bFlyPaused)
	{
		FlyDistance = FMath::Min(FlyDistance + FlySpeed * Dt, Total);
	}
	// Smooth the corner by averaging samples around the current distance; look ahead ~1.5 cells.
	const FVector Pos = (SampleFly(FlyDistance - 120.f) + SampleFly(FlyDistance) * 2.f + SampleFly(FlyDistance + 120.f)) * 0.25f;
	const FVector Ahead = SampleFly(FlyDistance + CellSize * 1.5f);
	const float BobZ = FMath::Sin(FlyDistance / 75.f * PI) * 1.5f; // subtle head bob (walking)
	FRotator Target = (Ahead - Pos).Rotation();
	Target.Pitch = -2.f;
	FlyRot = FMath::RInterpTo(FlyRot, Target, Dt, 2.5f);
	CaptureCamera->SetActorLocationAndRotation(Pos + FVector(0, 0, BobZ), FlyRot);
	FScreenshotRequest::RequestScreenshot(FlyDir / FString::Printf(TEXT("frame_%05d.png"), FlyFrame++), false, false);
}
