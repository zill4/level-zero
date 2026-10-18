#include "LevelZeroGameDirector.h"

#include "LevelZeroMazeGenerator.h"
#include "LevelZeroMonster.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequenceBase.h"
#include "LevelZeroMonsterAI.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogLevelZeroGame, Log, All);

ALevelZeroGameDirector::ALevelZeroGameDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	MonsterClass = ALevelZeroMonster::StaticClass();

	PanelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PanelRoot"));
	RootComponent = PanelRoot;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	PanelBackdrop = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PanelBackdrop"));
	PanelBackdrop->SetupAttachment(PanelRoot);
	PanelBackdrop->SetStaticMesh(Cube.Object);
	PanelBackdrop->SetRelativeLocation(FVector(-3.f, 0.f, 0.f)); // behind the text (actor +X faces the viewer)
	PanelBackdrop->SetRelativeScale3D(FVector(0.01f, 1.3f, 0.55f));
	PanelBackdrop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PanelBackdrop->SetCanEverAffectNavigation(false);
	PanelBackdrop->SetCastShadow(false);

	auto MakeText = [this](const TCHAR* Name, float Size, float Z)
	{
		UTextRenderComponent* T = CreateDefaultSubobject<UTextRenderComponent>(Name);
		T->SetupAttachment(PanelRoot);
		T->SetHorizontalAlignment(EHTA_Center);
		T->SetVerticalAlignment(EVRTA_TextCenter);
		T->SetWorldSize(Size);
		T->SetRelativeLocation(FVector(0.f, 0.f, Z));
		T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		T->SetCastShadow(false);
		return T;
	};
	TitleText = MakeText(TEXT("TitleText"), 14.f, 8.f);
	SubText = MakeText(TEXT("SubText"), 7.5f, -11.f);
	PanelRoot->SetVisibility(false, true);
}

void ALevelZeroGameDirector::BeginPlay()
{
	Super::BeginPlay();
	bCaptureMode = FCString::Strifind(FCommandLine::Get(), TEXT("LZFlythrough=")) != nullptr || FCString::Strifind(FCommandLine::Get(), TEXT("LZPropShots=")) != nullptr;
	bNoMonster = FParse::Param(FCommandLine::Get(), TEXT("LZNoMonster"));
	if (!Maze)
	{
		TActorIterator<ALevelZeroMazeGenerator> It(GetWorld()); // first maze in the world
		if (It) { Maze = *It; }
	}
	// The generator builds the maze + nav in its own BeginPlay; start one tick later so ordering never matters.
	GetWorldTimerManager().SetTimer(StartHandle, this, &ALevelZeroGameDirector::TryStartRound, 0.1f, true, 0.05f);
}

void ALevelZeroGameDirector::TryStartRound()
{
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Maze || (Nav && Nav->IsNavigationBuildInProgress()) || !UGameplayStatics::GetPlayerCameraManager(this, 0)) return;
	if (bCaptureMode && !Maze->IsFlying()) return; // capture: start the round with the first recorded frame
	GetWorldTimerManager().ClearTimer(StartHandle);
	RunNavSelfTest();
	if (!bNoMonster) { SpawnMonster(); }
	RunStartTime = GetWorld()->GetTimeSeconds();
	RoundState = ELZRoundState::Playing;
	UE_LOG(LogLevelZeroGame, Log, TEXT("Round started (capture=%d)"), bCaptureMode ? 1 : 0);
}

void ALevelZeroGameDirector::RunNavSelfTest()
{
	// The VR template's teleport projects the aim point onto the nav mesh; check that works across the maze.
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav || !Maze) { UE_LOG(LogLevelZeroGame, Warning, TEXT("NavSelfTest: no nav system")); return; }
	int32 Ok = 0, Total = 0;
	for (int32 Y = 0; Y < Maze->Height; Y += 3)
	{
		for (int32 X = 0; X < Maze->Width; X += 3)
		{
			FNavLocation Out;
			++Total;
			Ok += Nav->ProjectPointToNavigation(Maze->GetCellCenterWorld(FIntPoint(X, Y), 50.f), Out, FVector(150.f, 150.f, 200.f)) ? 1 : 0;
		}
	}
	const FVector Start = Maze->GetCellCenterWorld(Maze->StartCell, 50.f);
	const FVector Exit = Maze->GetCellCenterWorld(Maze->ExitCell, 50.f);
	UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(this, Start, Exit);
	const bool bPath = Path && Path->IsValid() && !Path->IsPartial();
	UE_LOG(LogLevelZeroGame, Log, TEXT("NavSelfTest: teleport projection %d/%d cells, start->exit path %s (%d points, %.0f cm), build %.2fs"),
		Ok, Total, bPath ? TEXT("OK") : TEXT("MISSING"), Path ? Path->PathPoints.Num() : 0, Path && Path->IsValid() ? Path->GetPathLength() : 0.f, Maze->LastNavBuildSeconds);
}

void ALevelZeroGameDirector::SpawnMonster()
{
	if (!Maze || !MonsterClass) return;
	FIntPoint Cell = Maze->ExitCell;
	FRotator Facing = FRotator::ZeroRotator;
	const TArray<FIntPoint> Route = Maze->FindPath(Maze->StartCell, Maze->ExitCell);
	if (bCaptureMode && Route.Num() > 4)
	{
		// Capture: stand on the camera's route, facing it, so the clip shows a sighting and a chase.
		int32 Index = FMath::Min(10, Route.Num() - 2);
		FParse::Value(FCommandLine::Get(), TEXT("LZMonsterCell="), Index);
		Index = FMath::Clamp(Index, 2, Route.Num() - 1);
		Cell = Route[Index];
		Facing = (Maze->GetCellCenterWorld(Route[Index - 1]) - Maze->GetCellCenterWorld(Cell)).Rotation();
	}
	else
	{
		// Random cell at least MinMonsterDistanceCells from the start (never on top of the exit).
		TArray<FIntPoint> Candidates;
		for (int32 Y = 0; Y < Maze->Height; ++Y)
		{
			for (int32 X = 0; X < Maze->Width; ++X)
			{
				const FIntPoint P(X, Y);
				const int32 D = Maze->FindPath(Maze->StartCell, P).Num() - 1;
				if (D >= MinMonsterDistanceCells && P != Maze->ExitCell) { Candidates.Add(P); }
			}
		}
		if (Candidates.Num() > 0) { Cell = Candidates[FMath::RandRange(0, Candidates.Num() - 1)]; }
		Facing = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f);
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.bDeferConstruction = true;
	ALevelZeroMonster* M = GetWorld()->SpawnActor<ALevelZeroMonster>(MonsterClass, Maze->GetCellCenterWorld(Cell, 128.f), FRotator(0.f, Facing.Yaw, 0.f), Params);
	if (M)
	{
		M->BodyMaterial = MonsterBodyMaterial;
		M->EyeMaterial = MonsterEyeMaterial;
		M->BodyMesh = MonsterMesh;
		M->BodyMeshMaterial = MonsterMeshMaterial;
		M->MeshYawOffset = MonsterMeshYawOffset;
		M->SkeletalBodyMesh = MonsterSkeletalMesh;
		M->RunAnim = MonsterRunAnim;
		M->WalkAnim = MonsterWalkAnim;
		M->RunAnimSpeed = MonsterRunAnimSpeed;
		M->WalkAnimSpeed = MonsterWalkAnimSpeed;
		M->EyeOffset.Z = MonsterEyeHeight;
		UGameplayStatics::FinishSpawningActor(M, FTransform(FRotator(0.f, Facing.Yaw, 0.f), Maze->GetCellCenterWorld(Cell, 128.f)));
		Monster = M;
		if (bCaptureMode)
		{
			if (ALevelZeroMonsterAI* AI = Cast<ALevelZeroMonsterAI>(M->GetController())) { AI->HoldPosition(60.f); } // lurk on the route until it spots the camera
		}
		UE_LOG(LogLevelZeroGame, Log, TEXT("Monster spawned at cell (%d,%d), %d cells from start, mesh=%s"), Cell.X, Cell.Y, Maze->FindPath(Maze->StartCell, Cell).Num() - 1,
			M->IsUsingSkeletalMesh() ? *GetNameSafe(MonsterSkeletalMesh) : M->IsUsingBodyMesh() ? *GetNameSafe(MonsterMesh) : TEXT("placeholder"));
	}
}

bool ALevelZeroGameDirector::GetPlayerView(FVector& OutLoc, FRotator& OutRot) const
{
	if (APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		OutLoc = PCM->GetCameraLocation();
		OutRot = PCM->GetCameraRotation();
		return true;
	}
	return false;
}

float ALevelZeroGameDirector::GetRunSeconds() const
{
	if (RoundState == ELZRoundState::Starting) return 0.f;
	const double End = (RoundState == ELZRoundState::Playing) ? GetWorld()->GetTimeSeconds() : RunEndTime;
	return float(End - RunStartTime);
}

void ALevelZeroGameDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (RoundState == ELZRoundState::Playing)
	{
		TickNoise();
		FVector Loc; FRotator Rot;
		if (Maze && GetPlayerView(Loc, Rot) && (!bCaptureMode || bNoMonster))
		{
			const FVector ExitCenter = Maze->GetCellCenterWorld(Maze->ExitCell, Loc.Z);
			if (FVector::Dist2D(Loc, ExitCenter) < Maze->CellSize * 0.45f) { NotifyPlayerEscaped(); }
		}
	}
	else if (RoundState == ELZRoundState::Caught || RoundState == ELZRoundState::Escaped)
	{
		UpdatePanelPose(DeltaSeconds, false);
	}
}

void ALevelZeroGameDirector::TickNoise()
{
	FVector Loc; FRotator Rot;
	if (!GetPlayerView(Loc, Rot)) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (!bHaveSample) { LastSampleLoc = Loc; NoiseSampleTime = Now; bHaveSample = true; return; }
	const double Dt = Now - NoiseSampleTime;
	if (Dt < 0.2) return;
	const float Moved = FVector::Dist2D(Loc, LastSampleLoc);
	const float Speed = Moved / float(Dt);
	LastSampleLoc = Loc;
	NoiseSampleTime = Now;

	float Radius = 0.f;
	const TCHAR* Kind = nullptr;
	int32 KindIdx = -1;
	if (Moved > TeleportJumpDistance) { Radius = TeleportNoiseRadius; Kind = TEXT("heard teleport landing"); KindIdx = 2; }
	else if (Speed > SprintSpeedThreshold) { Radius = SprintNoiseRadius; Kind = TEXT("heard sprinting"); KindIdx = 1; }
	else if (Speed > WalkSpeedThreshold) { Radius = WalkNoiseRadius; Kind = TEXT("heard footsteps"); KindIdx = 0; }
	if (!Kind) return;
	++NoiseEvents[KindIdx];
	for (TActorIterator<ALevelZeroMonster> It(GetWorld()); It; ++It)
	{
		if (ALevelZeroMonsterAI* AI = Cast<ALevelZeroMonsterAI>(It->GetController())) { AI->HearNoise(Loc, Radius, Kind); }
	}
}

void ALevelZeroGameDirector::FreezeMonsters()
{
	for (TActorIterator<ALevelZeroMonster> It(GetWorld()); It; ++It)
	{
		if (ALevelZeroMonsterAI* AI = Cast<ALevelZeroMonsterAI>(It->GetController())) { AI->Freeze(); }
	}
}

void ALevelZeroGameDirector::NotifyPlayerCaught(ALevelZeroMonster* By)
{
	if (RoundState != ELZRoundState::Playing) return;
	RoundState = ELZRoundState::Caught;
	RunEndTime = GetWorld()->GetTimeSeconds();
	FreezeMonsters();
	if (Maze && bCaptureMode) { Maze->PauseFlythrough(); }
	// Short jumpscare beat, then remove the monster so it doesn't block the caught card.
	GetWorldTimerManager().SetTimer(HideMonsterHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		for (TActorIterator<ALevelZeroMonster> It(GetWorld()); It; ++It) { It->SetActorHiddenInGame(true); }
	}), 0.75f, false);
	const float T = GetRunSeconds();
	UE_LOG(LogLevelZeroGame, Log, TEXT("CAUGHT after %.1fs (noise events walk=%d sprint=%d teleport=%d)"), T, NoiseEvents[0], NoiseEvents[1], NoiseEvents[2]);
	ShowPanel(TEXT("IT FOUND YOU"), FString::Printf(TEXT("survived %d:%04.1f  -  restarting..."), int32(T) / 60, FMath::Fmod(T, 60.f)), FLinearColor(0.85f, 0.08f, 0.05f));
	if (!bCaptureMode) { GetWorldTimerManager().SetTimer(RestartHandle, this, &ALevelZeroGameDirector::RestartRound, RestartDelayCaught, false); }
}

void ALevelZeroGameDirector::NotifyPlayerEscaped()
{
	if (RoundState != ELZRoundState::Playing) return;
	RoundState = ELZRoundState::Escaped;
	RunEndTime = GetWorld()->GetTimeSeconds();
	FreezeMonsters();
	const float T = GetRunSeconds();
	UE_LOG(LogLevelZeroGame, Log, TEXT("ESCAPED in %.1fs"), T);
	if (Maze) { Maze->SetExitMarkerVisible(false); } // the player is standing in the light; don't block the card
	if (Maze && bCaptureMode) { Maze->PauseFlythrough(); }
	ShowPanel(TEXT("ESCAPED"), FString::Printf(TEXT("time %d:%04.1f"), int32(T) / 60, FMath::Fmod(T, 60.f)), FLinearColor(0.3f, 1.f, 0.4f));
	if (!bCaptureMode) { GetWorldTimerManager().SetTimer(RestartHandle, this, &ALevelZeroGameDirector::RestartRound, RestartDelayEscaped, false); }
}

void ALevelZeroGameDirector::RestartRound()
{
	UE_LOG(LogLevelZeroGame, Log, TEXT("Restarting level"));
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

void ALevelZeroGameDirector::ShowPanel(const FString& Title, const FString& Sub, const FLinearColor& Color)
{
	TitleText->SetText(FText::FromString(Title));
	TitleText->SetTextRenderColor(Color.ToFColor(true));
	SubText->SetText(FText::FromString(Sub));
	SubText->SetTextRenderColor(FColor(235, 225, 190));
	// The level has no lights (self-lit materials), so the engine's lit text material renders black:
	// use black text on a self-lit tinted card (red = caught, green = escaped).
	if (MonsterBodyMaterial)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(MonsterBodyMaterial, this);
		MID->SetVectorParameterValue(TEXT("Color"), Color * 0.75f);
		MID->SetScalarParameterValue(TEXT("EmissiveStrength"), 1.f);
		PanelBackdrop->SetMaterial(0, MID);
	}
	PanelRoot->SetVisibility(true, true);
	UpdatePanelPose(0.f, true);
}

void ALevelZeroGameDirector::UpdatePanelPose(float DeltaSeconds, bool bSnap)
{
	// Comfortable VR UI: a panel 1.2 m ahead that lazily follows the head (no hard head-lock).
	FVector Loc; FRotator Rot;
	if (!GetPlayerView(Loc, Rot)) return;
	const FRotator Yaw(0.f, Rot.Yaw, 0.f);
	const FVector TargetLoc = Loc + Yaw.Vector() * 120.f + FVector(0, 0, -5.f);
	const FRotator TargetRot(0.f, Rot.Yaw + 180.f, 0.f); // text renders toward +X; face the viewer
	if (bSnap) { SetActorLocationAndRotation(TargetLoc, TargetRot); return; }
	SetActorLocationAndRotation(FMath::VInterpTo(GetActorLocation(), TargetLoc, DeltaSeconds, 3.f), FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaSeconds, 3.f));
}
