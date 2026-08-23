#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Core/Defense/Throw/ThrowTypes.h"
#include "ThrowPawn.generated.h"

class UCameraComponent;
class UCapsuleComponent;
class ACatchBall;

/**
 * 송구 훈련 폰 (1인칭).
 *
 * 흐름: 타겟(사람) 랜덤 배치 → 스페이스바 홀드로 파워 차오름 → 떼면 그 파워로 발사
 *       → 착지점 판정(FThrowJudge) → 10구 반복 → "N / 10" 집계.
 *
 * 방향은 자동 조준(타겟 쪽). 플레이어는 파워(거리)만 맞춘다.
 * 공은 포구의 ACatchBall 을 재활용해 포물선으로 날린다.
 */
UCLASS()
class MOTIONBASE_API AThrowPawn : public APawn
{
	GENERATED_BODY()

public:
	AThrowPawn();

	virtual void Tick(float DeltaSeconds) override;

	// ── HUD 가 읽는 상태 접근자 ──
	int32 GetTotalThrows() const { return TotalThrows; }
	int32 GetSuccessCount() const { return SuccessCount; }
	int32 GetThrowNumber() const { return FMath::Min(ThrowIndex + 1, TotalThrows); }

	/** 현재 파워 게이지 (0~1). 홀드 중이면 차오르는 값. */
	float GetCurrentPower() const { return CurrentPower; }
	bool  IsCharging() const { return bCharging; }

	/** 마지막 판정 결과 문구/색. 표시할 게 있으면 true. */
	bool GetLastOutcomeText(FString& OutText, FLinearColor& OutColor) const;

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, Category = "Throw")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, Category = "Throw")
	TObjectPtr<UCameraComponent> Camera;

	// ── 설정값 ──

	/** 총 시행 수. */
	UPROPERTY(EditAnywhere, Category = "Throw")
	int32 TotalThrows = 10;

	/** 파워가 0→1 까지 차오르는 시간 (초). */
	UPROPERTY(EditAnywhere, Category = "Throw")
	float ChargeTime = 1.2f;

	/** 성공으로 인정할 착지 반경 (cm). */
	UPROPERTY(EditAnywhere, Category = "Throw")
	float HitRadius = 200.0f;

	/** 타겟이 놓이는 최소/최대 거리 (cm, 정면). */
	UPROPERTY(EditAnywhere, Category = "Throw")
	float MinTargetDistance = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Throw")
	float MaxTargetDistance = 2600.0f;

	/** 타겟이 좌우로 흩어지는 폭 (cm). */
	UPROPERTY(EditAnywhere, Category = "Throw")
	float TargetSideSpread = 500.0f;

	/** 파워 1.0 이 도달시키는 최대 수평 거리 (cm). 거리→파워 역산 기준. */
	UPROPERTY(EditAnywhere, Category = "Throw")
	float MaxThrowRange = 3000.0f;

	/** 한 구 사이 간격 (초). */
	UPROPERTY(EditAnywhere, Category = "Throw")
	float IntervalBetweenThrows = 1.5f;

	/** 공 액터 클래스. 미지정 시 ACatchBall 기본 사용. */
	UPROPERTY(EditAnywhere, Category = "Throw")
	TSubclassOf<ACatchBall> BallClass;

private:
	// ── 입력 핸들러 ──
	void OnChargeStart();      // Space 누름
	void OnChargeRelease();    // Space 뗌
	void ReturnToModeSelect(); // M

	// ── 세션 진행 ──
	void StartSession();
	void SpawnNextTarget();
	void ThrowBall(float Power);
	void FinishThrow(const FThrowResult& Result);
	void EndSession();

	/** 파워(0~1) → 발사 속도 벡터. 방향은 타겟 자동 조준. */
	FVector PowerToVelocity(float Power) const;

	/** 타겟 거리를 정확히 맞히는 정답 파워(0~1). */
	float DistanceToIdealPower(float Distance) const;

	// ── 상태 ──
	UPROPERTY(Transient)
	TObjectPtr<ACatchBall> ActiveBall;

	FThrowTrial CurrentTrial;
	FVector HomeLocation = FVector::ZeroVector;

	int32 ThrowIndex = 0;
	int32 SuccessCount = 0;

	bool  bCharging = false;    // 스페이스바 홀드 중
	float CurrentPower = 0.0f;  // 0~1

	bool  bBallInFlight = false;
	bool  bSessionOver = false;

	bool  bWaitingNext = false;
	float IntervalTimer = 0.0f;

	FThrowResult LastResult;
	bool bHasResult = false;
};