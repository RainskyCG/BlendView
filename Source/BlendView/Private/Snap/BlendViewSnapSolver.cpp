// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Snap/BlendViewSnapSolver.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RawIndexBuffer.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "StaticMeshResources.h"
#include "Tools/BlendViewTransformPrecision.h"

namespace
{
	constexpr double TemporaryGridSizeUnrealUnits = 100.0;
	constexpr double BlenderSnapMinDistancePixels = 30.0;
	constexpr double VertexScreenRadiusPixels = BlenderSnapMinDistancePixels;
	constexpr double EdgeMidpointScreenRadiusPixels = BlenderSnapMinDistancePixels;
	constexpr double EdgeScreenRadiusPixels = BlenderSnapMinDistancePixels;
	constexpr double GridScreenRadiusPixels = BlenderSnapMinDistancePixels;
	constexpr double SnapHysteresisPixels = 5.0;
	constexpr double StructuralPositionWeldScale = 1000.0;
	constexpr double StructuralEdgeAngleDegrees = 30.0;
	constexpr double StructuralVertexTurnAngleDegrees = 15.0;

	FVector OrientNormalToVisibleTraceSide(const FVector& Normal, const FHitResult& Hit)
	{
		FVector OrientedNormal = Normal.GetSafeNormal();
		if (OrientedNormal.IsNearlyZero())
		{
			return FVector::ZeroVector;
		}

		const FVector TraceDirection = (Hit.TraceEnd - Hit.TraceStart).GetSafeNormal();
		if (!TraceDirection.IsNearlyZero() && FVector::DotProduct(OrientedNormal, TraceDirection) > 0.0)
		{
			OrientedNormal *= -1.0;
		}
		return OrientedNormal;
	}
	struct FQuantizedPositionKey
	{
		int64 X = 0;
		int64 Y = 0;
		int64 Z = 0;

		friend bool operator==(const FQuantizedPositionKey& A, const FQuantizedPositionKey& B)
		{
			return A.X == B.X && A.Y == B.Y && A.Z == B.Z;
		}

		friend uint32 GetTypeHash(const FQuantizedPositionKey& Key)
		{
			return HashCombine(HashCombine(GetTypeHash(Key.X), GetTypeHash(Key.Y)), GetTypeHash(Key.Z));
		}
	};

	struct FStructuralEdgeKey
	{
		int32 A = INDEX_NONE;
		int32 B = INDEX_NONE;

		FStructuralEdgeKey() = default;

		FStructuralEdgeKey(const int32 InA, const int32 InB)
			: A(FMath::Min(InA, InB))
			, B(FMath::Max(InA, InB))
		{
		}

		friend bool operator==(const FStructuralEdgeKey& Left, const FStructuralEdgeKey& Right)
		{
			return Left.A == Right.A && Left.B == Right.B;
		}

		friend uint32 GetTypeHash(const FStructuralEdgeKey& Key)
		{
			return HashCombine(GetTypeHash(Key.A), GetTypeHash(Key.B));
		}
	};

	struct FStructuralEdgeTopology
	{
		FVector FirstFaceNormal = FVector::ZeroVector;
		FVector SecondFaceNormal = FVector::ZeroVector;
		int32 FaceCount = 0;

		void AddFace(const FVector& FaceNormal)
		{
			if (FaceCount == 0)
			{
				FirstFaceNormal = FaceNormal;
			}
			else if (FaceCount == 1)
			{
				SecondFaceNormal = FaceNormal;
			}
			++FaceCount;
		}
	};

	struct FStructuralMeshCache
	{
		const FStaticMeshRenderData* RenderData = nullptr;
		FGuid LightingGuid;
		int32 VertexCount = 0;
		int32 IndexCount = 0;
		TArray<int32> WeldedVertexIds;
		TSet<int32> StructuralVertices;
		TSet<FStructuralEdgeKey> StructuralEdges;
		TMap<FStructuralEdgeKey, FVector> StructuralEdgeChainMidpoints;
	};

	FQuantizedPositionKey QuantizeStructuralPosition(const FVector& Position)
	{
		return {
			FMath::RoundToInt64(Position.X * StructuralPositionWeldScale),
			FMath::RoundToInt64(Position.Y * StructuralPositionWeldScale),
			FMath::RoundToInt64(Position.Z * StructuralPositionWeldScale)};
	}

	bool IsStructuralEdge(const FStructuralEdgeTopology& Edge)
	{
		if (Edge.FaceCount != 2)
		{
			return Edge.FaceCount > 0;
		}

		const double StructuralEdgeDotThreshold =
			FMath::Cos(FMath::DegreesToRadians(StructuralEdgeAngleDegrees));
		return FVector::DotProduct(Edge.FirstFaceNormal, Edge.SecondFaceNormal) <= StructuralEdgeDotThreshold;
	}

	FStructuralMeshCache BuildStructuralMeshCache(const UStaticMesh* StaticMesh)
	{
		FStructuralMeshCache Cache;
		const FStaticMeshRenderData* RenderData = StaticMesh ? StaticMesh->GetRenderData() : nullptr;
		if (!RenderData || RenderData->LODResources.IsEmpty())
		{
			return Cache;
		}

		const FStaticMeshLODResources& LOD = RenderData->LODResources[0];
		const FIndexArrayView Indices = LOD.IndexBuffer.GetArrayView();
		const FPositionVertexBuffer& Vertices = LOD.VertexBuffers.PositionVertexBuffer;
		Cache.RenderData = RenderData;
		Cache.LightingGuid = StaticMesh->GetLightingGuid();
		Cache.VertexCount = static_cast<int32>(Vertices.GetNumVertices());
		Cache.IndexCount = Indices.Num();
		Cache.WeldedVertexIds.SetNum(Cache.VertexCount);

		TMap<FQuantizedPositionKey, int32> WeldedVertexByPosition;
		TArray<FVector> WeldedPositions;
		WeldedVertexByPosition.Reserve(Cache.VertexCount);
		WeldedPositions.Reserve(Cache.VertexCount);
		for (int32 VertexIndex = 0; VertexIndex < Cache.VertexCount; ++VertexIndex)
		{
			const FVector Position(Vertices.VertexPosition(VertexIndex));
			const FQuantizedPositionKey PositionKey = QuantizeStructuralPosition(Position);
			if (const int32* ExistingId = WeldedVertexByPosition.Find(PositionKey))
			{
				Cache.WeldedVertexIds[VertexIndex] = *ExistingId;
				continue;
			}

			const int32 WeldedId = WeldedPositions.Add(Position);
			WeldedVertexByPosition.Add(PositionKey, WeldedId);
			Cache.WeldedVertexIds[VertexIndex] = WeldedId;
		}

		TMap<FStructuralEdgeKey, FStructuralEdgeTopology> EdgeTopology;
		EdgeTopology.Reserve(Indices.Num());
		for (int32 FirstIndex = 0; FirstIndex + 2 < Indices.Num(); FirstIndex += 3)
		{
			const uint32 RawIndices[3] = {
				Indices[FirstIndex],
				Indices[FirstIndex + 1],
				Indices[FirstIndex + 2]};
			if (RawIndices[0] >= static_cast<uint32>(Cache.VertexCount) ||
				RawIndices[1] >= static_cast<uint32>(Cache.VertexCount) ||
				RawIndices[2] >= static_cast<uint32>(Cache.VertexCount))
			{
				continue;
			}

			const int32 WeldedIds[3] = {
				Cache.WeldedVertexIds[RawIndices[0]],
				Cache.WeldedVertexIds[RawIndices[1]],
				Cache.WeldedVertexIds[RawIndices[2]]};
			if (WeldedIds[0] == WeldedIds[1] || WeldedIds[1] == WeldedIds[2] || WeldedIds[2] == WeldedIds[0])
			{
				continue;
			}

			const FVector FaceNormal = FVector::CrossProduct(
				WeldedPositions[WeldedIds[1]] - WeldedPositions[WeldedIds[0]],
				WeldedPositions[WeldedIds[2]] - WeldedPositions[WeldedIds[0]]).GetSafeNormal();
			if (FaceNormal.IsNearlyZero())
			{
				continue;
			}

			for (int32 EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
			{
				const FStructuralEdgeKey Edge(WeldedIds[EdgeIndex], WeldedIds[(EdgeIndex + 1) % 3]);
				EdgeTopology.FindOrAdd(Edge).AddFace(FaceNormal);
			}
		}

		TMap<int32, TSet<int32>> StructuralNeighbors;
		for (const TPair<FStructuralEdgeKey, FStructuralEdgeTopology>& Pair : EdgeTopology)
		{
			if (!IsStructuralEdge(Pair.Value))
			{
				continue;
			}

			Cache.StructuralEdges.Add(Pair.Key);
			StructuralNeighbors.FindOrAdd(Pair.Key.A).Add(Pair.Key.B);
			StructuralNeighbors.FindOrAdd(Pair.Key.B).Add(Pair.Key.A);
		}

		const double StraightDotThreshold =
			-FMath::Cos(FMath::DegreesToRadians(StructuralVertexTurnAngleDegrees));
		for (const TPair<int32, TSet<int32>>& Pair : StructuralNeighbors)
		{
			const int32 NeighborCount = Pair.Value.Num();
			if (NeighborCount == 1 || NeighborCount > 2)
			{
				Cache.StructuralVertices.Add(Pair.Key);
				continue;
			}
			if (NeighborCount != 2)
			{
				continue;
			}

			int32 NeighborIds[2] = {INDEX_NONE, INDEX_NONE};
			int32 NeighborIndex = 0;
			for (const int32 NeighborId : Pair.Value)
			{
				NeighborIds[NeighborIndex++] = NeighborId;
			}

			const FVector DirectionA = (WeldedPositions[NeighborIds[0]] - WeldedPositions[Pair.Key]).GetSafeNormal();
			const FVector DirectionB = (WeldedPositions[NeighborIds[1]] - WeldedPositions[Pair.Key]).GetSafeNormal();
			if (FVector::DotProduct(DirectionA, DirectionB) > StraightDotThreshold)
			{
				Cache.StructuralVertices.Add(Pair.Key);
			}
		}

		TSet<FStructuralEdgeKey> VisitedStructuralEdges;
		auto RegisterStructuralChainMidpoint = [&](const TArray<int32>& ChainVertices)
		{
			if (ChainVertices.Num() < 2)
			{
				return;
			}

			double ChainLength = 0.0;
			for (int32 Index = 1; Index < ChainVertices.Num(); ++Index)
			{
				ChainLength += FVector::Distance(
					WeldedPositions[ChainVertices[Index - 1]],
					WeldedPositions[ChainVertices[Index]]);
			}
			if (ChainLength <= UE_DOUBLE_SMALL_NUMBER)
			{
				return;
			}

			const double HalfLength = ChainLength * 0.5;
			double TraversedLength = 0.0;
			FVector ChainMidpoint = WeldedPositions[ChainVertices[0]];
			for (int32 Index = 1; Index < ChainVertices.Num(); ++Index)
			{
				const FVector& SegmentStart = WeldedPositions[ChainVertices[Index - 1]];
				const FVector& SegmentEnd = WeldedPositions[ChainVertices[Index]];
				const double SegmentLength = FVector::Distance(SegmentStart, SegmentEnd);
				if (TraversedLength + SegmentLength >= HalfLength)
				{
					const double Alpha = SegmentLength > UE_DOUBLE_SMALL_NUMBER
						? (HalfLength - TraversedLength) / SegmentLength
						: 0.0;
					ChainMidpoint = FMath::Lerp(SegmentStart, SegmentEnd, Alpha);
					break;
				}
				TraversedLength += SegmentLength;
			}

			for (int32 Index = 1; Index < ChainVertices.Num(); ++Index)
			{
				Cache.StructuralEdgeChainMidpoints.Add(
					FStructuralEdgeKey(ChainVertices[Index - 1], ChainVertices[Index]),
					ChainMidpoint);
			}
		};

		for (const int32 ChainStart : Cache.StructuralVertices)
		{
			const TSet<int32>* StartNeighbors = StructuralNeighbors.Find(ChainStart);
			if (!StartNeighbors)
			{
				continue;
			}

			for (const int32 FirstNeighbor : *StartNeighbors)
			{
				const FStructuralEdgeKey FirstEdge(ChainStart, FirstNeighbor);
				if (VisitedStructuralEdges.Contains(FirstEdge))
				{
					continue;
				}

				TArray<int32> ChainVertices;
				ChainVertices.Add(ChainStart);
				int32 PreviousVertex = ChainStart;
				int32 CurrentVertex = FirstNeighbor;
				while (CurrentVertex != INDEX_NONE)
				{
					ChainVertices.Add(CurrentVertex);
					VisitedStructuralEdges.Add(FStructuralEdgeKey(PreviousVertex, CurrentVertex));
					if (Cache.StructuralVertices.Contains(CurrentVertex))
					{
						break;
					}

					const TSet<int32>* CurrentNeighbors = StructuralNeighbors.Find(CurrentVertex);
					if (!CurrentNeighbors || CurrentNeighbors->Num() != 2)
					{
						break;
					}

					int32 NextVertex = INDEX_NONE;
					for (const int32 Neighbor : *CurrentNeighbors)
					{
						if (Neighbor != PreviousVertex)
						{
							NextVertex = Neighbor;
							break;
						}
					}
					if (NextVertex == INDEX_NONE ||
						VisitedStructuralEdges.Contains(FStructuralEdgeKey(CurrentVertex, NextVertex)))
					{
						break;
					}

					PreviousVertex = CurrentVertex;
					CurrentVertex = NextVertex;
				}

				RegisterStructuralChainMidpoint(ChainVertices);
			}
		}

		return Cache;
	}

	const FStructuralMeshCache* GetStructuralMeshCache(const UStaticMesh* StaticMesh)
	{
		if (!StaticMesh)
		{
			return nullptr;
		}

		static TMap<TWeakObjectPtr<UStaticMesh>, FStructuralMeshCache> Caches;
		static uint32 CacheAccessCount = 0;
		if ((++CacheAccessCount & 0xFFu) == 0)
		{
			for (auto Iterator = Caches.CreateIterator(); Iterator; ++Iterator)
			{
				if (!Iterator.Key().IsValid())
				{
					Iterator.RemoveCurrent();
				}
			}
		}

		const TWeakObjectPtr<UStaticMesh> CacheKey(const_cast<UStaticMesh*>(StaticMesh));
		const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
		const int32 VertexCount =
			RenderData && !RenderData->LODResources.IsEmpty()
				? static_cast<int32>(RenderData->LODResources[0].VertexBuffers.PositionVertexBuffer.GetNumVertices())
				: 0;
		const int32 IndexCount =
			RenderData && !RenderData->LODResources.IsEmpty()
				? RenderData->LODResources[0].IndexBuffer.GetArrayView().Num()
				: 0;

		FStructuralMeshCache* Cache = Caches.Find(CacheKey);
		if (!Cache ||
			Cache->RenderData != RenderData ||
			Cache->LightingGuid != StaticMesh->GetLightingGuid() ||
			Cache->VertexCount != VertexCount ||
			Cache->IndexCount != IndexCount)
		{
			Cache = &Caches.Add(CacheKey, BuildStructuralMeshCache(StaticMesh));
		}
		return Cache;
	}

	double CleanNearInteger(const double Value)
	{
		return FBlendViewTransformPrecision::CleanNearInteger(Value);
	}

	FVector CleanNearInteger(const FVector& Value)
	{
		return FBlendViewTransformPrecision::CleanNearInteger(Value);
	}

	bool IsScreenDistanceSnapKind(const EBlendViewSnapTargetKind Kind)
	{
		return Kind == EBlendViewSnapTargetKind::Vertex ||
			Kind == EBlendViewSnapTargetKind::EdgeMidpoint ||
			Kind == EBlendViewSnapTargetKind::Edge ||
			Kind == EBlendViewSnapTargetKind::Grid;
	}

	double GetRadiusForKind(const EBlendViewSnapTargetKind Kind, const double RadiusScale)
	{
		double Radius = 0.0;
		switch (Kind)
		{
		case EBlendViewSnapTargetKind::Vertex:
			Radius = VertexScreenRadiusPixels;
			break;
		case EBlendViewSnapTargetKind::EdgeMidpoint:
			Radius = EdgeMidpointScreenRadiusPixels;
			break;
		case EBlendViewSnapTargetKind::Edge:
			Radius = EdgeScreenRadiusPixels;
			break;
		case EBlendViewSnapTargetKind::Grid:
			Radius = GridScreenRadiusPixels;
			break;
		default:
			break;
		}

		return Radius * FMath::Max(RadiusScale, UE_SMALL_NUMBER);
	}

	double GetNearbyGeometrySearchRadius(const FBlendViewSnapQuery& Query)
	{
		double Radius = 0.0;
		if (Query.bEnableVertex)
		{
			Radius = FMath::Max(Radius, GetRadiusForKind(EBlendViewSnapTargetKind::Vertex, Query.ScreenRadiusScale));
		}
		if (Query.bEnableEdgeMidpoint)
		{
			Radius = FMath::Max(Radius, GetRadiusForKind(EBlendViewSnapTargetKind::EdgeMidpoint, Query.ScreenRadiusScale));
		}
		if (Query.bEnableEdge)
		{
			Radius = FMath::Max(Radius, GetRadiusForKind(EBlendViewSnapTargetKind::Edge, Query.ScreenRadiusScale));
		}
		return Radius;
	}

	int32 GetSnapPriority(const EBlendViewSnapTargetKind Kind)
	{
		switch (Kind)
		{
		case EBlendViewSnapTargetKind::Vertex:
			return 0;
		case EBlendViewSnapTargetKind::EdgeMidpoint:
			return 1;
		case EBlendViewSnapTargetKind::Edge:
			return 2;
		case EBlendViewSnapTargetKind::Grid:
			return 3;
		case EBlendViewSnapTargetKind::Face:
			return 4;
		default:
			return 100;
		}
	}

	FVector ClosestPointOnSegmentByScreenSpace(
		const FVector& WorldA,
		const FVector& WorldB,
		const FVector2D& ScreenA,
		const FVector2D& ScreenB,
		const FVector2D& ScreenPoint,
		double& OutScreenDistance)
	{
		const FVector2D ScreenEdge = ScreenB - ScreenA;
		const double EdgeLengthSquared = ScreenEdge.SizeSquared();
		if (EdgeLengthSquared <= UE_SMALL_NUMBER)
		{
			OutScreenDistance = FVector2D::Distance(ScreenPoint, ScreenA);
			return WorldA;
		}

		const double T = FMath::Clamp(
			FVector2D::DotProduct(ScreenPoint - ScreenA, ScreenEdge) / EdgeLengthSquared,
			0.0,
			1.0);
		OutScreenDistance = FVector2D::Distance(ScreenPoint, ScreenA + ScreenEdge * T);
		return FMath::Lerp(WorldA, WorldB, T);
	}

	void ConsiderCandidate(
		const FBlendViewSnapCandidate& Candidate,
		const double MaxDistance,
		bool& bHasBestCandidate,
		FBlendViewSnapCandidate& InOutBestCandidate)
	{
		if (Candidate.ScreenDistance > MaxDistance)
		{
			return;
		}

		if (!bHasBestCandidate || FBlendViewSnapSolver::IsCandidatePreferred(Candidate, InOutBestCandidate))
		{
			InOutBestCandidate = Candidate;
			bHasBestCandidate = true;
		}
	}
}

bool FBlendViewSnapSolver::IsCandidatePreferred(
	const FBlendViewSnapCandidate& Candidate,
	const FBlendViewSnapCandidate& CurrentBest)
{
	const int32 CandidatePriority = GetSnapPriority(Candidate.Kind);
	const int32 CurrentPriority = GetSnapPriority(CurrentBest.Kind);
	if (CandidatePriority != CurrentPriority)
	{
		return CandidatePriority < CurrentPriority;
	}

	return Candidate.ScreenDistance < CurrentBest.ScreenDistance;
}

void FBlendViewSnapSolver::Reset()
{
	LastCandidate = FBlendViewSnapCandidate();
	bHasLastCandidate = false;
}

bool FBlendViewSnapSolver::FindTemporarySnapTarget(
	const FBlendViewSnapQuery& Query,
	FBlendViewSnapCandidate& OutCandidate)
{
	FBlendViewSnapCandidate GeometryCandidate;
	const bool bHasGeometryCandidate =
		Query.bEnableGeometry && FindGeometrySnapTarget(Query, GeometryCandidate);

	FBlendViewSnapCandidate GridCandidate;
	const bool bHasGridCandidate =
		Query.bEnableGrid && FindGridSnapTarget(Query, GridCandidate);

	if (bHasGeometryCandidate && bHasGridCandidate)
	{
		OutCandidate = IsCandidatePreferred(GridCandidate, GeometryCandidate)
			? GridCandidate
			: GeometryCandidate;
		LastCandidate = OutCandidate;
		bHasLastCandidate = true;
		return true;
	}

	if (bHasGeometryCandidate)
	{
		OutCandidate = GeometryCandidate;
		LastCandidate = OutCandidate;
		bHasLastCandidate = true;
		return true;
	}

	if (bHasGridCandidate)
	{
		OutCandidate = GridCandidate;
		LastCandidate = OutCandidate;
		bHasLastCandidate = true;
		return true;
	}

	Reset();
	return false;
}

FBlendViewEditorSnapSettings FBlendViewSnapSolver::GetEditorSnapSettings()
{
	FBlendViewEditorSnapSettings SnapSettings;
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	if (!ViewportSettings || !GEditor)
	{
		return SnapSettings;
	}

	SnapSettings.bLocationEnabled = ViewportSettings->GridEnabled != 0;
	SnapSettings.bRotationEnabled = ViewportSettings->RotGridEnabled != 0;
	SnapSettings.bScaleEnabled = ViewportSettings->SnapScaleEnabled != 0;
	SnapSettings.LocationGridSize = FMath::Max(static_cast<double>(GEditor->GetGridSize()), UE_SMALL_NUMBER);
	SnapSettings.RotationGridSize = GEditor->GetRotGridSize();
	SnapSettings.ScaleGridSize = FMath::Max(static_cast<double>(GEditor->GetScaleGridSize()), UE_SMALL_NUMBER);
	return SnapSettings;
}

double FBlendViewSnapSolver::GetTemporaryTranslationGridSize(const bool bUseUnrealEditorSnap)
{
	const FBlendViewEditorSnapSettings SnapSettings = GetEditorSnapSettings();
	if (bUseUnrealEditorSnap && SnapSettings.bLocationEnabled)
	{
		return SnapSettings.LocationGridSize;
	}

	return TemporaryGridSizeUnrealUnits;
}

FVector FBlendViewSnapSolver::SnapDeltaToGrid(const FVector& Delta, const double GridSize)
{
	if (GridSize <= UE_SMALL_NUMBER)
	{
		return Delta;
	}

	return FVector(
		FMath::GridSnap(Delta.X, GridSize),
		FMath::GridSnap(Delta.Y, GridSize),
		FMath::GridSnap(Delta.Z, GridSize));
}

double FBlendViewSnapSolver::SnapScalarToGrid(const double Value, const double GridSize)
{
	return GridSize > UE_SMALL_NUMBER ? FMath::GridSnap(Value, GridSize) : Value;
}

FVector FBlendViewSnapSolver::CleanNearIntegerVector(const FVector& Value)
{
	return CleanNearInteger(Value);
}

bool FBlendViewSnapSolver::FindGeometrySnapTarget(
	const FBlendViewSnapQuery& Query,
	FBlendViewSnapCandidate& OutCandidate)
{
	if (!Query.ViewportClient || !Query.GetViewportTraceSegment)
	{
		return false;
	}

	bool bHasBestCandidate = false;
	FBlendViewSnapCandidate Candidate;
	FBlendViewSnapCandidate BestCandidate;
	if (FindGeometrySnapTargetAtViewportPosition(Query, Query.ViewportPosition, Query.bEnableFace, Candidate))
	{
		BestCandidate = Candidate;
		bHasBestCandidate = true;
	}

	const double SearchRadius = GetNearbyGeometrySearchRadius(Query);
	if (SearchRadius > UE_SMALL_NUMBER)
	{
		const FVector2D Directions[] = {
			FVector2D(1.0, 0.0),
			FVector2D(-1.0, 0.0),
			FVector2D(0.0, 1.0),
			FVector2D(0.0, -1.0),
			FVector2D(0.70710678118, 0.70710678118),
			FVector2D(-0.70710678118, 0.70710678118),
			FVector2D(0.70710678118, -0.70710678118),
			FVector2D(-0.70710678118, -0.70710678118),
			FVector2D(0.92387953251, 0.38268343236),
			FVector2D(0.38268343236, 0.92387953251),
			FVector2D(-0.38268343236, 0.92387953251),
			FVector2D(-0.92387953251, 0.38268343236),
			FVector2D(-0.92387953251, -0.38268343236),
			FVector2D(-0.38268343236, -0.92387953251),
			FVector2D(0.38268343236, -0.92387953251),
			FVector2D(0.92387953251, -0.38268343236)
		};
		const double SampleRadii[] = {
			SearchRadius * 0.25,
			SearchRadius * 0.5,
			SearchRadius * 0.75,
			SearchRadius
		};

		for (const double SampleRadius : SampleRadii)
		{
			for (const FVector2D& Direction : Directions)
			{
				const FVector2D SamplePosition = Query.ViewportPosition + Direction * SampleRadius;
				if (!FindGeometrySnapTargetAtViewportPosition(Query, SamplePosition, false, Candidate))
				{
					continue;
				}

				if (!bHasBestCandidate || IsCandidatePreferred(Candidate, BestCandidate))
				{
					BestCandidate = Candidate;
					bHasBestCandidate = true;
				}
			}
		}
	}

	if (!bHasBestCandidate)
	{
		return false;
	}

	OutCandidate = BestCandidate;
	ApplyHysteresis(Query, OutCandidate);
	return true;
}

bool FBlendViewSnapSolver::FindGeometrySnapTargetAtViewportPosition(
	const FBlendViewSnapQuery& Query,
	const FVector2D& TraceViewportPosition,
	const bool bAllowFaceFallback,
	FBlendViewSnapCandidate& OutCandidate)
{
	FVector RayStart;
	FVector RayEnd;
	if (!Query.GetViewportTraceSegment(TraceViewportPosition, RayStart, RayEnd))
	{
		return false;
	}

	UWorld* World = Query.ViewportClient->GetWorld();
	if (!World)
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BlendViewTemporarySnapTargetTrace), true);
	QueryParams.bReturnFaceIndex = true;
	for (const TWeakObjectPtr<AActor>& IgnoredActor : Query.IgnoredActors)
	{
		if (AActor* Actor = IgnoredActor.Get())
		{
			QueryParams.AddIgnoredActor(Actor);
		}
	}
	for (const TWeakObjectPtr<UPrimitiveComponent>& IgnoredComponent : Query.IgnoredComponents)
	{
		if (UPrimitiveComponent* Component = IgnoredComponent.Get())
		{
			QueryParams.AddIgnoredComponent(Component);
		}
	}

	if (!World->LineTraceSingleByChannel(Hit, RayStart, RayEnd, ECC_Visibility, QueryParams) ||
		!Hit.IsValidBlockingHit())
	{
		return false;
	}

	if (FindStaticMeshTriangleSnapTarget(Hit, Query, bAllowFaceFallback, OutCandidate))
	{
		return true;
	}

	if (bAllowFaceFallback && Query.bEnableFace)
	{
		OutCandidate.Location = Hit.ImpactPoint;
		OutCandidate.Normal = OrientNormalToVisibleTraceSide(Hit.ImpactNormal, Hit);
		OutCandidate.bHasNormal = !OutCandidate.Normal.IsNearlyZero();
		OutCandidate.Kind = EBlendViewSnapTargetKind::Face;
		OutCandidate.ScreenDistance = 0.0;
		return true;
	}

	return false;
}

bool FBlendViewSnapSolver::FindStaticMeshTriangleSnapTarget(
	const FHitResult& Hit,
	const FBlendViewSnapQuery& Query,
	const bool bAllowFaceFallback,
	FBlendViewSnapCandidate& OutCandidate)
{
	const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Hit.GetComponent());
	if (!StaticMeshComponent)
	{
		return false;
	}

	const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
	if (!StaticMesh || !StaticMesh->HasValidRenderData() || Hit.FaceIndex < 0 || !Query.ProjectWorldToViewport)
	{
		return false;
	}

	const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
	if (!RenderData || RenderData->LODResources.IsEmpty())
	{
		return false;
	}

	const FStaticMeshLODResources& LOD = RenderData->LODResources[0];
	const FIndexArrayView Indices = LOD.IndexBuffer.GetArrayView();
	const FPositionVertexBuffer& Vertices = LOD.VertexBuffers.PositionVertexBuffer;
	const int32 FirstIndex = Hit.FaceIndex * 3;
	if (FirstIndex < 0 ||
		FirstIndex + 2 >= Indices.Num() ||
		Indices[FirstIndex] >= static_cast<uint32>(Vertices.GetNumVertices()) ||
		Indices[FirstIndex + 1] >= static_cast<uint32>(Vertices.GetNumVertices()) ||
		Indices[FirstIndex + 2] >= static_cast<uint32>(Vertices.GetNumVertices()))
	{
		return false;
	}

	const uint32 TriangleVertexIndices[3] = {
		Indices[FirstIndex],
		Indices[FirstIndex + 1],
		Indices[FirstIndex + 2]};
	const FTransform& ComponentTransform = StaticMeshComponent->GetComponentTransform();
	const FVector TriangleWorld[3] = {
		ComponentTransform.TransformPosition(FVector(Vertices.VertexPosition(TriangleVertexIndices[0]))),
		ComponentTransform.TransformPosition(FVector(Vertices.VertexPosition(TriangleVertexIndices[1]))),
		ComponentTransform.TransformPosition(FVector(Vertices.VertexPosition(TriangleVertexIndices[2])))
	};
	FVector TriangleNormal = FVector::CrossProduct(
		TriangleWorld[1] - TriangleWorld[0],
		TriangleWorld[2] - TriangleWorld[0]).GetSafeNormal();
	if (TriangleNormal.IsNearlyZero())
	{
		TriangleNormal = Hit.ImpactNormal.GetSafeNormal();
	}
	TriangleNormal = OrientNormalToVisibleTraceSide(TriangleNormal, Hit);
	const bool bHasTriangleNormal = !TriangleNormal.IsNearlyZero();
	const FStructuralMeshCache* StructuralCache =
		Query.bEnableStructuralEdgeLimit ? GetStructuralMeshCache(StaticMesh) : nullptr;
	auto IsAllowedStructuralVertex = [&](const int32 TriangleIndex)
	{
		if (!StructuralCache)
		{
			return true;
		}
		const int32 RenderVertexIndex = static_cast<int32>(TriangleVertexIndices[TriangleIndex]);
		return StructuralCache->WeldedVertexIds.IsValidIndex(RenderVertexIndex) &&
			StructuralCache->StructuralVertices.Contains(StructuralCache->WeldedVertexIds[RenderVertexIndex]);
	};
	auto IsAllowedStructuralEdge = [&](const int32 EdgeIndex)
	{
		if (!StructuralCache)
		{
			return true;
		}
		const int32 NextIndex = (EdgeIndex + 1) % 3;
		const int32 RenderVertexA = static_cast<int32>(TriangleVertexIndices[EdgeIndex]);
		const int32 RenderVertexB = static_cast<int32>(TriangleVertexIndices[NextIndex]);
		if (!StructuralCache->WeldedVertexIds.IsValidIndex(RenderVertexA) ||
			!StructuralCache->WeldedVertexIds.IsValidIndex(RenderVertexB))
		{
			return false;
		}
		return StructuralCache->StructuralEdges.Contains(FStructuralEdgeKey(
			StructuralCache->WeldedVertexIds[RenderVertexA],
			StructuralCache->WeldedVertexIds[RenderVertexB]));
	};

	FVector2D TriangleScreen[3];
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (!Query.ProjectWorldToViewport(TriangleWorld[Index], TriangleScreen[Index]))
		{
			return false;
		}
	}

	FBlendViewSnapCandidate BestPointCandidate;
	FBlendViewSnapCandidate BestEdgeCandidate;

	bool bHasPointCandidate = false;
	if (Query.bEnableVertex)
	{
		for (int32 Index = 0; Index < 3; ++Index)
		{
			if (!IsAllowedStructuralVertex(Index))
			{
				continue;
			}

			FBlendViewSnapCandidate Candidate;
			Candidate.Location = TriangleWorld[Index];
			Candidate.Normal = TriangleNormal;
			Candidate.bHasNormal = bHasTriangleNormal;
			Candidate.Kind = EBlendViewSnapTargetKind::Vertex;
			Candidate.ScreenDistance = FVector2D::Distance(Query.ViewportPosition, TriangleScreen[Index]);
			ConsiderCandidate(
				Candidate,
				GetRadiusForKind(EBlendViewSnapTargetKind::Vertex, Query.ScreenRadiusScale),
				bHasPointCandidate,
				BestPointCandidate);
		}
	}

	bool bHasEdgeCandidate = false;
	for (int32 EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
	{
		const int32 NextIndex = (EdgeIndex + 1) % 3;

		if (Query.bEnableEdgeMidpoint)
		{
			TOptional<FVector> MidpointWorld;
			if (!StructuralCache)
			{
				MidpointWorld = (TriangleWorld[EdgeIndex] + TriangleWorld[NextIndex]) * 0.5;
			}
			else if (IsAllowedStructuralEdge(EdgeIndex))
			{
				const int32 RenderVertexA = static_cast<int32>(TriangleVertexIndices[EdgeIndex]);
				const int32 RenderVertexB = static_cast<int32>(TriangleVertexIndices[NextIndex]);
				const FStructuralEdgeKey StructuralEdge(
					StructuralCache->WeldedVertexIds[RenderVertexA],
					StructuralCache->WeldedVertexIds[RenderVertexB]);
				if (const FVector* ChainMidpoint = StructuralCache->StructuralEdgeChainMidpoints.Find(StructuralEdge))
				{
					MidpointWorld = ComponentTransform.TransformPosition(*ChainMidpoint);
				}
			}

			if (MidpointWorld.IsSet())
			{
				FVector2D MidpointScreen;
				if (Query.ProjectWorldToViewport(MidpointWorld.GetValue(), MidpointScreen))
				{
					FBlendViewSnapCandidate Candidate;
					Candidate.Location = MidpointWorld.GetValue();
					Candidate.Normal = TriangleNormal;
					Candidate.bHasNormal = bHasTriangleNormal;
					Candidate.Kind = EBlendViewSnapTargetKind::EdgeMidpoint;
					Candidate.ScreenDistance = FVector2D::Distance(Query.ViewportPosition, MidpointScreen);
					ConsiderCandidate(
						Candidate,
						GetRadiusForKind(EBlendViewSnapTargetKind::EdgeMidpoint, Query.ScreenRadiusScale),
						bHasPointCandidate,
						BestPointCandidate);
				}
			}
		}

		if (Query.bEnableEdge && IsAllowedStructuralEdge(EdgeIndex))
		{
			double EdgeDistance = 0.0;
			const FVector EdgeLocation = ClosestPointOnSegmentByScreenSpace(
				TriangleWorld[EdgeIndex],
				TriangleWorld[NextIndex],
				TriangleScreen[EdgeIndex],
				TriangleScreen[NextIndex],
				Query.ViewportPosition,
				EdgeDistance);

			FBlendViewSnapCandidate Candidate;
			Candidate.Location = EdgeLocation;
			Candidate.Normal = TriangleNormal;
			Candidate.bHasNormal = bHasTriangleNormal;
			Candidate.Kind = EBlendViewSnapTargetKind::Edge;
			Candidate.ScreenDistance = EdgeDistance;
			ConsiderCandidate(
				Candidate,
				GetRadiusForKind(EBlendViewSnapTargetKind::Edge, Query.ScreenRadiusScale),
				bHasEdgeCandidate,
				BestEdgeCandidate);
		}
	}

	if (bHasPointCandidate)
	{
		OutCandidate = BestPointCandidate;
		return true;
	}

	if (bHasEdgeCandidate)
	{
		OutCandidate = BestEdgeCandidate;
		return true;
	}

	if (bAllowFaceFallback && Query.bEnableFace)
	{
		OutCandidate.Location = Hit.ImpactPoint;
		OutCandidate.Normal = TriangleNormal;
		OutCandidate.bHasNormal = bHasTriangleNormal;
		OutCandidate.Kind = EBlendViewSnapTargetKind::Face;
		OutCandidate.ScreenDistance = 0.0;
		return true;
	}

	return false;
}

bool FBlendViewSnapSolver::FindGridSnapTarget(
	const FBlendViewSnapQuery& Query,
	FBlendViewSnapCandidate& OutCandidate) const
{
	if (Query.GridSizeUnrealUnits <= UE_SMALL_NUMBER || !Query.ProjectWorldToViewport)
	{
		return false;
	}

	const FVector GridSource = Query.bHasTargetHintLocation ? Query.TargetHintLocation : Query.SourceAfterDelta;
	OutCandidate.Location = SnapDeltaToGrid(GridSource, Query.GridSizeUnrealUnits);
	OutCandidate.Kind = EBlendViewSnapTargetKind::Grid;

	FVector2D GridScreenPosition;
	if (!Query.ProjectWorldToViewport(OutCandidate.Location, GridScreenPosition))
	{
		return false;
	}

	OutCandidate.ScreenDistance = FVector2D::Distance(Query.ViewportPosition, GridScreenPosition);
	const double GridRadius = GetRadiusForKind(EBlendViewSnapTargetKind::Grid, Query.ScreenRadiusScale);
	if (OutCandidate.ScreenDistance > GridRadius)
	{
		return ApplyHysteresis(Query, OutCandidate);
	}

	ApplyHysteresis(Query, OutCandidate);
	return OutCandidate.ScreenDistance <=
		GridRadius + SnapHysteresisPixels * FMath::Max(Query.ScreenRadiusScale, UE_SMALL_NUMBER);
}

bool FBlendViewSnapSolver::ApplyHysteresis(
	const FBlendViewSnapQuery& Query,
	FBlendViewSnapCandidate& InOutCandidate) const
{
	if (!bHasLastCandidate ||
		!IsScreenDistanceSnapKind(LastCandidate.Kind) ||
		LastCandidate.Kind != InOutCandidate.Kind ||
		!Query.ProjectWorldToViewport)
	{
		return false;
	}

	FVector2D LastScreenPosition;
	if (!Query.ProjectWorldToViewport(LastCandidate.Location, LastScreenPosition))
	{
		return false;
	}

	const double LastDistance = FVector2D::Distance(Query.ViewportPosition, LastScreenPosition);
	const double RadiusScale = FMath::Max(Query.ScreenRadiusScale, UE_SMALL_NUMBER);
	const double MaxDistance = GetRadiusForKind(LastCandidate.Kind, RadiusScale) + SnapHysteresisPixels * RadiusScale;
	if (LastDistance <= MaxDistance &&
		LastDistance <= InOutCandidate.ScreenDistance + SnapHysteresisPixels * RadiusScale)
	{
		InOutCandidate = LastCandidate;
		InOutCandidate.ScreenDistance = LastDistance;
		return true;
	}

	return false;
}
