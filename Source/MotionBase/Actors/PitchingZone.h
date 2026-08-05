#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/MotionBaseTypes.h"
#include "PitchingZone.generated.h"

class UStaticMeshComponent;
class USceneComponent;

/** 투구 시작 — 도달 위치/시각을 알려준다. 타자(ABat)가 이 시각을 타이밍 기준으로 쓴다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPitchThrown, EPitchType, PitchType, FVector, PlateLocation, float, ArrivalWorldTime);

/** 공이 홈플레이트에 도달함 (스윙 여부와 무관). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPitchArrived, FVector, PlateLocation);

/**
 * 투수 / 투구 스폰. (Actor 계층)
 *
 * 액터를 **마운드 위치에 두고 타자 쪽을 향하게(Forward = 타자 방향)** 배치한다.
 *   릴리스 지점 = ActorLocation + (0,0,ReleaseHeightCm)
 *   홈플레이트  = ActorLocation + Forward * ReleaseToPlateCm + (0,0,PlateHeightCm) + 코스 오프셋
 *
 * 궤적은 릴리스→플레이트 보간에 구종별 휨(break)을 얹는다. 휨은 sin(α·π) 형태라
 * 중간에서 최대로 휘고 **도달점은 정확히 PlateLocation** — 즉 브로드캐스트한
 * 도달 위치/시각 계약이 연출과 어긋나지 않는다.
 */
UCLASS()
class MOTIONBASE_API APitchingZone : public AActor
{
	GENERATED_BODY()

public:
	APitchingZone();

	virtual void Tick(float DeltaSeconds) override;

	/** 지정한 구종·구속으로 투구. */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Pitch")
	void ThrowPitch(EPitchType PitchType, float SpeedKmh);

	/** 난이도 설정(구속 범위·변화구 비율·코스)에 따라 무작위 투구. */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Pitch")
	void ThrowRandomPitch();

	/**
	 * 난이도 프리셋을 투구 파라미터에 적용한다 (구속·변화구 비율·간격·코스 분산).
	 * 모드 폰이 BeginPlay 에서 호출한다. 실측 캘리브레이션 대상(하드코딩 확정 금지).
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Pitch")
	void ApplyDifficulty(EDifficultyLevel Level);

	/** 타격 성공 시 공을 날려보내는 연출. */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Pitch")
	void LaunchHitBall(const FVector& Direction, float SpeedMps);

	/** 현재 공이 날아오는 중인지 (스윙 판정 가능 구간). */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Pitch")
	bool IsPitchInFlight() const { return State == EPitchState::Incoming; }

	/** 현재 투구의 도달 예정 시각 (월드 시간, 초). */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Pitch")
	float GetArrivalWorldTime() const { return ArrivalWorldTime; }

	UFUNCTION(BlueprintPure, Category = "MotionBase|Pitch")
	FVector GetPlateLocation() const { return PlateLocation; }

	/** 마운드→플레이트 거리 (cm). 배치 계산용. */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Pitch")
	float GetReleaseToPlateCm() const { return ReleaseToPlateCm; }

	/**
	 * 직전 투구가 스트라이크 존을 통과했는지 (코스 오프셋 기준).
	 * 타자(ASwingTestPawn)가 루킹 스트라이크/볼 판정에 쓴다.
	 */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Pitch")
	bool IsLastPitchStrike() const;

	UFUNCTION(BlueprintPure, Category = "MotionBase|Pitch")
	float GetStrikeZoneHalfWidthCm() const { return StrikeZoneHalfWidthCm; }

	UFUNCTION(BlueprintPure, Category = "MotionBase|Pitch")
	float GetStrikeZoneHalfHeightCm() const { return StrikeZoneHalfHeightCm; }

	UPROPERTY(BlueprintAssignable, Category = "MotionBase|Pitch")
	FOnPitchThrown OnPitchThrown;

	UPROPERTY(BlueprintAssignable, Category = "MotionBase|Pitch")
	FOnPitchArrived OnPitchArrived;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "MotionBase|Pitch")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 날아가는 공. */
	UPROPERTY(VisibleAnywhere, Category = "MotionBase|Pitch")
	TObjectPtr<UStaticMeshComponent> Ball;

	// ── 구장 규격 ──

	/** 마운드 → 홈플레이트 거리 (cm). 규격 18.44m. */
	UPROPERTY(EditAnywhere, Category = "MotionBase|Pitch|Field")
	float ReleaseToPlateCm = 1844.0f;

	/** 릴리스 높이 (cm). */
	UPROPERTY(EditAnywhere, Category = "MotionBase|Pitch|Field")
	float ReleaseHeightCm = 180.0f;

	/** 스트라이크존 중심 높이 (cm). */
	UPROPERTY(EditAnywhere, Category = "MotionBase|Pitch|Field")
	float PlateHeightCm = 100.0f;

	/** 코스 분산 — 좌우 / 상하 반경 (cm). */
	UPROPERTY(EditAnywhere, Category = "MotionBase|Pitch|Field")
	float CourseSpreadLateralCm = 25.0f;

	UPROPERTY(EditAnywhere, Category = "MotionBase|Pitch|Field")
	float CourseSpreadVerticalCm = 20.0f;

	// ── 스트라이크 존 ──
	// TODO(캘리브레이션): 실제 존 규격/체감으로 조정. 코스 분산이 이 값을 넘으면 볼이 된다.

	/** 스트라이크 존 좌우 반폭 (cm). 홈플레이트 폭 ≈ 43cm → 반폭 ≈ 21.6. */
	UPROPERTY(EditAnywhere, Category = "MotionBase|Pitch|Field")
	float StrikeZoneHalfWidthCm = 22.0f;

	/** 스트라이크 존 상하 반높이 (cm). */
	UPROPERTY(EditAnywhere, Category = "MotionBase|Pitch|Field")
	float StrikeZoneHalfHeightCm = 28.0f;

	/** 존을 디버그 박스로 표시할지. */
	UPROPERTY(EditAnywhere, Category = "MotionBase|Pitch|Field")
	bool bDrawStrikeZone = true;

	// ── 난이도 ──

	/** 자동으로 계속 투구할지. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionBase|Pitch|Difficulty")
	bool bAutoPitch = true;

	/** 도달 후 다음 투구까지 간격 (초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionBase|Pitch|Difficulty")
	float AutoPitchIntervalSec = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionBase|Pitch|Difficulty")
	float SpeedMinKmh = 95.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionBase|Pitch|Difficulty")
	float SpeedMaxKmh = 135.0f;

	/** 변화구 비율 0~1. 난이도가 오르면 증가. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionBase|Pitch|Difficulty")
	float BreakingBallRatio = 0.35f;

	/** 변화구 최대 휨 폭 (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionBase|Pitch|Difficulty")
	float BreakAmountCm = 60.0f;

private:
	enum class EPitchState : uint8
	{
		Idle,      // 대기
		Incoming,  // 투구 중 (플레이트로 접근)
		HitFlight  // 타격되어 날아가는 중
	};

	FVector ComputeReleaseLocation() const;

	void EnterIdle();

	EPitchState State = EPitchState::Idle;

	// 현재 투구
	FVector ReleaseLocation = FVector::ZeroVector;
	FVector PlateLocation = FVector::ZeroVector;
	FVector BreakVector = FVector::ZeroVector;
	float TravelDurationSec = 0.0f;
	float FlightTime = 0.0f;
	float ArrivalWorldTime = 0.0f;

	// 직전 투구의 코스 오프셋 (스트라이크/볼 판정용). 스트라이크 존 중심 기준.
	float LastCourseLateralCm = 0.0f;
	float LastCourseVerticalCm = 0.0f;

	/** 스트라이크 존을 디버그 박스로 그린다. */
	void DrawStrikeZone() const;

	// 타구 연출
	FVector HitVelocity = FVector::ZeroVector;
	FVector HitStart = FVector::ZeroVector;

	// 대기 타이머
	float IdleTimer = 0.0f;
};
