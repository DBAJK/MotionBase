#include "Core/Defense/Cover/CoverJudge.h"

bool FCoverJudge::IsOnBase(
	const FVector& PlayerLocation,
	const FVector& TargetLocation,
	float CoverRadius)
{
	// 수평 거리만 본다 (높이 차 무시).
	return FVector::Dist2D(PlayerLocation, TargetLocation) <= CoverRadius;
}

FCoverResult FCoverJudge::JudgeArrival(
	const FVector& PlayerLocation,
	const FVector& TargetLocation,
	float CoverRadius,
	float TimeTaken)
{
	FCoverResult Result;
	Result.DistanceToBase = FVector::Dist2D(PlayerLocation, TargetLocation);
	Result.TimeTaken      = TimeTaken;

	// 여기 호출은 "반경 안에 들어온 순간"이므로 성공.
	Result.Outcome = ECoverOutcome::Covered;
	return Result;
}

FCoverResult FCoverJudge::JudgeTimeout(
	const FVector& PlayerLocation,
	const FVector& TargetLocation,
	float CoverRadius,
	float TimeLimit)
{
	FCoverResult Result;
	Result.DistanceToBase = FVector::Dist2D(PlayerLocation, TargetLocation);
	Result.TimeTaken      = TimeLimit;

	// 시간이 다 된 시점에 반경 안이면(경계선 케이스) 성공으로 인정,
	// 아니면 얼마나 벗어났는지에 따라 시간초과/다른베이스.
	if (Result.DistanceToBase <= CoverRadius)
	{
		Result.Outcome = ECoverOutcome::Covered;
	}
	else if (Result.DistanceToBase <= CoverRadius * 3.0f)
	{
		// 근처까진 갔는데 못 붙음 → 시간 초과(조금 늦음).
		Result.Outcome = ECoverOutcome::TooSlow;
	}
	else
	{
		// 한참 떨어짐 → 판단 자체가 틀렸거나 엉뚱한 베이스.
		Result.Outcome = ECoverOutcome::WrongBase;
	}

	return Result;
}