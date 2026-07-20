#pragma once

#include "CoreMinimal.h"
#include "SwingSample.generated.h"

/**
 * 스윙 궤적의 한 시점 표본. (계층 간 계약 — Actor → 계산 계층)
 *
 * ABat 액터가 매 프레임 BatTip(SceneComponent)의 월드 좌표를 링버퍼에 쌓고,
 * USwingAnalyzer 가 이 배열을 미분해 배트 헤드 속도/평면각/컨택 순간을 추출한다.
 * 위치는 UE 기본 단위인 cm. (v_mps = v_cm / 100)
 */
USTRUCT(BlueprintType)
struct FSwingSample
{
	GENERATED_BODY()

	/** 표본 시각 (초). 게임 시작 기준 또는 스윙 시작 기준 상대시간. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Swing")
	double TimeSeconds = 0.0;

	/** 배트 헤드(BatTip) 월드 위치, cm. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Swing")
	FVector TipLocation = FVector::ZeroVector;

	/** 배트 헤드 월드 회전. 스윙 평면각/배트 각도 계산용. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Swing")
	FRotator TipRotation = FRotator::ZeroRotator;

	/**
	 * 선속도 (cm/s). 트래킹 시스템이 속도를 제공할 때만 채워진다.
	 * ⚠️ 비어 있을 수 있음 — 배트 속도의 정본은 USwingAnalyzer 의 위치 미분이다
	 *    (회전 채찍 효과 v_tip = v_hand + ω×r 를 반영하려면 미분이 필요).
	 *    이 필드는 참고·검증용.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Swing")
	FVector Velocity = FVector::ZeroVector;

	FSwingSample() = default;

	FSwingSample(double InTime, const FVector& InLoc, const FRotator& InRot)
		: TimeSeconds(InTime), TipLocation(InLoc), TipRotation(InRot)
	{
	}

	/** Velocity 필드 기준 속력 (m/s). 미제공 시 0 — 정본은 분석기 미분값. */
	FORCEINLINE float GetReportedSpeedMps() const { return static_cast<float>(Velocity.Size()) / 100.0f; }
};
