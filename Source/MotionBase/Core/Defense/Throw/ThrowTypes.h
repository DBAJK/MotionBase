#pragma once

#include "CoreMinimal.h"
#include "ThrowTypes.generated.h"

/**
 * 송구 한 번(1구)의 정보.
 *
 * ThrowPawn 이 타겟을 랜덤 배치하며 채운다. 방향은 자동 조준이므로
 * 플레이어는 파워(거리)만 맞춘다. "정확한 파워"는 타겟까지 거리로 역산해 둔다.
 */
USTRUCT(BlueprintType)
struct FThrowTrial
{
	GENERATED_BODY()

	/** 타겟(받는 사람) 위치 (월드). */
	UPROPERTY(BlueprintReadOnly)
	FVector TargetLocation = FVector::ZeroVector;

	/** 송구 시작 위치 (플레이어 손 높이). */
	UPROPERTY(BlueprintReadOnly)
	FVector ThrowOrigin = FVector::ZeroVector;

	/** 타겟까지 수평 거리 (cm). 파워 게이지 기준. */
	UPROPERTY(BlueprintReadOnly)
	float TargetDistance = 0.0f;

	/** 이 거리를 정확히 맞히는 "정답 파워" (0~1). 게이지가 여기 오면 명중. */
	UPROPERTY(BlueprintReadOnly)
	float IdealPower = 0.5f;

	/** 성공으로 인정하는 착지 반경 (cm). */
	UPROPERTY(BlueprintReadOnly)
	float HitRadius = 150.0f;
};

/** 송구 판정 결과. */
UENUM(BlueprintType)
enum class EThrowOutcome : uint8
{
	Ontarget  UMETA(DisplayName = "명중"),
	Short     UMETA(DisplayName = "짧음"),   // 파워 부족 — 앞에 떨어짐
	Over      UMETA(DisplayName = "넘김")    // 파워 과함 — 넘어감
};

/**
 * 송구 한 번의 판정 결과.
 * ThrowJudge 가 채운다. 착지점과 타겟의 거리로 명중/짧음/넘김을 가른다.
 */
USTRUCT(BlueprintType)
struct FThrowResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EThrowOutcome Outcome = EThrowOutcome::Short;

	/** 착지점 ↔ 타겟 거리 (cm). 작을수록 정확. */
	UPROPERTY(BlueprintReadOnly)
	float DistanceError = 0.0f;

	/** 사용한 파워 (0~1). */
	UPROPERTY(BlueprintReadOnly)
	float UsedPower = 0.0f;

	bool IsSuccess() const { return Outcome == EThrowOutcome::Ontarget; }
};