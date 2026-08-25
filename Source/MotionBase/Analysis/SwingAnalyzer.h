#pragma once

#include "CoreMinimal.h"
#include "Data/SwingSample.h"
#include "Data/SwingMetrics.h"
#include "SwingAnalyzer.generated.h"

/**
 * 스윙 원시 궤적 → 물리 지표 추출 (계산 계층, UE 렌더/액터 비의존).
 *
 * 입력은 숫자(FSwingSample 배열)뿐, 출력도 숫자(FSwingMetrics)뿐이므로
 * 헤드셋·Vive 없이 단위 테스트 가능. static 순수 함수로 구현해 계산 버그를
 * 연출 버그와 분리해서 잡는다.
 *
 * 참고(측정 방식, CLAUDE §5):
 *  - 컨트롤러 속도 ≠ 배트 속도. 배트 헤드는 회전 채찍 효과로 손보다 빠름 (v_tip = v_hand + ω×r).
 *    → ABat 이 BatTip SceneComponent 월드 좌표를 샘플링하면 회전 효과가 자동 반영됨.
 *  - 컨택 순간 속도(peak 아님)를 사용. 링버퍼 최근 15~20 샘플 권장.
 */
UCLASS()
class MOTIONBASE_API USwingAnalyzer : public UObject
{
	GENERATED_BODY()

public:
	// ── 판정 상수 (헤더에 노출: 다른 계층이 이 값에 맞춰야 하기 때문) ──
	// ⚠️ 실측 캘리브레이션 대상 (CLAUDE §규칙). 히트 판정 완화(2026-08) 이후 값.

	/** 유효 컨택 반경 (cm, 배럴 선분 ↔ 공 최단거리 기준). */
	static constexpr float ContactRadiusCm = 32.0f;

	/** 이 속도 미만은 '스윙 아님' (정지·트래킹 노이즈). m/s. */
	static constexpr float MinSwingSpeedMps = 1.5f;

	/**
	 * 컨택 가능 시간 창 (±초). 공이 플레이트 부근에 있는 동안만 컨택으로 인정.
	 * ⚠️ 두 곳이 이 값에 묶여 있다 — 넓힐 땐 같이 확인할 것:
	 *    · ABat::RingBufferSize — 창보다 짧으면 이른 컨택 표본이 버퍼에서 이미 밀려나 있다
	 *    · UHitModel 의 SprayTimingGainDeg — 창이 넓어지면 파울 밴드도 같이 넓어진다
	 */
	static constexpr float ContactTimeWindowSec = 0.32f;

	/** 배트 끝에서 손 쪽으로 이만큼을 유효 타격면(배럴)으로 본다. cm. */
	static constexpr float BarrelLengthCm = 44.0f;

	/**
	 * 궤적 표본 배열에서 스윙 지표를 계산한다.
	 * @param Samples        시간순 정렬된 BatTip 궤적 (cm, 초).
	 * @param BallLocation   가상 공 위치 (cm). 컨택 거리/타이밍 판정 기준.
	 * @param IdealContactTime 이상적 컨택 시각 (초). 투구 도달 타이밍.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Analysis")
	static FSwingMetrics AnalyzeSwing(
		const TArray<FSwingSample>& Samples,
		const FVector& BallLocation,
		double IdealContactTime);

	/** 두 표본 사이 순간 속도 (m/s). 중앙차분/전진차분 위임용 헬퍼. */
	static float ComputeSpeedMps(const FSwingSample& A, const FSwingSample& B);

	/** 궤적에서 최대 속도 표본 인덱스와 값 (m/s). */
	static float FindPeakSpeed(const TArray<FSwingSample>& Samples, int32& OutIndex);
};
