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
#include "Analysis/BodyMechanicsAnalyzer.h"
#include "Analysis/WeaknessDetector.h"
#include "Input/MockCameraPoseSource.h"
#include "UI/SessionResultView.h"
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
class MOTIONBASE_API ASwingTestPawn : public APawn, public ISessionResultView
{
	GENERATED_BODY()

public:
	ASwingTestPawn();

	virtual void Tick(float DeltaSeconds) override;

	/** ISessionResultView — [F] 요청 또는 세션 종료로 결과가 떠 있으면 요약을 채워 HUD 가 그리게 한다. */
	virtual bool GetSessionSummary(FSessionSummary& OutSummary) const override;

	/** 이 세션에서 던지는 총 투구 수. */
	int32 GetTotalPitches() const { return TotalPitches; }

	/** 현재 몇 번째 공인지 (1-based, 표시용). */
	int32 GetPitchNumber() const { return FMath::Clamp(PitchIndex, 1, TotalPitches); }

	/** 목표 구수를 다 채워 세션이 닫혔는지. */
	bool IsSessionOver() const { return bSessionOver; }

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

	/** 신체역학 분석 유효성 게이트 (카메라 포즈 → 지표). */
	UPROPERTY(EditAnywhere, Category = "SwingTest|BodyMechanics")
	FBodyMechanicsConfig BodyMechanicsConfig;

	/**
	 * Mock 포즈 형태 파라미터. 카메라(MediaPipe)가 없어 스윙마다 이 값으로
	 * 합성 포즈를 만들어 UBodyMechanicsAnalyzer 를 실제로 돌린다.
	 * 실제 카메라 수신부가 붙으면 이 경로만 교체한다.
	 */
	UPROPERTY(EditAnywhere, Category = "SwingTest|BodyMechanics")
	FMockSwingPoseParams MockPoseParams;

	/** 신체역학 약점 판별 임계값 (미보정 시 리포트에 전파). */
	UPROPERTY(EditAnywhere, Category = "SwingTest|BodyMechanics")
	FBodyMechanicsScoringConfig BodyMechanicsScoring;

	/** BeginPlay 에서 앞쪽에 생성되는 투수. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "SwingTest")
	TObjectPtr<APitchingZone> PitchingZone;

	/**
	 * 한 세션의 총 투구 수. 이 수를 채우면 투구를 멈추고 결과·추천 화면으로 넘어간다.
	 *
	 * **스윙 수가 아니라 투구 수**로 센다 — 안 치고 흘려보낸 공([Space] 미입력)도 한 구다.
	 * 안 그러면 가만히 있는 플레이어에게서 세션이 끝나지 않는다. (AVRBattingPawn 과 같은 기준.)
	 * 타석(볼카운트)은 이 구수 안에서 돌다가, 마지막 공에서 진행 중이면 그대로 종료된다.
	 */
	UPROPERTY(EditAnywhere, Category = "SwingTest", meta = (ClampMin = "1"))
	int32 TotalPitches = 10;

	void SimulateSwing();
	void ResetSession();

	/** 목표 구수를 다 채웠을 때 세션을 닫는다 (투구 정지 + 결과·추천 표시). */
	void EndSession();

	/**
	 * 현재까지의 스윙들을 한 세션 기록으로 저장 슬롯에 flush 한다 (ModeManager 경유).
	 * 시도가 없으면 아무 일도 하지 않는다. 세션 종료 지점(리셋·모드 복귀·폰 파괴)에서 호출.
	 */
	void FlushSessionToSave();

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

private:
	/** 키 입력 시각을 컨택 시점으로 하는 합성 스윙 궤적. */
	TArray<FSwingSample> BuildSyntheticSwing(double ContactWorldTime, const FVector& BallLocation,
		float ContactSpeedMps, float MissDistanceCm) const;

	FSwingMetrics LastMetrics;
	FBattedBallResult LastHit;
	FScoreResult LastSwingScore;
	FScoreResult SessionScore;
	TArray<FSwingMetrics> SessionHistory;

	/** 최근 스윙의 신체역학 지표 (Mock 포즈 → UBodyMechanicsAnalyzer). */
	FBodyMechanicsMetrics LastBodyMechanics;

	/** 세션 내 스윙별 신체역학 지표 — 세션 피드백에서 약점 판별 입력. */
	TArray<FBodyMechanicsMetrics> SessionBodyHistory;

	// ── 피드백(약점 판별 + 운동 추천 + AI 코칭) ──
	UPROPERTY(Transient)
	TObjectPtr<UAIFeedbackService> FeedbackService;

	/** 현재 세션 지표 → 약점 리포트 (스윙 + 신체역학 축). 피드백·저장 공용. */
	FWeaknessReport BuildSessionReport() const;

	FWeaknessReport LastReport;
	FChronicWeaknessReport LastChronic; // 과거 이력 기반 만성 약점·추세
	TArray<FTrainingDrill> LastDrills;
	FString CoachingText;      // AI 코칭 결과 (없으면 상태 문구)
	bool bShowFeedback = false;
	bool bAwaitingCoaching = false;

	// 현재 투구
	EPitchType CurrentPitchType = EPitchType::Fastball;
	bool bSwungThisPitch = false;
	bool bCurrentPitchIsStrike = false; // 이번 공이 존을 통과하는지

	// ── 세션 진행 ──
	/** 지금까지 던진 공 수 (흘려보낸 공 포함). TotalPitches 에 도달하면 세션 종료. */
	int32 PitchIndex = 0;

	/** 세션 종료됨 — 투구가 멈추고 결과 화면이 계속 떠 있는 상태 ([R] 로 재시작). */
	bool bSessionOver = false;

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

	// 타석(좌타/우타). BeginPlay 에서 읽어 카메라 위치·타구 방향에 반영.
	EBattingStance SessionStance = EBattingStance::Right;

	// 집계
	int32 SwingCount = 0;
	int32 ContactCount = 0;
	int32 MissedPitchCount = 0;
	int32 HomeRunCount = 0;
	int32 HitCount = 0;      // 안타(홈런 제외)
	bool bHasSwung = false;

	// 비거리 집계 (컨택한 타구 기준)
	float SessionMaxCarryM = 0.0f;
	float SessionCarrySumM = 0.0f;
};
