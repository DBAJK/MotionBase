#pragma once

#include "CoreMinimal.h"

/**
 * "기준선(base) + DynamicLevel(0~1) × Headroom" 동적 난이도 상태 — 계산 계층
 * (UE 액터/렌더 비의존, 순수 상태 머신).
 *
 * APitchingZone(타격)이 원래 쓰던 패턴을 일반화했다. 종목마다 조절하는 파라미터 개수와
 * 방향(구속처럼 올릴수록 어려운 것 / 반경처럼 줄일수록 어려운 것)이 달라서, 이 구조체는
 * "레벨 상태 하나"만 관리하고 그 레벨을 실제 파라미터로 바꾸는 계산(Apply)은 호출부가 한다
 * — 파라미터 목록을 여기 박아두면 종목이 늘 때마다 이 파일을 고쳐야 해서 오히려 결합이 커진다.
 *
 * 사용법 (세 종목 공통 계약):
 *   1) 세션 시작: Seed(과거 평균 성적 0~1) — 잘해온 사람은 더 어렵게 시작.
 *   2) 시도마다: RegisterOutcome(좋은 시도인지, 상승폭, 하강폭) → true 면 값이 바뀐 것이니
 *      호출부가 파라미터를 다시 계산해서 반영한다.
 *   3) 파라미터 계산: Apply(Base, Headroom) 또는 ApplyDown(Base, Headroom, Floor) 을
 *      필요한 만큼(반경/시간/속도 등) 불러 쓴다.
 */
struct FDynamicDifficultyLevel
{
	/** 현재 수준 (0~1). 0=프리셋 그대로, 1=최대 상승. */
	float Level = 0.0f;

	/** 세션 시작 시 과거 성적(0~1)으로 시작 난이도를 잡는다. */
	void Seed(float Level01)
	{
		Level = FMath::Clamp(Level01, 0.0f, 1.0f);
	}

	/**
	 * 시도 1건의 결과를 반영한다.
	 * @return 실제로 레벨이 바뀌었는지 — true 면 호출부가 Apply* 로 파라미터를 다시 계산해야 한다.
	 */
	bool RegisterOutcome(bool bGood, float StepUp, float StepDown)
	{
		const float Prev = Level;
		Level = FMath::Clamp(Level + (bGood ? StepUp : -StepDown), 0.0f, 1.0f);
		return !FMath::IsNearlyEqual(Prev, Level);
	}

	/** 올릴수록 어려워지는 파라미터(구속·변화구 비율 등). Base 에 Headroom 을 더한다. */
	float ApplyUp(float Base, float Headroom) const
	{
		return Base + Level * Headroom;
	}

	/** 내릴수록 어려워지는 파라미터(반경·체공시간·제한시간 등). Base 에서 Headroom 을 빼되 Floor 아래로는 안 내려간다. */
	float ApplyDown(float Base, float Headroom, float Floor) const
	{
		return FMath::Max(Base - Level * Headroom, Floor);
	}
};
