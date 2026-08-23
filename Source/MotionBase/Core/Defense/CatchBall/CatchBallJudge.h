#pragma once

#include "CoreMinimal.h"
#include "Core/Defense/CatchBall/CatchBallTypes.h"

/**
 * 포구 판정기 (순수 로직).
 *
 * 액터·컴포넌트가 아니라 static 함수 모음이다. 공↔포구지점 거리와
 * 타이밍만 받아 성공/헛손질/놓침을 가른다. UE 렌더·입력에 의존하지 않아
 * 장비 없이 단위 테스트가 가능하다. (타격의 USwingAnalyzer 와 같은 위치)
 *
 * 지금은 위치 + 타이밍만 본다. 글러브 위치(GloveError)는 LiDAR 손 추적
 * 연결 시 이 판정에 한 항을 더해 확장한다.
 */
class MOTIONBASE_API FCatchBallJudge
{
public:
	/**
	 * 스페이스바를 눌렀을 때의 포구 판정.
	 *
	 * @param BallLocation    스페이스바 순간 공의 위치 (월드)
	 * @param CatchLocation   포구 지점 = 플레이어 위치 (월드)
	 * @param CatchRadius     허용 캐치 반경 (cm)
	 * @param ElapsedTime     발사 후 경과 시간 (초)
	 * @param TimeToLanding   공이 포구 지점에 도달하는 예측 시간 (초)
	 * @param TimingTolerance 타이밍 허용 오차 (±초)
	 * @return 판정 결과 (성공/헛손질, 거리·타이밍 오차 포함)
	 */
	static FCatchResult JudgePress(
		const FVector& BallLocation,
		const FVector& CatchLocation,
		float CatchRadius,
		float ElapsedTime,
		float TimeToLanding,
		float TimingTolerance);

	/**
	 * 스페이스바 없이 공이 지나가버린 경우 (놓침).
	 * 한 구가 스페이스바 입력 없이 끝났을 때 Pawn 이 호출한다.
	 */
	static FCatchResult JudgeDropped();
};