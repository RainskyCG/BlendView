// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class FEditorViewportClient;
class UPrimitiveComponent;

enum class EBlendViewSnapTargetKind : uint8
{
	None,
	Grid,
	Vertex,
	EdgeMidpoint,
	Edge,
	Face
};

struct FBlendViewSnapCandidate
{
	FVector Location = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	EBlendViewSnapTargetKind Kind = EBlendViewSnapTargetKind::None;
	double ScreenDistance = TNumericLimits<double>::Max();
	bool bHasNormal = false;
};

struct FBlendViewEditorSnapSettings
{
	bool bLocationEnabled = false;
	bool bRotationEnabled = false;
	bool bScaleEnabled = false;
	double LocationGridSize = 10.0;
	FRotator RotationGridSize = FRotator(10.0, 10.0, 10.0);
	double ScaleGridSize = 0.25;
};

struct FBlendViewSnapQuery
{
	FVector2D ViewportPosition = FVector2D::ZeroVector;
	FVector SourceAfterDelta = FVector::ZeroVector;
	FVector TargetHintLocation = FVector::ZeroVector;
	FEditorViewportClient* ViewportClient = nullptr;
	TArray<TWeakObjectPtr<AActor>> IgnoredActors;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> IgnoredComponents;
	TFunction<bool(const FVector2D&, FVector&, FVector&)> GetViewportTraceSegment;
	TFunction<bool(const FVector&, FVector2D&)> ProjectWorldToViewport;
	double GridSizeUnrealUnits = 100.0;
	double ScreenRadiusScale = 1.0;
	bool bEnableGrid = true;
	bool bEnableGeometry = true;
	bool bEnableVertex = true;
	bool bEnableEdgeMidpoint = true;
	bool bEnableEdge = true;
	bool bEnableFace = true;
	bool bEnableStructuralEdgeLimit = false;
	bool bHasTargetHintLocation = false;
};

class FBlendViewSnapSolver
{
public:
	void Reset();

	bool FindTemporarySnapTarget(const FBlendViewSnapQuery& Query, FBlendViewSnapCandidate& OutCandidate);

	static FBlendViewEditorSnapSettings GetEditorSnapSettings();
	static double GetTemporaryTranslationGridSize(bool bUseUnrealEditorSnap);
	static FVector SnapDeltaToGrid(const FVector& Delta, double GridSize);
	static double SnapScalarToGrid(double Value, double GridSize);
	static FVector CleanNearIntegerVector(const FVector& Value);
	static bool IsCandidatePreferred(
		const FBlendViewSnapCandidate& Candidate,
		const FBlendViewSnapCandidate& CurrentBest);

private:
	bool FindGeometrySnapTarget(const FBlendViewSnapQuery& Query, FBlendViewSnapCandidate& OutCandidate);
	bool FindGeometrySnapTargetAtViewportPosition(
		const FBlendViewSnapQuery& Query,
		const FVector2D& TraceViewportPosition,
		bool bAllowFaceFallback,
		FBlendViewSnapCandidate& OutCandidate);
	bool FindStaticMeshTriangleSnapTarget(
		const FHitResult& Hit,
		const FBlendViewSnapQuery& Query,
		bool bAllowFaceFallback,
		FBlendViewSnapCandidate& OutCandidate);
	bool FindGridSnapTarget(const FBlendViewSnapQuery& Query, FBlendViewSnapCandidate& OutCandidate) const;
	bool ApplyHysteresis(const FBlendViewSnapQuery& Query, FBlendViewSnapCandidate& InOutCandidate) const;

	FBlendViewSnapCandidate LastCandidate;
	bool bHasLastCandidate = false;
};
