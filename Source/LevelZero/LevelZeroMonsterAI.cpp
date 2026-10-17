#include "LevelZeroMonsterAI.h"

#include "LevelZeroGameDirector.h"
#include "LevelZeroMazeGenerator.h"
#include "LevelZeroMonster.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogLevelZeroAI, Log, All);

ALevelZeroMonsterAI::ALevelZeroMonsterAI()
{
	PrimaryActorTick.bCanEverTick = true;
	bSetControlRotationFromPawnOrientation = true;
}

ALevelZeroMonster* ALevelZeroMonsterAI::GetMonster() const
{
	return Cast<ALevelZeroMonster>(GetPawn());
}

void ALevelZeroMonsterAI::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	TActorIterator<ALevelZeroMazeGenerator> It(GetWorld()); // first maze in the world
	if (It) { Maze = *It; }
	SetState(ELZMonsterState::Wander, TEXT("spawned"));
}

FString ALevelZeroMonsterAI::GetStateName() const
{
	switch (State)
	{
	case ELZMonsterState::Wander: return TEXT("Wander");
	case ELZMonsterState::Investigate: return TEXT("Investigate");
	case ELZMonsterState::Chase: return TEXT("Chase");
	case ELZMonsterState::Search: return TEXT("Search");
	default: return TEXT("Frozen");
	}
}

void ALevelZeroMonsterAI::SetState(ELZMonsterState NewState, const TCHAR* Reason)
{
	if (State == ELZMonsterState::Frozen && NewState != ELZMonsterState::Frozen) return;
	const ELZMonsterState Old = State;
	State = NewState;
	StateEnterTime = GetWorld()->GetTimeSeconds();
	NextRepathTime = 0.0;
	WaitUntil = 0.0;
	if (ALevelZeroMonster* M = GetMonster())
	{
		float Speed = M->WanderSpeed;
		if (NewState == ELZMonsterState::Investigate || NewState == ELZMonsterState::Search) Speed = M->InvestigateSpeed;
		if (NewState == ELZMonsterState::Chase) Speed = M->ChaseSpeed;
		M->GetCharacterMovement()->MaxWalkSpeed = Speed;
	}
	if (Old != NewState || NewState == ELZMonsterState::Wander)
	{
		UE_LOG(LogLevelZeroAI, Log, TEXT("Monster state %s -> %s (%s)"), *UEnum::GetValueAsString(Old), *UEnum::GetValueAsString(NewState), Reason);
	}
}

void ALevelZeroMonsterAI::Freeze()
{
	SetState(ELZMonsterState::Frozen, TEXT("round over"));
	StopMovement();
	GridPath.Reset();
}

void ALevelZeroMonsterAI::HoldPosition(float Seconds)
{
	StopMovement();
	GridPath.Reset();
	WaitUntil = GetWorld()->GetTimeSeconds() + Seconds;
}

bool ALevelZeroMonsterAI::GetPlayerEyes(FVector& OutEyes) const
{
	// The player camera is the HMD in VR and the view target in desktop/capture runs.
	if (APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		OutEyes = PCM->GetCameraLocation();
		return true;
	}
	return false;
}

bool ALevelZeroMonsterAI::CanSeePlayer(const FVector& PlayerEyes) const
{
	const ALevelZeroMonster* M = GetMonster();
	if (!M) return false;
	const FVector Eyes = M->GetEyesLocation();
	const FVector ToPlayer = PlayerEyes - Eyes;
	const float Dist = ToPlayer.Size();
	if (Dist > SightRange) return false;
	if (Dist > ProximityRange)
	{
		const float CosAngle = FVector::DotProduct(M->GetActorForwardVector(), ToPlayer.GetSafeNormal2D());
		if (CosAngle < FMath::Cos(FMath::DegreesToRadians(SightHalfAngle))) return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LZMonsterSight), false, M);
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0)) { Params.AddIgnoredActor(PlayerPawn); }
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eyes, PlayerEyes, ECC_Visibility, Params);
}

void ALevelZeroMonsterAI::HearNoise(const FVector& Location, float Radius, const TCHAR* Source)
{
	const ALevelZeroMonster* M = GetMonster();
	if (!M || State == ELZMonsterState::Frozen || State == ELZMonsterState::Chase) return;
	if (FVector::Dist2D(M->GetActorLocation(), Location) > Radius) return;
	TargetLocation = FVector(Location.X, Location.Y, M->GetActorLocation().Z);
	SetState(ELZMonsterState::Investigate, Source);
	MoveToward(TargetLocation, 60.f);
}

bool ALevelZeroMonsterAI::IsMoving() const
{
	if (GridPath.Num() > 0) return true;
	return GetMoveStatus() == EPathFollowingStatus::Moving || GetMoveStatus() == EPathFollowingStatus::Waiting;
}

void ALevelZeroMonsterAI::MoveToward(const FVector& Target, float Acceptance)
{
	GridPath.Reset();
	const EPathFollowingRequestResult::Type Result = MoveToLocation(Target, Acceptance, false, true, true, false);
	if (Result == EPathFollowingRequestResult::Failed && Maze.IsValid() && GetPawn())
	{
		// Nav mesh not ready or target off-mesh: follow the maze's own cell graph instead.
		const TArray<FIntPoint> Cells = Maze->FindPath(Maze->WorldToCell(GetPawn()->GetActorLocation()), Maze->WorldToCell(Target));
		for (int32 i = 1; i < Cells.Num(); ++i) { GridPath.Add(Maze->GetCellCenterWorld(Cells[i], GetPawn()->GetActorLocation().Z)); }
		GridIndex = 0;
		UE_LOG(LogLevelZeroAI, Verbose, TEXT("Nav move failed; grid path with %d cells"), GridPath.Num());
	}
}

void ALevelZeroMonsterAI::TickGridPath()
{
	APawn* P = GetPawn();
	if (!P || GridPath.Num() == 0) return;
	if (GridIndex >= GridPath.Num()) { GridPath.Reset(); return; }
	const FVector To = GridPath[GridIndex] - P->GetActorLocation();
	if (To.Size2D() < 60.f) { ++GridIndex; return; }
	P->AddMovementInput(To.GetSafeNormal2D(), 1.f);
}

void ALevelZeroMonsterAI::PickWanderTarget()
{
	APawn* P = GetPawn();
	if (!P || !Maze.IsValid()) return;
	const FIntPoint Here = Maze->WorldToCell(P->GetActorLocation());
	for (int32 Try = 0; Try < 8; ++Try)
	{
		const FIntPoint Cell(Here.X + FMath::RandRange(-5, 5), Here.Y + FMath::RandRange(-5, 5));
		if (Maze->IsValidCell(Cell) && Cell != Here)
		{
			TargetLocation = Maze->GetCellCenterWorld(Cell, P->GetActorLocation().Z);
			MoveToward(TargetLocation, 80.f);
			return;
		}
	}
}

void ALevelZeroMonsterAI::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ALevelZeroMonster* M = GetMonster();
	if (!M || State == ELZMonsterState::Frozen) return;
	TickGridPath();

	const double Now = GetWorld()->GetTimeSeconds();
	FVector PlayerEyes;
	const bool bHavePlayer = GetPlayerEyes(PlayerEyes);
	const bool bSees = bHavePlayer && CanSeePlayer(PlayerEyes);
	const FVector PlayerFloor = bHavePlayer ? FVector(PlayerEyes.X, PlayerEyes.Y, M->GetActorLocation().Z) : FVector::ZeroVector;

	if (Now >= NextDebugLog)
	{
		NextDebugLog = Now + 2.0;
		FString Why = TEXT("-");
		if (bHavePlayer)
		{
			const FVector Eyes = M->GetEyesLocation();
			const float Cos = FVector::DotProduct(M->GetActorForwardVector(), (PlayerEyes - Eyes).GetSafeNormal2D());
			FCollisionQueryParams Params(SCENE_QUERY_STAT(LZMonsterSightDbg), false, M);
			if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0)) { Params.AddIgnoredActor(PlayerPawn); }
			FHitResult Hit;
			const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Eyes, PlayerEyes, ECC_Visibility, Params);
			Why = FString::Printf(TEXT("cos=%.2f blocked=%d by=%s.%s eyes=%s player=%s"), Cos, bBlocked ? 1 : 0,
				*GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), *Eyes.ToCompactString(), *PlayerEyes.ToCompactString());
		}
		UE_LOG(LogLevelZeroAI, Verbose, TEXT("Monster tick: state=%s dist=%.0f sees=%d moving=%d grid=%d loc=%s %s"), *GetStateName(),
			bHavePlayer ? FVector::Dist2D(M->GetActorLocation(), PlayerFloor) : -1.f, bSees ? 1 : 0, IsMoving() ? 1 : 0, GridPath.Num(), *M->GetActorLocation().ToCompactString(), *Why);
	}

	if (bSees)
	{
		LastSeenTime = Now;
		LastKnownPlayer = PlayerFloor;
		if (State != ELZMonsterState::Chase) { SetState(ELZMonsterState::Chase, TEXT("saw player")); }
	}

	if (bHavePlayer && FVector::Dist2D(M->GetActorLocation(), PlayerFloor) < CatchRadius && FMath::Abs(PlayerEyes.Z - M->GetActorLocation().Z) < 300.f)
	{
		UE_LOG(LogLevelZeroAI, Log, TEXT("Monster caught the player"));
		for (TActorIterator<ALevelZeroGameDirector> It(GetWorld()); It; ++It) { It->NotifyPlayerCaught(M); }
		Freeze();
		return;
	}

	switch (State)
	{
	case ELZMonsterState::Chase:
		if (Now >= NextRepathTime)
		{
			MoveToward(bSees ? PlayerFloor : LastKnownPlayer, 30.f);
			NextRepathTime = Now + 0.25;
		}
		if (!bSees && Now - LastSeenTime > LoseSightSeconds)
		{
			TargetLocation = LastKnownPlayer;
			SetState(ELZMonsterState::Search, TEXT("lost sight"));
			MoveToward(TargetLocation, 60.f);
		}
		break;
	case ELZMonsterState::Investigate:
	case ELZMonsterState::Search:
		if (!IsMoving())
		{
			if (WaitUntil == 0.0) { WaitUntil = Now + 2.5; } // look around
			else if (Now >= WaitUntil) { SetState(ELZMonsterState::Wander, State == ELZMonsterState::Search ? TEXT("gave up") : TEXT("nothing here")); }
		}
		if (Now - StateEnterTime > 12.0) { SetState(ELZMonsterState::Wander, TEXT("timeout")); }
		break;
	case ELZMonsterState::Wander:
	default:
		if (!IsMoving())
		{
			if (WaitUntil == 0.0) { WaitUntil = Now + FMath::FRandRange(0.8, 2.0); }
			else if (Now >= WaitUntil) { WaitUntil = 0.0; PickWanderTarget(); }
		}
		break;
	}
}
