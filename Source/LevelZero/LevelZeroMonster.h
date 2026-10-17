// Level Zero - placeholder monster: a tall, thin primitive silhouette (Tripo model replaces it later).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LevelZeroMonster.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

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

	FVector GetEyesLocation() const { return GetActorLocation() + FVector(0, 0, EyeHeight); }

protected:
	virtual void BeginPlay() override;

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
};
