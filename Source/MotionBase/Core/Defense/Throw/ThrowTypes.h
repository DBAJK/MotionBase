#pragma once

#include "CoreMinimal.h"
#include "Data/MotionBaseTypes.h"   // EBaseType (송구·백업 공용 어휘)
#include "ThrowTypes.generated.h"

/**
 * 송구 한 번(1구)의 정보.
 *
 * 한 시행은 **포구 → 전환 → 송구** 한 묶음이다. 스펙의 측정 지표 세 개
 * (정확도 / 구속 / transfer time) 중 transfer time 은 "공을 잡은 순간"이
 * 없으면 정의 자체가 불가능하므로, 목표 베이스와 함께 급구(feed)도 이 구조체에 담는다.
 *
 * 방향은 자동 조준이므로 플레이어는 파워(거리)만 맞춘다.
 */
USTRUCT(BlueprintType)
struct FThrowTrial
{
	GENERATED_BODY()

	/** 이번 시행에 지정된 목표 베이스. 포구 직후 콜(표시)된다. */
	UPROPERTY(BlueprintReadOnly)
	EBaseType TargetBase = EBaseType::First;

	/** 타겟(받는 사람) 위치 (월드) = 목표 베이스 위치. */
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

	/** 성공(목표 zone 도달)으로 인정하는 착지 반경 (cm). */
	UPROPERTY(BlueprintReadOnly)
	float HitRadius = 150.0f;

	/** 급구(feed) 발사 위치 — 플레이어에게 굴려/띄워 주는 공의 출발점. */
	UPROPERTY(BlueprintReadOnly)
	FVector FeedLaunchLocation = FVector::ZeroVector;

	/** 급구 초기 속도 (cm/s). */
	UPROPERTY(BlueprintReadOnly)
	FVector FeedVelocity = FVector::ZeroVector;

	/** 급구가 플레이어 손에 닿는 예상 시간 (초). 포구 판정 창의 기준. */
	UPROPERTY(BlueprintReadOnly)
	float FeedFlightSec = 1.0f;
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
 * 송구 한 번의 판정 결과 — 스펙의 측정 지표 3종을 모두 담는다.
 *   ① 송구 정확도 : Outcome + DistanceError (목표 zone 도달 여부)
 *   ② 구속        : ReleaseSpeedKmh
 *   ③ 전환 시간   : TransferTimeSec (포구 → 릴리스)
 */
USTRUCT(BlueprintType)
struct FThrowResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EThrowOutcome Outcome = EThrowOutcome::Short;

	/** 이번 시행의 목표 베이스 (베이스별 정확도 집계용). */
	UPROPERTY(BlueprintReadOnly)
	EBaseType TargetBase = EBaseType::First;

	/** 착지점 ↔ 타겟 거리 (cm). 작을수록 정확. */
	UPROPERTY(BlueprintReadOnly)
	float DistanceError = 0.0f;

	/** 사용한 파워 (0~1). */
	UPROPERTY(BlueprintReadOnly)
	float UsedPower = 0.0f;

	/** 측정 지표 ②: 릴리스 구속 (km/h). 발사 속도 벡터의 크기에서 환산. */
	UPROPERTY(BlueprintReadOnly)
	float ReleaseSpeedKmh = 0.0f;

	/**
	 * 측정 지표 ③: 포구 → 송구 전환 시간 (초).
	 * 공을 잡은 순간부터 손을 떠나는 순간까지. 실전에서 주자를 잡느냐를 가르는 값이다.
	 * 급구를 못 잡고(fumble) 던졌으면 -1 (미측정).
	 */
	UPROPERTY(BlueprintReadOnly)
	float TransferTimeSec = -1.0f;

	/** 급구를 정상적으로 포구했는지. false 면 전환 시간이 무효다. */
	UPROPERTY(BlueprintReadOnly)
	bool bCleanCatch = false;

	bool IsSuccess() const { return Outcome == EThrowOutcome::Ontarget; }
};
