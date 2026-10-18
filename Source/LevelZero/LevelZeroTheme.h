// Level Zero - data-driven level theme. One asset per level (Content/LevelZero/Themes/DA_LZTheme_<Id>), built from
// Tools/levels/<id>.json by Tools/create_level.py. ALevelZeroMazeGenerator::ApplyTheme copies these onto the generator
// at the start of Generate(), so the generator keeps one code path and a map without a theme behaves exactly as before.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LevelZeroTheme.generated.h"

class UStaticMesh;
class UWorld;

/** How the base grid is carved before rooms/halls are merged in. */
UENUM(BlueprintType)
enum class ELZLayoutStyle : uint8
{
	Maze,   // recursive backtracker (optionally N cells wide)
	Aisles  // parallel E-W aisles between rack walls, N-S cross-aisles every CrossAisleEvery cells
};

/** Which floor cells use the alternate floor colour. */
UENUM(BlueprintType)
enum class ELZFloorPattern : uint8
{
	None,
	Rooms,    // pool water, lawns
	Halls,
	Checker,  // mall food court
	Stripes   // cinema carpet bands
};

UCLASS(BlueprintType)
class LEVELZERO_API ULevelZeroTheme : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---------------- Identity / flow ----------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme")
	FName LevelId = TEXT("level0");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme")
	FText DisplayName;

	/** Map opened when the player reaches this level's exit (empty = restart this level). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme")
	TSoftObjectPtr<UWorld> NextLevel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme")
	int32 Seed = 20261021;

	// ---------------- Dimensions ----------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout", meta = (ClampMin = 4, ClampMax = 64))
	int32 Width = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout", meta = (ClampMin = 4, ClampMax = 64))
	int32 Height = 30;

	/** Cell size in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout", meta = (ClampMin = 200))
	float CellSize = 400.f;

	/** Corridor width in cells (1 = classic maze, 2 = Level 0's 8 m corridors, 3+ = mall concourses). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout", meta = (ClampMin = 1, ClampMax = 4))
	int32 CorridorWidthCells = 2;

	/** Ceiling height in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout")
	float CeilingHeight = 300.f;

	/** Height of the maze walls (shelving racks, showroom partitions). 0 = full height up to the ceiling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout")
	float PartitionHeight = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout")
	float WallThickness = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout")
	ELZLayoutStyle LayoutStyle = ELZLayoutStyle::Maze;

	/** Aisles: a cross-aisle every N cells (its width in cells below). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout", meta = (ClampMin = 2))
	int32 CrossAisleEvery = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout", meta = (ClampMin = 1))
	int32 CrossAisleWidth = 2;

	/** Ceiling height of room cells (auditoriums, showroom void); 0 = CeilingHeight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout")
	float RoomCeilingHeight = 0.f;

	/** Ceiling height of pillar-hall cells (mall atrium); 0 = CeilingHeight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout")
	float HallCeilingHeight = 0.f;

	/** Pillar halls: a (non-walkable, greybox) upper-floor balcony ring on the hall's edge cells at this height; 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout")
	float BalconyHeight = 0.f;

	/** Room cells: visual stepped seat tiers (cinema auditoriums). No collision. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout")
	bool bSeatTiersInRooms = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout")
	int32 NumRooms = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout")
	FIntPoint RoomSizeMin = FIntPoint(3, 3);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout")
	FIntPoint RoomSizeMax = FIntPoint(6, 5);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout")
	int32 NumPillarHalls = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout")
	FIntPoint PillarHallSizeMin = FIntPoint(4, 4);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout")
	FIntPoint PillarHallSizeMax = FIntPoint(8, 7);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout")
	float PillarSize = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout", meta = (ClampMin = 0, ClampMax = 1))
	float LoopChance = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout", meta = (ClampMin = 0, ClampMax = 1))
	float StubWallChance = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout", meta = (ClampMin = 0, ClampMax = 1))
	float RoomDoorwayChance = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout", meta = (ClampMin = 0, ClampMax = 1))
	float CorridorDoorwayChance = 0.04f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout")
	float DoorWidth = 170.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout")
	float DoorHeight = 230.f;

	/** Tiles farther than this (Chebyshev cells) are hidden; keep FogEnd below about (StreamRadiusCells - 1) * CellSize. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Layout", meta = (ClampMin = 1))
	int32 StreamRadiusCells = 5;

	// ---------------- Palette + lighting (M_LZ_Surface parameters) ----------------
	/** Apply the colours below as dynamic instances of the generator's base materials. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Palette")
	bool bApplyPalette = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Palette")
	FLinearColor WallColor = FLinearColor(0.80f, 0.70f, 0.38f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Palette")
	FLinearColor FloorColor = FLinearColor(0.46f, 0.39f, 0.20f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Palette")
	FLinearColor AltFloorColor = FLinearColor(0.2f, 0.2f, 0.2f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Palette")
	ELZFloorPattern AltFloorPattern = ELZFloorPattern::None;

	/** Warm/cool cast of the self-lit surfaces (M_LZ_Surface LightTint; Level 0 = warm fluorescent). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Palette")
	FLinearColor LightTint = FLinearColor(1.0f, 0.93f, 0.74f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Palette")
	FLinearColor CeilingColor = FLinearColor(0.82f, 0.79f, 0.66f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Palette")
	FLinearColor TrimColor = FLinearColor(0.66f, 0.56f, 0.30f);

	/** Pillars (0 alpha = use WallColor). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Palette")
	FLinearColor PillarColor = FLinearColor(0.f, 0.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Palette")
	FLinearColor LightColor = FLinearColor(1.0f, 0.95f, 0.78f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Palette")
	float LightEmissive = 3.5f;

	/** Base-shader light levels (see M_LZ_Surface). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Palette")
	float Ambient = 0.38f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Palette")
	float Bounce = 0.62f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Palette")
	float FloorLight = 0.62f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Palette")
	float CeilLight = 0.80f;

	/** Multiplies the Tripo props' self-lit levels (1 = authored). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Palette")
	float PropBrightness = 1.f;

	// ---------------- Fog ----------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Fog")
	FLinearColor FogColor = FLinearColor(0.30f, 0.26f, 0.13f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Fog")
	float FogStart = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Fog")
	float FogEnd = 3400.f;

	// ---------------- Lights ----------------
	/** One ceiling fixture every N cells in X and Y (1 = every cell). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Lights", meta = (ClampMin = 1, ClampMax = 8))
	int32 LightSpacingCells = 1;

	/** Fixture props: every light cell gets a fixture (dead or lit). Off = LitPanelChance applies, the rest stay dark. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Lights")
	bool bAllLightCellsLit = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Lights", meta = (ClampMin = 0, ClampMax = 1))
	float LitPanelChance = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Lights", meta = (ClampMin = 0, ClampMax = 1))
	float DeadPanelChance = 0.08f;

	/** Uniform scale on the ceiling fixture mesh (warehouse high-bays, mall skylights). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Lights")
	float LightFixtureScale = 1.f;

	// ---------------- Prop sets ----------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Props")
	bool bPlaceProps = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Props")
	TObjectPtr<UStaticMesh> CeilingLightMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Props")
	TObjectPtr<UStaticMesh> WallVentMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Props", meta = (ClampMin = 0, ClampMax = 1))
	float WallVentChance = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Props")
	float WallVentZ = 228.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Props")
	TArray<TObjectPtr<UStaticMesh>> ClutterWallMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Props")
	TArray<TObjectPtr<UStaticMesh>> ClutterFreeMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Props")
	int32 NumClutterClusters = 16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Props")
	FIntPoint ClusterWallItems = FIntPoint(1, 3);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Props")
	FIntPoint ClusterFreeItems = FIntPoint(0, 2);

	/** Clusters may also go in corridor cells with 2+ closed walls (aisles, showroom bays). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Props")
	bool bClustersInCorridors = false;

	// ---------------- Landmark rules ----------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Landmarks")
	TArray<TObjectPtr<UStaticMesh>> LandmarkMeshes;

	/** Count per landmark mesh (inclusive range). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Landmarks")
	FIntPoint LandmarksPerMesh = FIntPoint(1, 2);

	/** Minimum BFS distance from the start cell. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Landmarks")
	int32 LandmarkMinStartDistance = 10;

	/** Minimum Chebyshev spacing between landmarks (cells). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Landmarks")
	int32 LandmarkSpacing = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Landmarks")
	bool bLandmarksInCorridors = false;
};
