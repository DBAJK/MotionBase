#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Data/MotionBaseTypes.h"
#include "Data/SwingSample.h"
#include "Data/SwingMetrics.h"
#include "Data/ScoreResult.h"
#include "Data/TrainingFeedback.h"
#include "Data/BattedBall.h"
#include "Scoring/ScoringService.h"
#include "SwingTestPawn.generated.h"

class UCameraComponent;
class USceneComponent;
class APitchingZone;
class UAIFeedbackService;

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	/** 시작 화면(모드 선택)으로 복귀. */
	void ReturnToModeSelect();

	/** [F] 세션 채점 → 약점 판별 → 드릴 추천 → (키 있으면) AI 코칭 요청. */
	void RequestFeedback();

	UFUNCTION()
	void HandlePitchThrown(EPitchType PitchType, FVector InPlateLocation, float InArrivalWorldTime);

	UFUNCTION()
	void HandlePitchArrived(FVector InPlateLocation);

	/** AI 코칭 응답 수신. */
	UFUNCTION()
	void HandleCoachingReady(bool bSuccess, const FString& Text);

	/** 세션 종료 후 결과 화면(약점·드릴·코칭)을 그린다. */
	void DrawFeedback() const;

private:
	/** 키 입력 시각을 컨택 시점으로 하는 합성 스윙 궤적. */
	TArray<FSwingSample> BuildSyntheticSwing(double ContactWorldTime, const FVector& BallLocation,
		float ContactSpeedMps, float MissDistanceCm) const;

	FSwingMetrics LastMetrics;
	FBattedBallResult LastHit;
	FScoreResult LastSwingScore;
	FScoreResult SessionScore;
	TArray<FSwingMetrics> SessionHistory;

	// ── 피드백(약점 판별 + 운동 추천 + AI 코칭) ──
	UPROPERTY(Transient)
	TObjectPtr<UAIFeedbackService> FeedbackService;

	FWeaknessReport LastReport;
	TArray<FTrainingDrill> LastDrills;
	FString CoachingText;      // AI 코칭 결과 (없으면 상태 문구)
	bool bShowFeedback = false;
	bool bAwaitingCoaching = false;

	// 현재 투구
	EPitchType CurrentPitchType = EPitchType::Fastball;
	bool bSwungThisPitch = false;
	bool bCurrentPitchIsStrike = false; // 이번 공이 존을 통과하는지

	// ── 볼카운트 / 타석 판정 ──
	int32 Balls = 0;
	int32 Strikes = 0;
	int32 StrikeoutCount = 0; // 세션 삼진
	int32 WalkCount = 0;      // 세션 볼넷
	FString LastPitchCall;    // 화면 표시용: "스트라이크"/"볼"/"파울"/"삼진!"/"볼넷!"/판정 등

	/** 스트라이크 하나 추가. 파울은 2스트라이크 이후엔 카운트되지 않는다. */
	void AddStrike(bool bFromFoul);

	/** 타석 종료 — 볼카운트 초기화하고 사유를 표시. */
	void EndAtBat(const FString& Reason);

	// 난이도 (ModeManager 에서 읽어 BeginPlay 에서 투구에 반영)
	EDifficultyLevel SessionDifficulty = EDifficultyLevel::Amateur;

	// 집계
	int32 SwingCount = 0;
	int32 ContactCount = 0;
	int32 MissedPitchCount = 0;
	int32 HomeRunCount = 0;
	int32 HitCount = 0;      // 안타(홈런 제외)
	bool bHasSwung = false;
};
