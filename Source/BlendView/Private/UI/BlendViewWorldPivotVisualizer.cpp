// Copyright 2026 RainskyCG. All Rights Reserved.

#include "UI/BlendViewWorldPivotVisualizer.h"

#include "Compat/BlendViewViewportCompat.h"
#include "Components/LineBatchComponent.h"
#include "EditorViewportClient.h"
#include "Engine/World.h"
#include "SceneView.h"
#include "Viewport/BlendViewViewportContext.h"
#include "Viewport/BlendViewViewportProjector.h"

namespace
{
	constexpr uint32 BlendViewPivotBatchId = 24681135;
	constexpr float OuterRadiusPixels = 4.5f;
	constexpr float InnerRadiusPixels = 3.25f;
	constexpr int32 FillLines = 9;

	ULineBatchComponent* GetPivotLineBatcher(const FBlendViewViewportContext& Context)
	{
		if (!Context.ViewportClient)
		{
			return nullptr;
		}

		UWorld* World = Context.ViewportClient->GetWorld();
		return World
			? World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent)
			: nullptr;
	}
}

void FBlendViewWorldPivotVisualizer::Draw(
	const FBlendViewViewportContext& Context,
	const FVector& PivotLocation,
	const FLinearColor& PivotColor)
{
	if (!Context.IsValid() || !Context.ViewportClient)
	{
		return;
	}

	ULineBatchComponent* LineBatcher = GetPivotLineBatcher(Context);
	if (!LineBatcher)
	{
		return;
	}

	LineBatcher->ClearBatch(BlendViewPivotBatchId);

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		Context.Viewport,
		Context.ViewportClient->GetScene(),
		Context.ViewportClient->EngineShowFlags));
	const FSceneView* Scene = FBlendViewViewportProjector::CreateSceneView(Context, ViewFamily);
	if (!Scene)
	{
		return;
	}

	const float ViewportHeight = Scene->UnscaledViewRect.Height();
	const float ProjectionScale = BlendViewViewportCompat::GetProjectionMatrix(*Scene).M[1][1];
	const float PivotProjectionDepth = Scene->WorldToScreen(PivotLocation).W;
	if (ViewportHeight <= UE_SMALL_NUMBER ||
		FMath::Abs(ProjectionScale) <= UE_SMALL_NUMBER ||
		PivotProjectionDepth <= UE_SMALL_NUMBER)
	{
		return;
	}

	const float PivotPixelToWorld = Scene->IsPerspectiveProjection()
		? 2.0f * PivotProjectionDepth / (ViewportHeight * ProjectionScale)
		: 2.0f / (ViewportHeight * ProjectionScale);
	const FVector DiscRight = Scene->GetViewRight().GetSafeNormal() * OuterRadiusPixels * PivotPixelToWorld;
	const FVector DiscUp = Scene->GetViewUp().GetSafeNormal() * OuterRadiusPixels * PivotPixelToWorld;

	for (int32 Index = -FillLines; Index <= FillLines; ++Index)
	{
		const float Y = static_cast<float>(Index) / FillLines;
		const float HalfWidth = FMath::Sqrt(FMath::Max(0.0f, 1.0f - Y * Y));
		const FVector RowCenter = PivotLocation + DiscUp * Y;
		const bool bOutlineRow = FMath::Abs(Y) > InnerRadiusPixels / OuterRadiusPixels;
		const float InnerHalfWidth = bOutlineRow
			? 0.0f
			: FMath::Sqrt(FMath::Max(
				0.0f,
				FMath::Square(InnerRadiusPixels / OuterRadiusPixels) - Y * Y));

		LineBatcher->DrawLine(
			RowCenter - DiscRight * HalfWidth,
			RowCenter + DiscRight * HalfWidth,
			FLinearColor::Black,
			SDPG_Foreground,
			PivotPixelToWorld,
			0.0f,
			BlendViewPivotBatchId);
		if (InnerHalfWidth > 0.0f)
		{
			LineBatcher->DrawLine(
				RowCenter - DiscRight * InnerHalfWidth,
				RowCenter + DiscRight * InnerHalfWidth,
				PivotColor,
				SDPG_Foreground,
				PivotPixelToWorld,
				0.0f,
				BlendViewPivotBatchId);
		}
	}
}

void FBlendViewWorldPivotVisualizer::Clear(const FBlendViewViewportContext& Context)
{
	if (ULineBatchComponent* LineBatcher = GetPivotLineBatcher(Context))
	{
		LineBatcher->ClearBatch(BlendViewPivotBatchId);
	}
}
