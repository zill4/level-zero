// Level Zero - seeded procedural "Level 0" maze built from instanced greybox modules.
#include "LevelZeroMazeGenerator.h"

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
#if WITH_EDITOR
#include "ShaderCompiler.h"
#include "AssetCompilingManager.h"
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
	UpdateStreaming(false);
}

// ------------------------------------------------------------------ Layout

void ALevelZeroMazeGenerator::Generate()
{
	const double T0 = FPlatformTime::Seconds();
	Width = FMath::Clamp(Width, 4, 64);
	Height = FMath::Clamp(Height, 4, 64);

	FRandomStream Rng(Seed);
	BuildLayout(Rng);
	BuildMeshes(Rng);

	if (PlayerStartActor)
	{
		PlayerStartActor->SetActorLocationAndRotation(GetCellCenterWorld(StartCell, 100.f), FRotator(0.f, GetFacingYaw(StartCell), 0.f));
	}
	LastStreamCell = FIntPoint(-9999, -9999);
	LastGenerateMs = (FPlatformTime::Seconds() - T0) * 1000.0;
	UE_LOG(LogLevelZeroMaze, Log, TEXT("%s"), *GetStatsString());
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

	// 1. Perfect maze: iterative recursive backtracker.
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
	for (UInstancedStaticMeshComponent* ISM : { FloorISM.Get(), CeilingISM.Get(), WallISM.Get(), DoorFrameISM.Get(), PillarISM.Get(), LightPanelISM.Get(), DeadPanelISM.Get(), CollisionISM.Get() })
	{
		ISM->ClearInstances();
	}
	Instances.Reset();
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
	const FSlot Slots[] = {
		{ FloorISM, FloorMaterial, FLinearColor(0.42f, 0.36f, 0.18f) },
		{ CeilingISM, CeilingMaterial, FLinearColor(0.78f, 0.75f, 0.62f) },
		{ WallISM, WallMaterial, FLinearColor(0.78f, 0.68f, 0.36f) },
		{ DoorFrameISM, DoorFrameMaterial, FLinearColor(0.62f, 0.53f, 0.28f) },
		{ PillarISM, WallMaterial, FLinearColor(0.78f, 0.68f, 0.36f) },
		{ LightPanelISM, LightPanelMaterial, FLinearColor(1.f, 0.97f, 0.8f) },
		{ DeadPanelISM, DeadPanelMaterial, FLinearColor(0.25f, 0.24f, 0.2f) },
		{ CollisionISM, nullptr, FLinearColor::White },
	};
	for (const FSlot& S : Slots)
	{
		S.ISM->SetStaticMesh(CubeMesh);
		if (S.ISM != CollisionISM)
		{
			S.ISM->SetMaterial(0, ResolveMaterial(S.Mat, S.Fallback));
		}
	}

	const float CS = CellSize, H = WallHeight, T = WallThickness;

	// Floor + ceiling tiles, ceiling light panels.
	NumLitPanels = NumDeadPanels = 0;
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 C = Idx(X, Y);
			const FVector Center((X + 0.5f) * CS, (Y + 0.5f) * CS, 0.f);
			AddBox(FloorISM, Center + FVector(0, 0, -5.f), FVector(CS, CS, 10.f), { C }, false);
			AddBox(CeilingISM, Center + FVector(0, 0, H + 5.f), FVector(CS, CS, 10.f), { C }, false);

			const bool bHall = CellTypes[C] == static_cast<uint8>(ELZCellType::PillarHall);
			const float Roll = Rng.FRand();
			const FVector PanelSize = (Rng.FRand() < 0.5f) ? FVector(120.f, 60.f, 2.f) : FVector(60.f, 120.f, 2.f);
			if (bHall || Roll < LitPanelChance)
			{
				AddBox(LightPanelISM, Center + FVector(0, 0, H - 1.f), PanelSize, { C }, false);
				++NumLitPanels;
			}
			else if (Roll < LitPanelChance + DeadPanelChance)
			{
				AddBox(DeadPanelISM, Center + FVector(0, 0, H - 1.f), PanelSize, { C }, false);
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
		if (!bOpen)
		{
			AddBox(WallISM, MakeCenter(SpanStart + CS * 0.5f, H * 0.5f), MakeSize(CS + T, H), { CellA, CellB }, true);
			++NumWallSegments;
		}
		else if (bDoor)
		{
			const float PostLen = Post + T * 0.5f;
			AddBox(DoorFrameISM, MakeCenter(SpanStart - T * 0.5f + PostLen * 0.5f, H * 0.5f), MakeSize(PostLen, H), { CellA, CellB }, true);
			AddBox(DoorFrameISM, MakeCenter(SpanStart + CS + T * 0.5f - PostLen * 0.5f, H * 0.5f), MakeSize(PostLen, H), { CellA, CellB }, true);
			AddBox(DoorFrameISM, MakeCenter(SpanStart + CS * 0.5f, (DoorHeight + H) * 0.5f), MakeSize(CS - 2.f * Post, H - DoorHeight), { CellA, CellB }, true);
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
		AddBox(PillarISM, FVector(P.X * CS, P.Y * CS, H * 0.5f), FVector(PillarSize, PillarSize, H),
			{ Idx(P.X - 1, P.Y - 1), Idx(P.X, P.Y - 1), Idx(P.X - 1, P.Y), Idx(P.X, P.Y) }, true);
	}

	// Floor collision slab.
	FloorCollision->SetRelativeLocation(FVector(Width * CS * 0.5f, Height * CS * 0.5f, -5.f));
	FloorCollision->SetBoxExtent(FVector(Width * CS * 0.5f, Height * CS * 0.5f, 5.f));

	// Exit: glowing slab + trigger (step 4 wires the win screen to OnExitReached).
	const FVector ExitCenter((ExitCell.X + 0.5f) * CS, (ExitCell.Y + 0.5f) * CS, 0.f);
	ExitMarker->SetStaticMesh(CubeMesh);
	ExitMarker->SetMaterial(0, ResolveMaterial(ExitMaterial, FLinearColor(0.2f, 1.f, 0.35f)));
	ExitMarker->SetRelativeLocation(ExitCenter + FVector(0, 0, 110.f));
	ExitMarker->SetRelativeScale3D(FVector(0.9f, 0.9f, 2.2f));
	ExitTrigger->SetRelativeLocation(ExitCenter + FVector(0, 0, 150.f));
	ExitTrigger->SetBoxExtent(FVector(CS * 0.4f, CS * 0.4f, 150.f));
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
		PillarCorners.Num(), Rooms, Halls, NumLitPanels, NumDeadPanels, Instances.Num(), LastGenerateMs);
}

void ALevelZeroMazeGenerator::BeginShotModeIfRequested()
{
	FString Path;
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
		ACameraActor* Cam = GetWorld()->SpawnActor<ACameraActor>(Loc, Rot);
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
