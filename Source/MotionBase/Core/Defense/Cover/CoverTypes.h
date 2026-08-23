#pragma once

#include "CoreMinimal.h"
#include "CoverTypes.generated.h"

/** 베이스 종류. */
UENUM(BlueprintType)
enum class EBaseType : uint8
{
	First   UMETA(DisplayName = "1루"),
	Second  UMETA(DisplayName = "2루"),
	Third   UMETA(DisplayName = "3루"),
	Home    UMETA(DisplayName = "홈")
};

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
 * 백업 위치 판단 문제(선택형).
 *
 * 경기 상황(랜덤 타구/송구)에서 "당신의 수비 위치는 어디를 백업/커버해야 하는가"를
 * 4지선다로 고른다. 이동이 아니라 판단 훈련. CoverPawn 이 미리 채운 목록에서 뽑는다.
 */
USTRUCT(BlueprintType)
struct FBackupQuiz
{
	GENERATED_BODY()

	/** 경기 상황 설명. */
	UPROPERTY(BlueprintReadOnly)
	FString Situation;

	/** 당신이 맡은 수비 위치. */
	UPROPERTY(BlueprintReadOnly)
	FString Role;

	/** 선택지 (보통 4개). */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> Options;

	/** 정답 선택지 인덱스. */
	UPROPERTY(BlueprintReadOnly)
	int32 Correct = 0;

	/** 정답 해설. */
	UPROPERTY(BlueprintReadOnly)
	FString Explain;
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

	bool IsSuccess() const { return Outcome == ECoverOutcome::Covered; }
};