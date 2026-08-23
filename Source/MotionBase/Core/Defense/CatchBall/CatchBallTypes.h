#pragma once

#include "CoreMinimal.h"
#include "CatchBallTypes.generated.h"

/** 타구 유형. Mixed 는 매 구 나머지 셋 중 하나를 랜덤으로 고른다. */
UENUM(BlueprintType)
enum class ECatchBallType : uint8
{
	GroundBall    UMETA(DisplayName = "땅볼"),
	FlyBall       UMETA(DisplayName = "뜬공"),
	LineDrive     UMETA(DisplayName = "라인드라이브"),
	Mixed         UMETA(DisplayName = "혼합")
};

/**
 * 한 구(1구)의 발사 정보.
 *
 * CatchBallPawn 이 유형에 맞춰 값을 채워 ACatchBall 에게 넘긴다.
 * 궤적은 "발사 위치 + 초기 속도 + 중력" 으로 결정되며, 낙구지점/도달시간은
 * 판정·마커 표시를 위해 미리 계산해 함께 담는다.
 */
USTRUCT(BlueprintType)
struct FCatchTrial
{
	GENERATED_BODY()

	/** 이번 구의 실제 유형 (Mixed 는 여기서 셋 중 하나로 확정된 값). */
	UPROPERTY(BlueprintReadOnly)
	ECatchBallType ResolvedType = ECatchBallType::GroundBall;

	/** 발사 시작 위치 (월드). */
	UPROPERTY(BlueprintReadOnly)
	FVector LaunchLocation = FVector::ZeroVector;

	/** 초기 속도 벡터 (cm/s). 방향·속력을 함께 담는다. */
	UPROPERTY(BlueprintReadOnly)
	FVector LaunchVelocity = FVector::ZeroVector;

	/** 예측 낙구지점 (월드, 바닥 마커를 여기 그린다). */
	UPROPERTY(BlueprintReadOnly)
	FVector PredictedLanding = FVector::ZeroVector;

	/** 발사~낙구지점 도달까지 걸리는 시간 (초). 타이밍 판정 기준. */
	UPROPERTY(BlueprintReadOnly)
	float TimeToLanding = 0.0f;

	/** 이 구에서 허용하는 캐치 반경 (cm). 유형별 난이도. */
	UPROPERTY(BlueprintReadOnly)
	float CatchRadius = 120.0f;
};

/** 포구 시도 판정 결과. */
UENUM(BlueprintType)
enum class ECatchOutcome : uint8
{
	Success   UMETA(DisplayName = "성공"),
	Miss      UMETA(DisplayName = "헛손질"),   // 범위 밖에서 스페이스바
	Dropped   UMETA(DisplayName = "놓침")      // 스페이스바 없이 공이 지나감
};

/**
 * 한 구 판정 결과.
 *
 * CatchBallJudge 가 채운다. 지금은 위치+타이밍만 보고 판정하며,
 * 글러브 위치(GloveError)는 LiDAR 손 추적 연결 전까지 비워둔다(자리만).
 */
USTRUCT(BlueprintType)
struct FCatchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	ECatchOutcome Outcome = ECatchOutcome::Dropped;

	/** 스페이스바 순간 공↔포구지점 거리 (cm). 작을수록 정확. */
	UPROPERTY(BlueprintReadOnly)
	float DistanceError = 0.0f;

	/** 공 도달 시각 대비 스페이스바 타이밍 오차 (초, 음수=빠름). */
	UPROPERTY(BlueprintReadOnly)
	float TimingError = 0.0f;

	/** 성공 여부 단축 접근자. */
	bool IsSuccess() const { return Outcome == ECatchOutcome::Success; }

	/** [예약] 글러브 위치 오차 (cm). LiDAR 손 추적 연결 시 사용. */
	UPROPERTY(BlueprintReadOnly)
	float GloveError = -1.0f;   // -1 = 미측정
};