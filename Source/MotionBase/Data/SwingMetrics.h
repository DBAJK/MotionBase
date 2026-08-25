#pragma once

#include "CoreMinimal.h"
#include "SwingMetrics.generated.h"

/**
 * USwingAnalyzer 가 원시 FSwingSample 배열에서 뽑아낸 물리 지표.
 * (계산 계층 → 점수 계층 입력)
 */
USTRUCT(BlueprintType)
struct FSwingMetrics
{
	GENERATED_BODY()

	/** 컨택 순간 배트 헤드 속도 (m/s). peak 아님 — 실제 임팩트 시점 속도. */
	UPROPERTY(BlueprintReadWrite, Category = "Swing")
	float ContactSpeedMps = 0.0f;

	/** 스윙 최고 속도 (m/s). */
	UPROPERTY(BlueprintReadWrite, Category = "Swing")
	float PeakSpeedMps = 0.0f;

	/**
	 * 컨택 타이밍 오차 (초) = **컨택 시각 − 이상 시각**.
	 *   양수(+) = 늦음 (공이 지나간 뒤에 배트가 옴 → 밀어치기·땅볼)
	 *   음수(−) = 빠름 (공이 오기 전에 배트가 지나감 → 당겨치기·뜬공)
	 *
	 * ⚠️ 부호를 반대로 읽으면 UHitModel 의 발사각·좌우각이 통째로 뒤집힌다.
	 *    (USwingAnalyzer 가 채우고, UHitModel·UScoringService·UWeaknessDetector 가 이 부호를 전제한다.)
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Swing")
	float TimingErrorSeconds = 0.0f;

	/** 스위트스팟 ↔ 공 중심 최소 거리 (cm). 컨택 정확도. */
	UPROPERTY(BlueprintReadWrite, Category = "Swing")
	float ContactDistanceCm = 0.0f;

	/** 스윙 평면각 (도). 어퍼/다운스윙 판별용. */
	UPROPERTY(BlueprintReadWrite, Category = "Swing")
	float SwingPlaneAngleDeg = 0.0f;

	/** 유효 컨택 여부. false면 헛스윙(또는 스윙 자체가 없었음 — bSwingDetected 로 구분). */
	UPROPERTY(BlueprintReadWrite, Category = "Swing")
	bool bContacted = false;

	/**
	 * 시간 창 안에서 **실제 스윙 동작이 감지됐는지** (컨택 여부와 무관).
	 *
	 * 왜 필요한가: VR 은 스윙을 버튼이 아니라 궤적으로 자동 판정하므로, bContacted=false 하나로는
	 *   ① 지켜본 공(스윙 안 함)  ② 휘둘렀는데 빗나감(헛스윙)
	 * 을 구분할 수 없다. 둘을 같이 묶으면 헛스윙이 시도 집계에서 빠져 **컨택률이 영원히 100%** 가 된다.
	 *   - true  + bContacted=false → 헛스윙 (시도 1건으로 집계해야 함)
	 *   - false                    → 지켜본 공 (시도 아님)
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Swing")
	bool bSwingDetected = false;
};
