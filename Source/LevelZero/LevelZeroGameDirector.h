// Level Zero - round flow: spawns the monster, turns player movement into noise, shows caught / escaped screens.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelZeroGameDirector.generated.h"

class ALevelZeroMazeGenerator;
class ALevelZeroMonster;
class UTextRenderComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class USkeletalMesh;
class UAnimSequenceBase;

UENUM(BlueprintType)
enum class ELZRoundState : uint8
{
	Starting,
	Playing,
	Caught,
	Escaped
};

UCLASS(Blueprintable)
class LEVELZERO_API ALevelZeroGameDirector : public AActor
{
	GENERATED_BODY()

public:
	ALevelZeroGameDirector();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director")
	TObjectPtr<ALevelZeroMazeGenerator> Maze;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director")
	TSubclassOf<ALevelZeroMonster> MonsterClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director|Monster")
	TObjectPtr<UMaterialInterface> MonsterBodyMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director|Monster")
	TObjectPtr<UMaterialInterface> MonsterEyeMaterial;

	/** Production monster mesh (Tripo, reduced). Empty = primitive placeholder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director|Monster")
	TObjectPtr<UStaticMesh> MonsterMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director|Monster")
	TObjectPtr<UMaterialInterface> MonsterMeshMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director|Monster")
	float MonsterMeshYawOffset = 0.f;

	/** Rigged monster (preferred over MonsterMesh). Made by Tools/import_tripo_rig.py. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	TObjectPtr<USkeletalMesh> MonsterSkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	TObjectPtr<UAnimSequenceBase> MonsterRunAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	TObjectPtr<UAnimSequenceBase> MonsterWalkAnim;

	/** Authored ground speed of the clips in body heights per second, from the import's root-motion analysis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	float MonsterRunAnimSpeed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	float MonsterWalkAnimSpeed = 0.f;

	/** Eye height above the feet (cm) for the production mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	float MonsterEyeHeight = 235.f;

	/** Monster spawns at least this many BFS cells away from the start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director|Monster")
	int32 MinMonsterDistanceCells = 14;

	// Noise: player speed thresholds (cm/s) and how far each noise carries (cm).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director|Noise")
	float WalkSpeedThreshold = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director|Noise")
	float SprintSpeedThreshold = 320.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director|Noise")
	float WalkNoiseRadius = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director|Noise")
	float SprintNoiseRadius = 2400.f;

	/** Teleport landings (a jump > TeleportJumpDistance within one sample) make a medium noise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director|Noise")
	float TeleportNoiseRadius = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director|Noise")
	float TeleportJumpDistance = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director")
	float RestartDelayCaught = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director")
	float RestartDelayEscaped = 8.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Director")
	ELZRoundState RoundState = ELZRoundState::Starting;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Director")
	TObjectPtr<ALevelZeroMonster> Monster;

	void NotifyPlayerCaught(ALevelZeroMonster* By);

	UFUNCTION(BlueprintCallable, Category = "Director")
	void NotifyPlayerEscaped();

	UFUNCTION(BlueprintPure, Category = "Director")
	float GetRunSeconds() const;

	UFUNCTION(BlueprintCallable, Category = "Director")
	void RestartRound();
	/** Escaped: open the maze's NextLevel if it has one, else restart this level. */
	void AdvanceOrRestart();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "UI") TObjectPtr<USceneComponent> PanelRoot;
	UPROPERTY(VisibleAnywhere, Category = "UI") TObjectPtr<UStaticMeshComponent> PanelBackdrop;
	UPROPERTY(VisibleAnywhere, Category = "UI") TObjectPtr<UTextRenderComponent> TitleText;
	UPROPERTY(VisibleAnywhere, Category = "UI") TObjectPtr<UTextRenderComponent> SubText;

private:
	double RunStartTime = 0.0;
	double RunEndTime = 0.0;
	double NoiseSampleTime = 0.0;
	FVector LastSampleLoc = FVector::ZeroVector;
	bool bHaveSample = false;
	bool bCaptureMode = false;
	bool bNoMonster = false; // -LZNoMonster: test the exit/win path in capture runs
	int32 NoiseEvents[3] = { 0, 0, 0 };
	FTimerHandle StartHandle;
	FTimerHandle RestartHandle;
	FTimerHandle HideMonsterHandle;

	void TryStartRound();
	void SpawnMonster();
	void RunNavSelfTest();
	void TickNoise();
	void ShowPanel(const FString& Title, const FString& Sub, const FLinearColor& Color);
	void UpdatePanelPose(float DeltaSeconds, bool bSnap);
	void FreezeMonsters();
	bool GetPlayerView(FVector& OutLoc, FRotator& OutRot) const;
};
