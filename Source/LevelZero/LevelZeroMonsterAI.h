// Level Zero - monster brain: Wander -> Investigate (noise) -> Chase (sight) -> Search -> Wander.
#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "LevelZeroMonsterAI.generated.h"

class ALevelZeroMonster;
class ALevelZeroMazeGenerator;

UENUM(BlueprintType)
enum class ELZMonsterState : uint8
{
	Wander,
	Investigate,
	Chase,
	Search,
	Frozen
};

UCLASS()
class LEVELZERO_API ALevelZeroMonsterAI : public AAIController
{
	GENERATED_BODY()

public:
	ALevelZeroMonsterAI();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Senses")
	float SightRange = 2000.f;

	/** Half-angle of the vision cone in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Senses")
	float SightHalfAngle = 70.f;

	/** Always notices the player this close, regardless of facing (but still needs line of sight). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Senses")
	float ProximityRange = 350.f;

	/** Seconds without sight before the chase turns into a search. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Senses")
	float LoseSightSeconds = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Senses")
	float CatchRadius = 95.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	ELZMonsterState State = ELZMonsterState::Wander;

	/** Called by the player noise emitter. Radius = how far this noise carries (cm). */
	void HearNoise(const FVector& Location, float Radius, const TCHAR* Source);

	void Freeze();

	/** Stand still (still watching and listening) for Seconds before the first wander move. */
	void HoldPosition(float Seconds);

	UFUNCTION(BlueprintPure, Category = "State")
	FString GetStateName() const;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	TWeakObjectPtr<ALevelZeroMazeGenerator> Maze;
	FVector TargetLocation = FVector::ZeroVector;
	FVector LastKnownPlayer = FVector::ZeroVector;
	double LastSeenTime = -1000.0;
	double StateEnterTime = 0.0;
	double NextRepathTime = 0.0;
	double WaitUntil = 0.0;
	double NextDebugLog = 0.0;
	TArray<FVector> GridPath; // fallback route (cell centres) when the nav mesh can't serve a path
	int32 GridIndex = 0;

	void SetState(ELZMonsterState NewState, const TCHAR* Reason);
	bool GetPlayerEyes(FVector& OutEyes) const;
	bool CanSeePlayer(const FVector& PlayerEyes) const;
	void MoveToward(const FVector& Target, float Acceptance);
	bool IsMoving() const;
	void TickGridPath();
	void PickWanderTarget();
	ALevelZeroMonster* GetMonster() const;
};
