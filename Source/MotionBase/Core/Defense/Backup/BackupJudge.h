#pragma once

#include "CoreMinimal.h"
#include "Core/Defense/Backup/BackupTypes.h"

/**
 * 백업 위치 판단 판정기 (순수 로직, UE 액터/렌더 비의존).
 *
 * 1단계(판단/방향)와 2단계(실행/도착)를 각각 순수 함수로 쪼갠다 — 헤드셋 없이
 * 합성 좌표만으로 단위 테스트가 가능하다 (FCoverBallJudge/FThrowJudge 와 같은 위치).
 * 폰(BackupPawn)이 매 틱 이 함수들을 불러 상태를 조립한다.
 */
class MOTIONBASE_API FBackupJudge
{
public:
	/**
	 * 이동 개시(1단계) 커밋 조건 — 누적 변위 또는 게이트 경과 시간 중 먼저 찬 쪽.
	 *
	 * ⚠️ 첫 프레임 방향을 쓰면 안 된다 — 트랙패드/스틱 최초 접촉 노이즈로 반대 방향처럼
	 *    보일 수 있다. 그래서 "이미 좀 움직인 뒤"의 방향을 본다.
	 *
	 * @param DisplacedCm    큐 이후 누적 이동 거리 (cm).
	 * @param GateElapsedSec 큐 이후 게이트(이동 버튼)가 눌려 있던 누적 시간 (초).
	 */
	static bool ShouldCommitHeading(float DisplacedCm, float GateElapsedSec,
		float CommitDistanceCm, float CommitTimeSec)
	{
		return (DisplacedCm >= CommitDistanceCm) || (GateElapsedSec >= CommitTimeSec);
	}

	/**
	 * 후보 방향 중 Heading 에 가장 가까운 것의 인덱스와, 2위와의 각도差(margin, deg)를 낸다.
	 * 후보가 1개뿐이면 마진은 무한대로 취급(항상 확정).
	 *
	 * @param HeadingDir     확정된 이동 방향(단위 벡터, XY).
	 * @param CandidateDirs  후보들의 방향(단위 벡터, XY) — 큐 지점 기준.
	 * @param OutMarginDeg   1위와 2위의 각도差(도). 작을수록 애매함.
	 */
	static int32 NearestCandidate(const FVector2D& HeadingDir, const TArray<FVector2D>& CandidateDirs,
		float& OutMarginDeg)
	{
		OutMarginDeg = 0.0f;
		if (CandidateDirs.Num() == 0)
		{
			return INDEX_NONE;
		}

		// 코사인 유사도 내림차순(=각도 오름차순)으로 1·2위를 찾는다.
		float BestCos = -2.0f, SecondCos = -2.0f;
		int32 BestIdx = INDEX_NONE;

		for (int32 i = 0; i < CandidateDirs.Num(); ++i)
		{
			const float C = FVector2D::DotProduct(HeadingDir, CandidateDirs[i].GetSafeNormal());
			if (C > BestCos)
			{
				SecondCos = BestCos;
				BestCos = C;
				BestIdx = i;
			}
			else if (C > SecondCos)
			{
				SecondCos = C;
			}
		}

		if (CandidateDirs.Num() == 1)
		{
			OutMarginDeg = 180.0f; // 비교 대상이 없으니 항상 확정.
		}
		else
		{
			const float BestDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(BestCos, -1.0f, 1.0f)));
			const float SecondDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(SecondCos, -1.0f, 1.0f)));
			OutMarginDeg = SecondDeg - BestDeg; // 항상 >= 0 (Best 가 가장 작은 각도이므로).
		}

		return BestIdx;
	}

	/** 도착(2단계) 판정 — 시간 안에 존 안이면 성공, 시간 넘겼는데 방향은 맞았으면 시간초과. */
	static EBackupOutcome JudgeArrival(bool bInZone, bool bDirectionWasCorrect, float ElapsedSec, float TimeLimitSec)
	{
		if (bInZone)
		{
			return EBackupOutcome::Covered;
		}
		if (ElapsedSec < TimeLimitSec)
		{
			return EBackupOutcome::NoStart; // 아직 판정할 시점이 아님 (호출부에서 시간 안이면 안 부른다).
		}
		return bDirectionWasCorrect ? EBackupOutcome::TooSlow : EBackupOutcome::WrongZone;
	}

	/** 경로 효율 0~1 (1=완벽한 직선). 이동이 없으면 -1. */
	static float PathEfficiency(float StraightLineDistanceCm, float AccumulatedPathLengthCm)
	{
		if (AccumulatedPathLengthCm <= KINDA_SMALL_NUMBER)
		{
			return -1.0f;
		}
		return FMath::Clamp(StraightLineDistanceCm / AccumulatedPathLengthCm, 0.0f, 1.0f);
	}
};
