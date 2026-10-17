#include "LevelZeroMonster.h"

#include "LevelZeroMonsterAI.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(GetCapsuleComponent());

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
}
