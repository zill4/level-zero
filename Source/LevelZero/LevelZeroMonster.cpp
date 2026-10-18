#include "LevelZeroMonster.h"

#include "LevelZeroMonsterAI.h"
#include "Components/CapsuleComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	UStaticMeshComponent* MakePart(ACharacter* Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale, const FRotator& Rot = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* C = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Parent);
		C->SetStaticMesh(Mesh);
		C->SetRelativeLocation(Loc);
		C->SetRelativeRotation(Rot);
		C->SetRelativeScale3D(Scale);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(false);
		return C;
	}
}

ALevelZeroMonster::ALevelZeroMonster()
{
	PrimaryActorTick.bCanEverTick = true;
	// 2.5 m tall capsule; radius matches the default nav agent so the runtime nav mesh fits it.
	GetCapsuleComponent()->InitCapsuleSize(34.f, 125.f);
	GetCapsuleComponent()->SetCanEverAffectNavigation(false);

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = WanderSpeed;
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 240.f, 0.f);
	Move->bUseRVOAvoidance = false;
	bUseControllerRotationYaw = false;

	AIControllerClass = ALevelZeroMonsterAI::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sph(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	FallbackMaterial = Mat.Object;

	MotionRoot = CreateDefaultSubobject<USceneComponent>(TEXT("MotionRoot"));
	MotionRoot->SetupAttachment(GetCapsuleComponent());
	MotionRoot->SetRelativeLocation(FVector(0, 0, -125.f)); // feet

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(MotionRoot);
	VisualRoot->SetRelativeLocation(FVector(0, 0, 125.f)); // back to capsule centre: placeholder parts are authored from there

	BodyMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMeshComp->SetupAttachment(MotionRoot);
	BodyMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMeshComp->SetCanEverAffectNavigation(false);
	BodyMeshComp->SetCastShadow(false);
	BodyMeshComp->SetVisibility(false);

	BodySkelComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BodySkel"));
	BodySkelComp->SetupAttachment(MotionRoot);
	BodySkelComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodySkelComp->SetCanEverAffectNavigation(false);
	BodySkelComp->SetCastShadow(false);
	BodySkelComp->SetVisibility(false);
	BodySkelComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	// Engine cylinder/sphere are 100 cm tall, pivot centred. Origin = capsule centre, feet at -125.
	LegL = MakePart(this, VisualRoot, TEXT("LegL"), Cyl.Object, FVector(0, -11, -75), FVector(0.09f, 0.09f, 1.0f));
	LegR = MakePart(this, VisualRoot, TEXT("LegR"), Cyl.Object, FVector(0, 11, -75), FVector(0.09f, 0.09f, 1.0f));
	Torso = MakePart(this, VisualRoot, TEXT("Torso"), Cyl.Object, FVector(0, 0, 40), FVector(0.24f, 0.17f, 1.3f));
	Head = MakePart(this, VisualRoot, TEXT("Head"), Sph.Object, FVector(4, 0, 124), FVector(0.26f, 0.24f, 0.34f));
	ArmL = MakePart(this, VisualRoot, TEXT("ArmL"), Cyl.Object, FVector(2, -22, 15), FVector(0.06f, 0.06f, 1.7f), FRotator(0, 0, 6));
	ArmR = MakePart(this, VisualRoot, TEXT("ArmR"), Cyl.Object, FVector(2, 22, 15), FVector(0.06f, 0.06f, 1.7f), FRotator(0, 0, -6));
	EyeL = MakePart(this, VisualRoot, TEXT("EyeL"), Sph.Object, FVector(14, -5, 128), FVector(0.035f));
	EyeR = MakePart(this, VisualRoot, TEXT("EyeR"), Sph.Object, FVector(14, 5, 128), FVector(0.035f));
}

void ALevelZeroMonster::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->MaxWalkSpeed = WanderSpeed;

	auto Resolve = [this](UMaterialInterface* M, const FLinearColor& C) -> UMaterialInterface*
	{
		if (M) return M;
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(FallbackMaterial, this);
		MID->SetVectorParameterValue(TEXT("Color"), C);
		return MID;
	};
	UMaterialInterface* Body = Resolve(BodyMaterial, FLinearColor(0.02f, 0.02f, 0.02f));
	UMaterialInterface* Eyes = Resolve(EyeMaterial, FLinearColor(0.9f, 0.95f, 1.f));
	for (UStaticMeshComponent* C : { Torso.Get(), Head.Get(), LegL.Get(), LegR.Get(), ArmL.Get(), ArmR.Get() }) { C->SetMaterial(0, Body); }
	EyeL->SetMaterial(0, Eyes);
	EyeR->SetMaterial(0, Eyes);
	ApplyBodyMesh();
}

void ALevelZeroMonster::PlaceEyes(const FVector& L, const FVector& R)
{
	// Feet-space (X forward, Z up from the floor) -> VisualRoot space (capsule centre).
	EyeL->SetRelativeLocation(L - FVector(0, 0, 125.f));
	EyeR->SetRelativeLocation(R - FVector(0, 0, 125.f));
	EyeL->SetRelativeScale3D(FVector(EyeScale));
	EyeR->SetRelativeScale3D(FVector(EyeScale));
}

FString ALevelZeroMonster::GetAnimDebug() const
{
	return FString::Printf(TEXT("anim=%s rate=%.2f running=%d scale=%.3f"), *GetNameSafe(CurrentAnim), CurrentRate, bRunning ? 1 : 0, SkelScale);
}

void ALevelZeroMonster::ApplyBodyMesh()
{
	bUsingSkeletal = SkeletalBodyMesh != nullptr;
	bUsingBodyMesh = bUsingSkeletal || BodyMesh != nullptr;
	BodySkelComp->SetVisibility(bUsingSkeletal);
	BodyMeshComp->SetVisibility(bUsingBodyMesh && !bUsingSkeletal);
	for (UStaticMeshComponent* C : { Torso.Get(), Head.Get(), LegL.Get(), LegR.Get(), ArmL.Get(), ArmR.Get() }) { C->SetVisibility(!bUsingBodyMesh); }
	if (!bUsingBodyMesh) return;
	const FVector EyeLFeet(EyeOffset.X, -EyeOffset.Y, EyeOffset.Z), EyeRFeet(EyeOffset.X, EyeOffset.Y, EyeOffset.Z);

	if (bUsingSkeletal)
	{
		BodySkelComp->SetSkeletalMeshAsset(SkeletalBodyMesh);
		const FBox B = SkeletalBodyMesh->GetImportedBounds().GetBox(); // reference pose
		SkelScale = TargetHeight / FMath::Max(B.GetSize().Z, 1.f);
		const FRotator Yaw(0.f, MeshYawOffset, 0.f);
		BodySkelComp->SetRelativeScale3D(FVector(SkelScale));
		BodySkelComp->SetRelativeRotation(Yaw);
		BodySkelComp->SetRelativeLocation(-Yaw.RotateVector(FVector(B.GetCenter().X, B.GetCenter().Y, B.Min.Z)) * SkelScale);
		if (BodyMeshMaterial)
		{
			for (int32 i = 0; i < BodySkelComp->GetNumMaterials(); ++i) { BodySkelComp->SetMaterial(i, BodyMeshMaterial); }
		}
		// Eyes: place them on the reference pose, then ride the head bone so they follow the animation.
		PlaceEyes(EyeLFeet, EyeRFeet);
		FName Bone = EyeBone;
		if (Bone.IsNone())
		{
			for (int32 i = 0; i < BodySkelComp->GetNumBones(); ++i)
			{
				const FString N = BodySkelComp->GetBoneName(i).ToString();
				if (N.EndsWith(TEXT("Head"), ESearchCase::IgnoreCase)) { Bone = BodySkelComp->GetBoneName(i); break; }
			}
		}
		BodySkelComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		BodySkelComp->RefreshBoneTransforms();
		if (!Bone.IsNone())
		{
			EyeL->AttachToComponent(BodySkelComp, FAttachmentTransformRules::KeepWorldTransform, Bone);
			EyeR->AttachToComponent(BodySkelComp, FAttachmentTransformRules::KeepWorldTransform, Bone);
		}
		UE_LOG(LogTemp, Display, TEXT("LZ: monster rig %s scale %.3f eye bone %s run=%s walk=%s"), *GetNameSafe(SkeletalBodyMesh), SkelScale,
			*Bone.ToString(), *GetNameSafe(RunAnim), *GetNameSafe(WalkAnim));
		UpdateAnimation(0.f, 0.f);
		return;
	}

	BodyMeshComp->SetStaticMesh(BodyMesh);
	const FBox B = BodyMesh->GetBoundingBox();
	const float S = TargetHeight / FMath::Max(B.GetSize().Z, 1.f);
	const FRotator Yaw(0.f, MeshYawOffset, 0.f);
	BodyMeshComp->SetRelativeScale3D(FVector(S));
	BodyMeshComp->SetRelativeRotation(Yaw);
	// Bottom-centre of the bounds goes to the feet pivot.
	BodyMeshComp->SetRelativeLocation(-Yaw.RotateVector(FVector(B.GetCenter().X, B.GetCenter().Y, B.Min.Z)) * S);
	if (BodyMeshMaterial)
	{
		for (int32 i = 0; i < BodyMeshComp->GetNumMaterials(); ++i) { BodyMeshComp->SetMaterial(i, BodyMeshMaterial); }
	}
	PlaceEyes(EyeLFeet, EyeRFeet);
}

void ALevelZeroMonster::UpdateAnimation(float Speed, float DeltaSeconds)
{
	if (!RunAnim && !WalkAnim) return;
	// Hysteresis between investigate and chase speed.
	const float Mid = 0.5f * (InvestigateSpeed + ChaseSpeed);
	bRunning = bRunning ? Speed > Mid * 0.9f : Speed > Mid * 1.1f;
	UAnimSequenceBase* Want = (bRunning || !WalkAnim) ? (RunAnim ? RunAnim.Get() : WalkAnim.Get()) : WalkAnim.Get();
	const bool bWalkClip = !bRunning;
	const float Authored = (Want == WalkAnim && WalkAnimSpeed > 0.f) ? WalkAnimSpeed : RunAnimSpeed;
	float Target = 1.f;
	if (Speed < 10.f) { Target = IdlePlayRate; }
	else if (Authored > 0.f) { Target = FMath::Clamp(Speed / (Authored * TargetHeight) * (bWalkClip ? WalkPlayRate : RunPlayRate), MinPlayRate, 2.f); }
	else { Target = bWalkClip ? WalkPlayRate : RunPlayRate; }
	CurrentRate = DeltaSeconds > 0.f ? FMath::FInterpTo(CurrentRate, Target, DeltaSeconds, 5.f) : Target;
	if (Want != CurrentAnim)
	{
		// Keep the gait phase when switching clips so the feet don't pop.
		float Phase = 0.f;
		if (CurrentAnim && CurrentAnim->GetPlayLength() > 0.f) { Phase = BodySkelComp->GetPosition() / CurrentAnim->GetPlayLength(); }
		BodySkelComp->PlayAnimation(Want, true);
		BodySkelComp->SetPosition(Phase * Want->GetPlayLength(), false);
		CurrentAnim = Want;
	}
	BodySkelComp->SetPlayRate(CurrentRate);
}

void ALevelZeroMonster::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (IsHidden()) return;
	const float Speed = GetVelocity().Size2D();
	if (bUsingSkeletal) { UpdateAnimation(Speed, DeltaSeconds); }
	const float Walk = FMath::Clamp(Speed / 60.f, 0.f, 1.f);
	GaitPhase = FMath::Fmod(GaitPhase + DeltaSeconds * Speed / FMath::Max(StepLength, 1.f) * PI, 2.f * PI);
	const float Step = FMath::Sin(GaitPhase);
	const float Breath = FMath::Sin(GetWorld()->GetTimeSeconds() * 1.4f);
	LeanAlpha = FMath::FInterpTo(LeanAlpha, FMath::Clamp(Speed / FMath::Max(ChaseSpeed, 1.f), 0.f, 1.f), DeltaSeconds, 4.f);

	// Duck under door lintels (they block Visibility traces, not pawns).
	const FVector Feet = GetActorLocation() - FVector(0, 0, 125.f);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LZMonsterDuck), false, this);
	const bool bLow = GetWorld()->LineTraceSingleByChannel(Hit, Feet + FVector(0, 0, 150.f) + GetActorForwardVector() * 30.f,
		Feet + FVector(0, 0, TargetHeight + 10.f) + GetActorForwardVector() * 30.f, ECC_Visibility, Params);
	DuckAlpha = FMath::FInterpTo(DuckAlpha, bLow ? 1.f : 0.f, DeltaSeconds, 6.f);

	// The rig's clips carry bob/sway/lean; only the lintel duck stays procedural for it.
	const float Proc = bUsingSkeletal ? 0.f : 1.f;
	const float Z = (FMath::Abs(Step) * BobAmplitude * Walk + Breath * 0.8f * (1.f - Walk)) * Proc;
	const float Roll = Step * SwayDegrees * Walk * Proc;
	const float Pitch = -LeanDegrees * LeanAlpha * Proc - 12.f * DuckAlpha; // negative pitch = lean forward
	MotionRoot->SetRelativeLocationAndRotation(FVector(0, 0, -125.f + Z), FRotator(Pitch, 0.f, Roll));
	MotionRoot->SetRelativeScale3D(FVector(1.f, 1.f, 1.f - DuckDepth * DuckAlpha));
}
