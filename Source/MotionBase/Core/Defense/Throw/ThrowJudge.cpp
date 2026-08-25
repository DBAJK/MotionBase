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

void FThrowJudge::FillMotionMetrics(
	FThrowResult& InOut,
	EBaseType TargetBase,
	float ReleaseSpeedCms,
	float TransferTimeSec,
	bool bCleanCatch)
{
	InOut.TargetBase = TargetBase;

	// cm/s → km/h : 1 cm/s = 0.036 km/h.
	InOut.ReleaseSpeedKmh = FMath::Max(ReleaseSpeedCms, 0.0f) * 0.036f;

	// 급구를 못 잡았으면 "잡은 순간"이 없으므로 전환 시간은 미측정(-1)로 남긴다.
	// 0 을 넣으면 평균이 낙관적으로 오염된다.
	InOut.bCleanCatch = bCleanCatch;
	InOut.TransferTimeSec = (bCleanCatch && TransferTimeSec >= 0.0f) ? TransferTimeSec : -1.0f;
}