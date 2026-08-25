#pragma once

#include "CoreMinimal.h"
#include "Data/MotionBaseTypes.h"   // EBaseType (송구·백업 공용 어휘)
#include "CoverTypes.generated.h"

/**
 * 커버 한 번(1회)의 정보.
 *
 * CoverPawn 이 정답 베이스를 랜덤으로 골라 채운다.
 * 플레이어는 제한 시간 안에 그 베이스로 이동하면 된다.
 */
USTRUCT(BlueprintType)
struct FCoverTrial
{
	GENERATED_BODY()

	/** 이번에 커버해야 할 정답 베이스. */
	UPROPERTY(BlueprintReadOnly)
	EBaseType TargetBase = EBaseType::First;

	/** 정답 베이스의 위치 (월드). */
	UPROPERTY(BlueprintReadOnly)
	FVector TargetLocation = FVector::ZeroVector;

	/** 제한 시간 (초). 이 안에 도착해야 성공. */
	UPROPERTY(BlueprintReadOnly)
	float TimeLimit = 3.0f;

	/** 성공으로 인정할 베이스 반경 (cm). */
	UPROPERTY(BlueprintReadOnly)
	float CoverRadius = 150.0f;
};

/**
 * 백업 위치 판단 한 케이스(선택형).
 *
 * 스펙의 1 케이스 = **`플레이어 포지션` + `타구 방향·종류` + `주자 상황` → 정답 백업 zone**.
 * 세 요소를 각각 필드로 들고 있어야 "무엇 때문에 틀렸는지"를 나중에 집계할 수 있다
 * (예: 주자 3루 상황만 정답률이 낮다). 그래서 상황 문자열 하나로 뭉치지 않는다.
 *
 * 이동이 아니라 판단 훈련이므로 정답은 좌표가 아니라 **백업 zone 이름**이다.
 * (그래서 보기가 항상 1B/2B/3B/Home 이 아니라 케이스마다 다르다 — "홈 뒤", "3루 뒤",
 *  "1루 커버", "중계(cutoff)" 처럼 실제 백업 행동이 보기로 나온다.)
 */
USTRUCT(BlueprintType)
struct FBackupQuiz
{
	GENERATED_BODY()

	/** 타구 방향·종류 (예: "우익수 앞 안타", "3루 방면 번트"). */
	UPROPERTY(BlueprintReadOnly)
	FString Situation;

	/** 주자 상황 (예: "주자 1루", "주자 없음", "주자 2루 - 홈 노림"). */
	UPROPERTY(BlueprintReadOnly)
	FString Runners;

	/** 당신이 맡은 수비 포지션 (P/C/1B/2B/SS/3B/LF/CF/RF). */
	UPROPERTY(BlueprintReadOnly)
	FString Role;

	/** 선택지 (보통 4개) — 백업 zone / 커버 행동. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> Options;

	/** 정답 선택지 인덱스. */
	UPROPERTY(BlueprintReadOnly)
	int32 Correct = 0;

	/** 정답 해설. */
	UPROPERTY(BlueprintReadOnly)
	FString Explain;

	/**
	 * 난이도 핵심 시나리오인지 (투수 홈/3루 택1, 중견수 광범위 백업, 1루수 이탈 시 1루 커버).
	 * 집계에서 "핵심 3종만의 정답률"을 따로 뽑아 코칭에 쓴다.
	 */
	UPROPERTY(BlueprintReadOnly)
	bool bKeyScenario = false;
};

/** 커버 판정 결과. */
UENUM(BlueprintType)
enum class ECoverOutcome : uint8
{
	Covered   UMETA(DisplayName = "커버 성공"),
	TooSlow   UMETA(DisplayName = "시간 초과"),   // 제한 시간 안에 못 감
	WrongBase UMETA(DisplayName = "다른 베이스")  // 시간은 됐는데 엉뚱한 데 있음
};

/**
 * 커버 한 번의 판정 결과.
 * CoverJudge 가 채운다.
 */
USTRUCT(BlueprintType)
struct FCoverResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	ECoverOutcome Outcome = ECoverOutcome::TooSlow;

	/** 정답 베이스까지 남은 거리 (cm). 성공이면 반경 이내. */
	UPROPERTY(BlueprintReadOnly)
	float DistanceToBase = 0.0f;

	/** 도착까지 걸린 시간 (초). */
	UPROPERTY(BlueprintReadOnly)
	float TimeTaken = 0.0f;

	/**
	 * 측정 지표 ②: **판단(이동 시작)까지 걸린 시간** (초).
	 * 선택형에서는 "상황 제시 → 보기 확정"까지의 시간이 곧 판단 시간이다.
	 * (VR 드웰 선택은 드웰 시간이 고정 지연으로 섞이므로, 폰이 드웰분을 빼고 채운다.)
	 */
	UPROPERTY(BlueprintReadOnly)
	float DecisionTimeSec = 0.0f;

	/** 이 케이스가 난이도 핵심 시나리오였는지 (집계 분리용). */
	UPROPERTY(BlueprintReadOnly)
	bool bKeyScenario = false;

	bool IsSuccess() const { return Outcome == ECoverOutcome::Covered; }
};