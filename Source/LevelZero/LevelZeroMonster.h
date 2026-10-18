// Level Zero - monster. Priority: rigged Tripo skeletal mesh (in-place walk/run) > static Tripo mesh > primitive placeholder.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LevelZeroMonster.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class USkeletalMesh;
class USkeletalMeshComponent;
class UAnimSequenceBase;

UCLASS(Blueprintable)
class LEVELZERO_API ALevelZeroMonster : public ACharacter
{
	GENERATED_BODY()

public:
	ALevelZeroMonster();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	float WanderSpeed = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	float InvestigateSpeed = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	float ChaseSpeed = 330.f;

	/** Sight-trace origin above the actor origin (capsule centre). ~1.65 m above the floor so 2.3 m door lintels don't clip it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	float EyeHeight = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Visuals")
	TObjectPtr<UMaterialInterface> BodyMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Visuals")
	TObjectPtr<UMaterialInterface> EyeMaterial;

	// ---- Production mesh (Tripo). When set, it replaces the primitive placeholder; capsule/nav are unchanged.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Mesh")
	TObjectPtr<UStaticMesh> BodyMesh;

	/** Optional override for every slot of BodyMesh (e.g. MI_LZ_Monster_Tripo). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Mesh")
	TObjectPtr<UMaterialInterface> BodyMeshMaterial;

	/** BodyMesh is uniformly scaled so its bounds are this tall (cm); feet sit at the capsule bottom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Mesh")
	float TargetHeight = 260.f;

	/** Yaw applied to BodyMesh so its face points along the actor's +X. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Mesh")
	float MeshYawOffset = 0.f;

	/** Glowing eye placement for BodyMesh, in actor space: X forward, Y half spacing, Z height above the feet (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Mesh")
	FVector EyeOffset = FVector(18.f, 4.5f, 235.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Mesh")
	float EyeScale = 0.028f;

	// ---- Rigged mesh (preferred). In-place clips; play rate follows the AI's velocity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Rig")
	TObjectPtr<USkeletalMesh> SkeletalBodyMesh;

	/** Chase clip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Rig")
	TObjectPtr<UAnimSequenceBase> RunAnim;

	/** Wander/investigate clip. If empty, RunAnim is played slowed down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Rig")
	TObjectPtr<UAnimSequenceBase> WalkAnim;

	/** Ground speed the clips were authored at, in body heights per second (the removed root motion; scale-independent). 0 = fixed rates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Rig")
	float RunAnimSpeed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Rig")
	float WalkAnimSpeed = 0.f;

	/** Multipliers on the speed-matched rate. Walk < 1 = a slower, creepier gait (a little foot slide). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Rig")
	float WalkPlayRate = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Rig")
	float RunPlayRate = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Rig")
	float MinPlayRate = 0.3f;

	/** Rate while standing still: an almost-frozen, twitching pose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Rig")
	float IdlePlayRate = 0.06f;

	/** Bone the eyes ride on. None = first bone named "...Head". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Rig")
	FName EyeBone = NAME_None;

	// ---- Procedural motion (static mesh / placeholder only; the rig's clips replace it)
	/** Distance covered per footfall (cm): one vertical bob per step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Motion")
	float StepLength = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Motion")
	float BobAmplitude = 3.5f;

	/** Side-to-side roll per step (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Motion")
	float SwayDegrees = 3.5f;

	/** Forward lean at full chase speed (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Motion")
	float LeanDegrees = 9.f;

	/** How much it crouches (fraction of height) when passing under a door lintel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Motion")
	float DuckDepth = 0.14f;

	FVector GetEyesLocation() const { return GetActorLocation() + FVector(0, 0, EyeHeight); }
	bool IsUsingBodyMesh() const { return bUsingBodyMesh; }
	bool IsUsingSkeletalMesh() const { return bUsingSkeletal; }
	UFUNCTION(BlueprintPure, Category = "Monster|Rig") FString GetAnimDebug() const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Pivot at the feet: bob / sway / lean / duck are applied here so the body tilts from the ground. */
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<USceneComponent> MotionRoot;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<UStaticMeshComponent> BodyMeshComp;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<USkeletalMeshComponent> BodySkelComp;

	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<USceneComponent> VisualRoot;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<UStaticMeshComponent> Torso;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<UStaticMeshComponent> Head;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<UStaticMeshComponent> LegL;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<UStaticMeshComponent> LegR;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<UStaticMeshComponent> ArmL;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<UStaticMeshComponent> ArmR;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<UStaticMeshComponent> EyeL;
	UPROPERTY(VisibleAnywhere, Category = "Visuals") TObjectPtr<UStaticMeshComponent> EyeR;

	UPROPERTY() TObjectPtr<UMaterialInterface> FallbackMaterial;

private:
	bool bUsingBodyMesh = false;
	bool bUsingSkeletal = false;
	bool bRunning = false;
	float SkelScale = 1.f;
	float CurrentRate = 1.f;
	UPROPERTY() TObjectPtr<UAnimSequenceBase> CurrentAnim;
	void PlaceEyes(const FVector& FeetSpaceL, const FVector& FeetSpaceR);
	void UpdateAnimation(float Speed, float DeltaSeconds);
	float GaitPhase = 0.f;
	float DuckAlpha = 0.f;
	float LeanAlpha = 0.f;
	void ApplyBodyMesh();
};
