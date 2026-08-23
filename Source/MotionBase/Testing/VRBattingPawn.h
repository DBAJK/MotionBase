#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Data/MotionBaseTypes.h"
#include "Data/SwingMetrics.h"
#include "Data/ScoreResult.h"
#include "Data/BattedBall.h"
#include "Scoring/ScoringService.h"
#include "VRBattingPawn.generated.h"

class UCameraComponent;
class USceneComponent;
class ABat;
class APitchingZone;

/**
 * VR 타격 폰 — HMD + Vive 컨트롤러(=배트)로 실제 스윙하는 타격 모드.
 *
 * ASwingTestPawn(키보드·합성 스윙)의 VR 대응. AMotionBaseGameMode 는 **HMD 연결 시**
 * 타격 모드를 이 폰으로, 아니면 ASwingTestPawn 으로 띄운다 (베이스 스테이션 없이도
 * PC 개발이 안 막히도록).
 *
 * 스윙 감지는 트리거가 아니라 **궤적 자동 판정**이다:
 *   투구 발생 → Bat->BeginSwingCapture(도달 위치·시각)
 *   도달 직후(+PostContactDelay) → Bat->EndSwingCaptureAndAnalyze()
 *     → USwingAnalyzer 가 시간창·동작 게이트로 "실제로 휘둘렀는지"를 판정한다.
 * 야구 스윙에 버튼을 누르는 건 부자연스러우므로 트리거 입력은 두지 않는다.
 *
 * ⚠️ 위치 추적(베이스 스테이션)이 없으면 회전만 되고 스윙 측정 불가 — HUD 에 경고 표시.
 */
UCLASS()
class MOTIONBASE_API AVRBattingPawn : public APawn
{
	GENERATED_BODY()

public:
	AVRBattingPawn();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** 트래킹 원점. HMD 카메라·배트가 여기 기준으로 추적된다. */
	UPROPERTY(VisibleAnywhere, Category = "VRBatting")
	TObjectPtr<USceneComponent> VROrigin;

	/** HMD 를 따라가는 카메라 (bLockToHmd 기본 true). */
	UPROPERTY(VisibleAnywhere, Category = "VRBatting")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "VRBatting")
	TObjectPtr<ABat> Bat;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "VRBatting")
	TObjectPtr<APitchingZone> PitchingZone;

	UPROPERTY(EditAnywhere, Category = "VRBatting")
	FScoringConfig ScoringConfig;

	/** 도달 후 이 시간(초) 뒤에 스윙을 분석한다 — 늦은 컨택까지 궤적에 담기게. */
	UPROPERTY(EditAnywhere, Category = "VRBatting")
	float PostContactDelaySec = 0.12f;

	UFUNCTION()
	void HandlePitchThrown(EPitchType PitchType, FVector InPlateLocation, float InArrivalWorldTime);

	UFUNCTION()
	void HandlePitchArrived(FVector InPlateLocation);

	void ReturnToModeSelect();
	void ResetSession();

private:
	void AnalyzeSwingNow();

	// 현재 투구
	FVector CurrentPlate = FVector::ZeroVector;
	float CurrentArrivalWorldTime = 0.0f;
	EPitchType CurrentPitchType = EPitchType::Fastball;
	bool bPitchActive = false;
	bool bAnalyzedThisPitch = true;

	// 세션 (ModeManager 에서 읽음)
	EDifficultyLevel SessionDifficulty = EDifficultyLevel::Amateur;
	EBattingStance SessionStance = EBattingStance::Right;

	// 결과
	FSwingMetrics LastMetrics;
	FBattedBallResult LastHit;
	FScoreResult LastSwingScore;
	FScoreResult SessionScore;
	TArray<FSwingMetrics> SessionHistory;
	FString LastCall;

	int32 SwingCount = 0;
	int32 ContactCount = 0;
	int32 HomeRunCount = 0;
	int32 HitCount = 0;
	int32 MissedPitchCount = 0;
	bool bHasResult = false;
};
