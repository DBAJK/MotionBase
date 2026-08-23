#pragma once

#include "CoreMinimal.h"
#include "Core/Defense/Throw/ThrowTypes.h"

/**
 * 송구 판정기 (순수 로직).
 *
 * 던진 공의 착지점과 타겟을 비교해 명중/짧음/넘김을 가른다.
 * UE 렌더·입력에 의존하지 않아 장비 없이 테스트 가능. (포구의 FCatchBallJudge 와 같은 위치)
 *
 * 방향은 자동 조준이므로 여기선 "거리(파워)"만 본다.
 */
class MOTIONBASE_API FThrowJudge
{
public:
	/**
	 * 착지점 기준 송구 판정.
	 *
	 * @param LandingLocation 던진 공이 떨어진 위치 (월드)
	 * @param TargetLocation  타겟 위치 (월드)
	 * @param ThrowOrigin     송구 시작 위치 (짧음/넘김 방향 판단용)
	 * @param HitRadius       명중으로 인정할 반경 (cm)
	 * @param UsedPower       사용한 파워 (0~1, 결과에 기록)
	 * @return 판정 결과
	 */
	static FThrowResult Judge(
		const FVector& LandingLocation,
		const FVector& TargetLocation,
		const FVector& ThrowOrigin,
		float HitRadius,
		float UsedPower);
};