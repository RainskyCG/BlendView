// Copyright 2026 RainskyCG. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FBlendViewSceneCursor
{
public:
	bool IsVisible() const
	{
		return bVisible;
	}

	const FTransform& GetTransform() const
	{
		return Transform;
	}

	FVector GetLocation() const
	{
		return Transform.GetLocation();
	}

	FQuat GetRotation() const
	{
		return Transform.GetRotation();
	}

	void SetTransform(const FTransform& InTransform)
	{
		Transform = InTransform;
		bVisible = true;
	}

	void SetLocationPreservingRotation(const FVector& InLocation)
	{
		Transform.SetLocation(InLocation);
		bVisible = true;
	}

private:
	bool bVisible = false;
	FTransform Transform = FTransform::Identity;
};
