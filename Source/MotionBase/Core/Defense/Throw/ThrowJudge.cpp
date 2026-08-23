#include "Core/Defense/Throw/ThrowJudge.h"

FThrowResult FThrowJudge::Judge(
	const FVector& LandingLocation,
	const FVector& TargetLocation,
	const FVector& ThrowOrigin,
	float HitRadius,
	float UsedPower)
{
	FThrowResult Result;
	Result.UsedPower = UsedPower;

	// 수평 거리만 본다 (높이 차는 무시 — 바닥 착지 기준).
	const FVector LandFlat(LandingLocation.X, LandingLocation.Y, 0.0f);
	const FVector TargetFlat(TargetLocation.X, TargetLocation.Y, 0.0f);
	Result.DistanceError = FVector::Dist(LandFlat, TargetFlat);

	// 반경 안이면 명중.
	if (Result.DistanceError <= HitRadius)
	{
		Result.Outcome = EThrowOutcome::Ontarget;
		return Result;
	}

	// 빗나갔으면 짧음/넘김 구분: 송구 원점에서 타겟까지 방향을 기준으로,
	// 착지점이 타겟보다 앞(못 미침)이면 짧음, 뒤(지나침)면 넘김.
	const FVector OriginFlat(ThrowOrigin.X, ThrowOrigin.Y, 0.0f);
	const FVector ThrowDir = (TargetFlat - OriginFlat).GetSafeNormal();

	// 원점 기준, 타겟까지 거리 vs 착지점까지 거리(던진 방향으로 투영).
	const float TargetProj = FVector::DotProduct(TargetFlat - OriginFlat, ThrowDir);
	const float LandProj   = FVector::DotProduct(LandFlat - OriginFlat, ThrowDir);

	Result.Outcome = (LandProj < TargetProj) ? EThrowOutcome::Short : EThrowOutcome::Over;
	return Result;
}