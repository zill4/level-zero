// Level Zero - seeded procedural "Level 0" maze built from instanced greybox modules.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelZeroMazeGenerator.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UBoxComponent;
class UStaticMesh;
class UMaterialInterface;
class APlayerStart;

UENUM(BlueprintType)
enum class ELZCellType : uint8
{
	Corridor,
	Room,
	PillarHall
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLZExitReachedSignature, AActor*, ReachedBy);

UCLASS(Blueprintable)
class LEVELZERO_API ALevelZeroMazeGenerator : public AActor
{
	GENERATED_BODY()

public:
	ALevelZeroMazeGenerator();

	// ---------------- Layout ----------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze")
	int32 Seed = 20261021;

	/** Pick a new random seed every time play starts (keep off for reproducible layouts). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze")
	bool bRandomSeedOnBeginPlay = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze", meta = (ClampMin = 4, ClampMax = 64))
	int32 Width = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze", meta = (ClampMin = 4, ClampMax = 64))
	int32 Height = 30;

	/** Cell size in cm (4 m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze", meta = (ClampMin = 200))
	float CellSize = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze")
	float WallHeight = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze")
	float WallThickness = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Rooms")
	int32 NumRooms = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Rooms")
	FIntPoint RoomSizeMin = FIntPoint(2, 2);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Rooms")
	FIntPoint RoomSizeMax = FIntPoint(5, 4);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Rooms")
	int32 NumPillarHalls = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Rooms")
	FIntPoint PillarHallSizeMin = FIntPoint(4, 4);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Rooms")
	FIntPoint PillarHallSizeMax = FIntPoint(7, 6);

	/** Chance to knock out a remaining corridor wall, creating loops (important for chases). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze", meta = (ClampMin = 0, ClampMax = 1))
	float LoopChance = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Doorways", meta = (ClampMin = 0, ClampMax = 1))
	float RoomDoorwayChance = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Doorways", meta = (ClampMin = 0, ClampMax = 1))
	float CorridorDoorwayChance = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Doorways")
	float DoorWidth = 170.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Doorways")
	float DoorHeight = 230.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze")
	float PillarSize = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Lights", meta = (ClampMin = 0, ClampMax = 1))
	float LitPanelChance = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Lights", meta = (ClampMin = 0, ClampMax = 1))
	float DeadPanelChance = 0.08f;

	// ---------------- Streaming ----------------
	/** Hide modules farther than StreamRadiusCells (Chebyshev) from the player camera at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Streaming")
	bool bStreamTiles = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Streaming", meta = (ClampMin = 1))
	int32 StreamRadiusCells = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Streaming")
	float StreamUpdateInterval = 0.1f;

	// ---------------- Player ----------------
	/** Moved to the start cell whenever the maze is generated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Player")
	TObjectPtr<APlayerStart> PlayerStartActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Player")
	bool bTeleportPlayerToStart = true;

	// ---------------- Visuals ----------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Visuals")
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Visuals")
	TObjectPtr<UMaterialInterface> WallMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Visuals")
	TObjectPtr<UMaterialInterface> FloorMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Visuals")
	TObjectPtr<UMaterialInterface> CeilingMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Visuals")
	TObjectPtr<UMaterialInterface> DoorFrameMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Visuals")
	TObjectPtr<UMaterialInterface> LightPanelMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Visuals")
	TObjectPtr<UMaterialInterface> DeadPanelMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Visuals")
	TObjectPtr<UMaterialInterface> ExitMaterial;

	// ---------------- Results ----------------
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Maze|Result")
	FIntPoint StartCell = FIntPoint::ZeroValue;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Maze|Result")
	FIntPoint ExitCell = FIntPoint::ZeroValue;

	/** BFS steps between start and exit. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Maze|Result")
	int32 StartToExitDistance = 0;

	UPROPERTY(BlueprintAssignable, Category = "Maze")
	FLZExitReachedSignature OnExitReached;

	// ---------------- API ----------------
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Maze")
	void Generate();

	UFUNCTION(BlueprintPure, Category = "Maze")
	FVector GetCellCenterWorld(FIntPoint Cell, float Z = 0.f) const;

	UFUNCTION(BlueprintPure, Category = "Maze")
	FIntPoint WorldToCell(FVector WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Maze")
	bool IsValidCell(FIntPoint Cell) const;

	/** True if From and To are adjacent and nothing blocks the boundary (doorways are passable). */
	UFUNCTION(BlueprintPure, Category = "Maze")
	bool IsPassable(FIntPoint From, FIntPoint To) const;

	UFUNCTION(BlueprintPure, Category = "Maze")
	ELZCellType GetCellType(FIntPoint Cell) const;

	/** Shortest cell path (BFS), inclusive of both ends. Empty if unreachable. For monster AI. */
	UFUNCTION(BlueprintCallable, Category = "Maze")
	TArray<FIntPoint> FindPath(FIntPoint From, FIntPoint To) const;

	UFUNCTION(BlueprintCallable, Category = "Maze")
	FString GetDebugAscii() const;

	UFUNCTION(BlueprintCallable, Category = "Maze")
	bool ExportDebugAscii(const FString& FilePath) const;

	UFUNCTION(BlueprintCallable, Category = "Maze")
	FString GetStatsString() const;

	UFUNCTION(BlueprintPure, Category = "Maze")
	int32 GetNumVisibleInstances() const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> FloorISM;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> CeilingISM;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> WallISM;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> DoorFrameISM;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> PillarISM;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> LightPanelISM;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> DeadPanelISM;

	/** Invisible, never streamed: walls/doorframes/pillars collision (also what nav will use). */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> CollisionISM;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> FloorCollision;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ExitMarker;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> ExitTrigger;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FallbackMaterial;

private:
	struct FInstanceRef
	{
		UInstancedStaticMeshComponent* Comp = nullptr;
		int32 Index = INDEX_NONE;
		FTransform Xf;
		int32 Cells[4] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };
		int32 NumCells = 0;
		bool bVisible = true;
	};

	// Layout data (row-major, Idx = Y * Width + X)
	TArray<uint8> CellTypes;
	TArray<bool> EastOpen;   // boundary (X,Y) <-> (X+1,Y)
	TArray<bool> SouthOpen;  // boundary (X,Y) <-> (X,Y+1)
	TArray<bool> EastDoor;
	TArray<bool> SouthDoor;
	TArray<FIntPoint> PillarCorners;
	TArray<FInstanceRef> Instances;
	FIntPoint LastStreamCell = FIntPoint(-9999, -9999);
	int32 NumLitPanels = 0;
	int32 NumDeadPanels = 0;
	int32 NumDoorways = 0;
	int32 NumWallSegments = 0;
	double LastGenerateMs = 0.0;

	// Debug capture: -LZShot=<png> [-LZTopDown]  |  -LZFlythrough=<dir> [-LZFlyCells=N] [-LZFlySpeed=cm/s]
	FTimerHandle ShotTimer;
	FString PendingShotPath;
	bool bShotTopDown = false;
	FString FlyDir;
	TArray<FVector> FlyPoints;
	float FlyDistance = 0.f;
	float FlySpeed = 160.f;
	int32 FlyFrame = 0;
	bool bFlying = false;
	FRotator FlyRot = FRotator::ZeroRotator;
	TObjectPtr<AActor> CaptureCamera;
	void TickFlythrough(float DeltaSeconds);
	FVector SampleFly(float Distance) const;
	int32 ShotPhase = 0;
	float ShotElapsed = 0.f;
	float ShotQuietTime = 0.f;
	float ShotExitAt = 0.f;

	int32 Idx(int32 X, int32 Y) const { return Y * Width + X; }
	int32 Idx(FIntPoint P) const { return P.Y * Width + P.X; }
	void SetOpen(FIntPoint A, FIntPoint B, bool bOpen);
	void CarveRect(int32 X0, int32 Y0, int32 W, int32 H, ELZCellType Type, bool bAddPillars);
	TArray<int32> BFS(FIntPoint From) const;
	void BuildLayout(FRandomStream& Rng);
	void BuildMeshes(FRandomStream& Rng);
	void ClearMeshes();
	void AddBox(UInstancedStaticMeshComponent* ISM, const FVector& Center, const FVector& SizeCm, std::initializer_list<int32> OwnerCells, bool bCollision);
	UMaterialInterface* ResolveMaterial(UMaterialInterface* Mat, const FLinearColor& FallbackColor);
	float GetFacingYaw(FIntPoint Cell) const;
	void UpdateStreaming(bool bForce);
	void ShowAllInstances();
	void PlacePlayerAtStart();
	void BeginShotModeIfRequested();
	void TickShotMode();

	UFUNCTION()
	void HandleExitOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
