#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Data/MotionBaseTypes.h"
#include "Data/SwingSample.h"
#include "Data/SwingMetrics.h"
#include "Data/ScoreResult.h"
#include "Scoring/ScoringService.h"
#include "SwingTestPawn.generated.h"

class UCameraComponent;
class USceneComponent;
class APitchingZone;

/**
 * Stage 1 화면 테스트용 타자 폰 (Vive 불필요, PIE 전용).
 *
 * APitchingZone 이 자동으로 공을 던지고, [Space] 로 스윙한다.
 * 타이밍 오차는 **실제 키 입력 시각 − 공의 도달 시각**이므로 진짜 타이밍 게임이다.
 * 결과는 USwingAnalyzer → UScoringService 를 그대로 통과한다.
 *
 * ⚠️ 임시 테스트 코드. Vive 배선 후 ABat 기반으로 교체 예정.
 */
UCLASS()
class MOTIONBASE_API ASwingTestPawn : public APawn
{
	GENERATED_BODY()

public:
	ASwingTestPawn();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, Category = "SwingTest")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "SwingTest")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditAnywhere, Category = "SwingTest")
	FScoringConfig ScoringConfig;

	/** BeginPlay 에서 앞쪽에 생성되는 투수. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "SwingTest")
	TObjectPtr<APitchingZone> PitchingZone;

	void SimulateSwing();
	void ResetSession();

	UFUNCTION()
	void HandlePitchThrown(EPitchType PitchType, FVector InPlateLocation, float InArrivalWorldTime);

	UFUNCTION()
	void HandlePitchArrived(FVector InPlateLocation);

private:
	/** 키 입력 시각을 컨택 시점으로 하는 합성 스윙 궤적. */
	TArray<FSwingSample> BuildSyntheticSwing(double ContactWorldTime, const FVector& BallLocation,
		float ContactSpeedMps, float MissDistanceCm) const;

	FSwingMetrics LastMetrics;
	FScoreResult LastSwingScore;
	FScoreResult SessionScore;
	TArray<FSwingMetrics> SessionHistory;

	// 현재 투구
	EPitchType CurrentPitchType = EPitchType::Fastball;
	bool bSwungThisPitch = false;

	// 집계
	int32 SwingCount = 0;
	int32 ContactCount = 0;
	int32 MissedPitchCount = 0;
	bool bHasSwung = false;
};
