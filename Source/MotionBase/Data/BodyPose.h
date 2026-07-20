#pragma once

#include "CoreMinimal.h"
#include "BodyPose.generated.h"

/**
 * 전신 자세 한 프레임 샘플 (LiDAR / 트래커 공통).
 * 반응속도·수비·베이스러닝·피트니스 모드가 소비한다.
 * 단위: 위치 cm, 시간 초.
 */
USTRUCT(BlueprintType)
struct FBodyPoseSample
{
	GENERATED_BODY()

	/** 세션 시작 기준 경과 시간(초). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Body")
	float TimeSec = 0.0f;

	/** 몸통 중심(허리) 위치 (cm). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Body")
	FVector RootPosition = FVector::ZeroVector;

	/** 기준 자세 대비 높이 변화 (cm). 양수=점프, 음수=숙이기/스쿼트. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Body")
	float HeightDelta = 0.0f;

	/** 좌/우 발 위치 (cm). LiDAR 스펙에 따라 미제공일 수 있음. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Body")
	FVector LeftFoot = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Body")
	FVector RightFoot = FVector::ZeroVector;

	/** 이 샘플에 발 데이터가 유효한지. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Body")
	bool bHasFootData = false;
};

/** 전신 동작 모드 공통 원시 지표 (점수화 이전 단계). */
USTRUCT(BlueprintType)
struct FBodyMotionMetrics
{
	GENERATED_BODY()

	/** 자극 제시 → 동작 시작까지 반응 시간 (ms). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Body")
	float ReactionTimeMs = 0.0f;

	/** 이동 속도 (m/s). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Body")
	float MoveSpeedMps = 0.0f;

	/** 목표 지점까지 남은 거리 오차 (cm). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Body")
	float PositionErrorCm = 0.0f;

	/** 자세 안정성 0~1 (흔들림이 적을수록 1). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Body")
	float PostureStability = 0.0f;

	/** 반복 횟수 (피트니스 모드). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Body")
	int32 RepCount = 0;

	/** 시도 성공 여부 (포구/세이프 등). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Body")
	bool bSucceeded = false;
};
