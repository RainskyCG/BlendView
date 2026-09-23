// Copyright 2026 RainskyCG. All Rights Reserved.

#include "Tools/BlendViewTransformMath.h"

#include "Tools/BlendViewTransformPrecision.h"

namespace
{
	double GetSignedAxisScale(
		const FVector& ObjectAxis,
		const FVector& ConstraintScaleFactor,
		const FQuat& ConstraintRotation)
	{
		const FVector AxisInConstraintSpace = ConstraintRotation.UnrotateVector(ObjectAxis);
		const FVector TransformedAxis = ConstraintRotation.RotateVector(
			AxisInConstraintSpace * ConstraintScaleFactor);
		const double Magnitude = TransformedAxis.Size();
		if (Magnitude <= UE_SMALL_NUMBER)
		{
			return 0.0;
		}

		return FVector::DotProduct(TransformedAxis, ObjectAxis) < 0.0
			? -Magnitude
			: Magnitude;
	}

	FVector GetObjectScaleFactor(
		const FQuat& ObjectRotation,
		const FVector& ConstraintScaleFactor,
		const FQuat& ConstraintRotation)
	{
		return FVector(
			GetSignedAxisScale(
				ObjectRotation.RotateVector(FVector::XAxisVector),
				ConstraintScaleFactor,
				ConstraintRotation),
			GetSignedAxisScale(
				ObjectRotation.RotateVector(FVector::YAxisVector),
				ConstraintScaleFactor,
				ConstraintRotation),
			GetSignedAxisScale(
				ObjectRotation.RotateVector(FVector::ZAxisVector),
				ConstraintScaleFactor,
				ConstraintRotation));
	}

	FVector ReflectVectorInConstraintSpace(
		const FVector& Vector,
		const FVector& MirrorScaleFactor,
		const FQuat& ConstraintRotation,
		const bool bLocalConstraint)
	{
		const FQuat MirrorSpaceRotation = bLocalConstraint
			? ConstraintRotation.GetNormalized()
			: FQuat::Identity;
		const FVector VectorInMirrorSpace = MirrorSpaceRotation.UnrotateVector(Vector);
		return MirrorSpaceRotation.RotateVector(VectorInMirrorSpace * MirrorScaleFactor);
	}
}

FTransform FBlendViewTransformMath::BuildRotatedTransform(
	const FTransform& BaselineTransform,
	const FVector& PivotLocation,
	const FQuat& DeltaRotation)
{
	FTransform NewTransform = BaselineTransform;
	const FVector InitialLocation = BaselineTransform.GetLocation();
	NewTransform.SetLocation(FBlendViewTransformPrecision::CleanNearInteger(
		PivotLocation + DeltaRotation.RotateVector(InitialLocation - PivotLocation)));
	NewTransform.SetRotation(FBlendViewTransformPrecision::CleanNearIntegerRotation(
		(DeltaRotation * BaselineTransform.GetRotation()).GetNormalized()));
	return NewTransform;
}

FTransform FBlendViewTransformMath::BuildScaledTransform(
	const FTransform& BaselineTransform,
	const FVector& PivotLocation,
	const FVector& ScaleFactor,
	const FQuat& ConstraintRotation,
	const bool bLocalConstraint)
{
	const FVector InitialLocation = BaselineTransform.GetLocation();
	const FVector Offset = InitialLocation - PivotLocation;
	const FQuat ScaleSpaceRotation = bLocalConstraint
		? ConstraintRotation.GetNormalized()
		: FQuat::Identity;
	const FVector OffsetInScaleSpace = ScaleSpaceRotation.UnrotateVector(Offset);
	const FVector NewLocation = PivotLocation + ScaleSpaceRotation.RotateVector(
		OffsetInScaleSpace * ScaleFactor);
	const FVector ObjectScaleFactor = GetObjectScaleFactor(
		BaselineTransform.GetRotation().GetNormalized(),
		ScaleFactor,
		ScaleSpaceRotation);

	// Match Blender object scaling: transform each existing object axis through the
	// constraint scale, retain its signed length, and keep object rotation unchanged.
	FTransform NewTransform = BaselineTransform;
	NewTransform.SetLocation(NewLocation);
	NewTransform.SetScale3D(FBlendViewTransformPrecision::CleanNearInteger(
		BaselineTransform.GetScale3D() * ObjectScaleFactor));
	return NewTransform;
}

FTransform FBlendViewTransformMath::BuildMirroredTransform(
	const FTransform& BaselineTransform,
	const FVector& PivotLocation,
	const FVector& MirrorScaleFactor,
	const FQuat& ConstraintRotation,
	const bool bLocalConstraint)
{
	const FVector InitialLocation = BaselineTransform.GetLocation();
	const FVector NewLocation = PivotLocation + ReflectVectorInConstraintSpace(
		InitialLocation - PivotLocation,
		MirrorScaleFactor,
		ConstraintRotation,
		bLocalConstraint);

	const FQuat BaselineRotation = BaselineTransform.GetRotation().GetNormalized();
	const FVector NewXAxis = ReflectVectorInConstraintSpace(
		BaselineRotation.RotateVector(FVector::XAxisVector * MirrorScaleFactor.X),
		MirrorScaleFactor,
		ConstraintRotation,
		bLocalConstraint).GetSafeNormal();
	const FVector NewYAxis = ReflectVectorInConstraintSpace(
		BaselineRotation.RotateVector(FVector::YAxisVector * MirrorScaleFactor.Y),
		MirrorScaleFactor,
		ConstraintRotation,
		bLocalConstraint).GetSafeNormal();

	const FQuat NewRotation = FBlendViewTransformPrecision::CleanNearIntegerRotation(
		FRotationMatrix::MakeFromXY(NewXAxis, NewYAxis).ToQuat());

	FTransform NewTransform = BaselineTransform;
	NewTransform.SetLocation(FBlendViewTransformPrecision::CleanNearInteger(NewLocation));
	NewTransform.SetRotation(NewRotation);
	NewTransform.SetScale3D(FBlendViewTransformPrecision::CleanNearInteger(
		BaselineTransform.GetScale3D() * MirrorScaleFactor));
	return NewTransform;
}
