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

	/** 컨택 타이밍 오차 (초). 이상 타이밍 대비 +빠름 / -느림. */
	UPROPERTY(BlueprintReadWrite, Category = "Swing")
	float TimingErrorSeconds = 0.0f;

	/** 스위트스팟 ↔ 공 중심 최소 거리 (cm). 컨택 정확도. */
	UPROPERTY(BlueprintReadWrite, Category = "Swing")
	float ContactDistanceCm = 0.0f;

	/** 스윙 평면각 (도). 어퍼/다운스윙 판별용. */
	UPROPERTY(BlueprintReadWrite, Category = "Swing")
	float SwingPlaneAngleDeg = 0.0f;

	/** 유효 컨택 여부. false면 헛스윙. */
	UPROPERTY(BlueprintReadWrite, Category = "Swing")
	bool bContacted = false;
};
