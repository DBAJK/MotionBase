#include "Core/Defense/CatchBall/CatchBallJudge.h"

FCatchResult FCatchBallJudge::JudgePress(
	const FVector& BallLocation,
	const FVector& CatchLocation,
	float CatchRadius,
	float ElapsedTime,
	float TimeToLanding,
	float TimingTolerance)
{
	FCatchResult Result;

	// 위치 오차: 공과 포구 지점 사이 거리.
	Result.DistanceError = FVector::Dist(BallLocation, CatchLocation);

	// 타이밍 오차: 공 도달 시각 대비 스페이스바 시각 (음수 = 너무 빠름).
	Result.TimingError = ElapsedTime - TimeToLanding;

	// 위치·타이밍 둘 다 허용 범위 안이어야 성공.
	const bool bPositionOK = (Result.DistanceError <= CatchRadius);
	const bool bTimingOK   = (FMath::Abs(Result.TimingError) <= TimingTolerance);

	Result.Outcome = (bPositionOK && bTimingOK)
		? ECatchOutcome::Success
		: ECatchOutcome::Miss;   // 범위 밖이거나 타이밍 어긋나면 헛손질

	return Result;
}

FCatchResult FCatchBallJudge::JudgeDropped()
{
	FCatchResult Result;
	Result.Outcome = ECatchOutcome::Dropped;   // 시도 자체가 없었음
	// DistanceError / TimingError 는 기본값(0) — 시도 안 함 표시.
	return Result;
}