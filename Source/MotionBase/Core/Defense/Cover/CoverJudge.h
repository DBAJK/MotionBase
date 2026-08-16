#pragma once

#include "CoreMinimal.h"
#include "Core/Defense/Cover/CoverTypes.h"

/**
 * 커버 판정기 (순수 로직).
 *
 * 플레이어 위치·경과 시간을 받아 정답 베이스 커버 성공 여부를 가른다.
 * UE 렌더·입력에 의존하지 않아 장비 없이 테스트 가능. (포구 FCatchBallJudge 와 같은 위치)
 */
class MOTIONBASE_API FCoverJudge
{
public:
	/**
	 * 플레이어가 정답 베이스 반경 안에 있는지 (매 프레임 성공 확인용).
	 *
	 * @param PlayerLocation 플레이어 현재 위치 (월드)
	 * @param TargetLocation 정답 베이스 위치 (월드)
	 * @param CoverRadius    성공 반경 (cm)
	 * @return 반경 안이면 true
	 */
	static bool IsOnBase(
		const FVector& PlayerLocation,
		const FVector& TargetLocation,
		float CoverRadius);

	/**
	 * 도착 시점의 커버 성공 판정.
	 *
	 * @param PlayerLocation 플레이어 위치 (월드)
	 * @param TargetLocation 정답 베이스 위치 (월드)
	 * @param CoverRadius    성공 반경 (cm)
	 * @param TimeTaken      경과 시간 (초)
	 * @return 성공 결과 (거리·시간 포함)
	 */
	static FCoverResult JudgeArrival(
		const FVector& PlayerLocation,
		const FVector& TargetLocation,
		float CoverRadius,
		float TimeTaken);

	/**
	 * 제한 시간이 다 됐을 때의 판정 (도착 못 함).
	 *
	 * @param PlayerLocation 플레이어 위치 (월드)
	 * @param TargetLocation 정답 베이스 위치 (월드)
	 * @param CoverRadius    성공 반경 (cm)
	 * @param TimeLimit      제한 시간 (초, 기록용)
	 * @return 시간 초과/다른 베이스 결과
	 */
	static FCoverResult JudgeTimeout(
		const FVector& PlayerLocation,
		const FVector& TargetLocation,
		float CoverRadius,
		float TimeLimit);
};