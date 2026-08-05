#pragma once

#include "CoreMinimal.h"
#include "BattedBall.generated.h"

/**
 * 가상 타구 판정. (계산 계층 UHitModel → 연출·점수·UI)
 * ⚠️ 이름 충돌 주의: UE 물리의 FHitResult 와 다르다. 여기선 "친 공의 결과"다.
 */
UENUM(BlueprintType)
enum class EHitClass : uint8
{
	Whiff    UMETA(DisplayName = "헛스윙"),
	Foul     UMETA(DisplayName = "파울"),
	Out      UMETA(DisplayName = "아웃"),    // 약한 땅볼/뜬공
	Hit      UMETA(DisplayName = "안타"),
	HomeRun  UMETA(DisplayName = "홈런")
};

/**
 * 스윙 지표(FSwingMetrics)에서 추정한 타구 결과.
 * 모두 결정론적으로 계산된다(무작위 없음) — 단위 테스트 가능.
 */
USTRUCT(BlueprintType)
struct FBattedBallResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	EHitClass Class = EHitClass::Whiff;

	/** 타구 속도 (m/s). 컨택 속도 × 반발 × 컨택 품질. */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	float ExitVelocityMps = 0.0f;

	/** 발사각 (도). 양수=뜬공, 0 근처=라인드라이브, 음수=땅볼. */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	float LaunchAngleDeg = 0.0f;

	/** 좌우 방향각 (도). +우측 / -좌측. |각| > 파울라인 이면 파울. */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	float SprayAngleDeg = 0.0f;

	/** 추정 비거리 (m). */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	float CarryDistanceM = 0.0f;

	/** 페어 여부 (파울/헛스윙이면 false). */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	bool bFair = false;
};
