#pragma once

#include "CoreMinimal.h"
#include "Core/Defense/Throw/ThrowTypes.h"

/**
 * 송구 판정기 (순수 로직).
 *
 * 던진 공의 착지점과 목표 베이스를 비교해 명중/짧음/넘김을 가르고,
 * 구속·전환 시간을 결과에 함께 기록한다.
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
	 * @param TargetLocation  목표 베이스 위치 (월드)
	 * @param ThrowOrigin     송구 시작 위치 (짧음/넘김 방향 판단용)
	 * @param HitRadius       목표 zone 반경 (cm) — 이 안이면 도달로 본다
	 * @param UsedPower       사용한 파워 (0~1, 결과에 기록)
	 * @return 판정 결과 (정확도만 채워진다 — 구속·전환은 아래 FillMotionMetrics 로)
	 */
	static FThrowResult Judge(
		const FVector& LandingLocation,
		const FVector& TargetLocation,
		const FVector& ThrowOrigin,
		float HitRadius,
		float UsedPower);

	/**
	 * 정확도 외 측정 지표(구속·전환 시간·목표 베이스)를 결과에 채운다.
	 *
	 * 정확도 판정과 분리한 이유: 정확도는 착지 시점에야 알 수 있고, 구속·전환 시간은
	 * 릴리스 시점에 이미 확정된다. 두 시점을 한 함수에 묶으면 폰이 릴리스 값을 임시
	 * 변수로 들고 다녀야 하므로, 값이 정해진 쪽에서 각각 채운다.
	 *
	 * @param InOut            정확도가 채워진 결과 (여기에 덧씌운다)
	 * @param TargetBase       이번 시행의 목표 베이스
	 * @param ReleaseSpeedCms  릴리스 속도 (cm/s) — km/h 환산은 여기서 한다
	 * @param TransferTimeSec  포구→릴리스 시간 (초). 미측정이면 음수를 넘긴다
	 * @param bCleanCatch      급구를 정상 포구했는지
	 */
	static void FillMotionMetrics(
		FThrowResult& InOut,
		EBaseType TargetBase,
		float ReleaseSpeedCms,
		float TransferTimeSec,
		bool bCleanCatch);
};
