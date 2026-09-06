#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Data/MotionBaseTypes.h"
#include "Data/SwingMetrics.h"
#include "Data/ScoreResult.h"
#include "Data/BattedBall.h"
#include "Analysis/SwingAnalyzer.h"
#include "Scoring/ScoringService.h"
#include "Data/TrainingFeedback.h"
#include "UI/SessionResultView.h"
#include "UI/VRExitGesture.h"
#include "UI/VRHaptics.h"
#include "VRBattingPawn.generated.h"

class UCameraComponent;
class USceneComponent;
class UTextRenderComponent;
class UVRInfoPanel;
class ABat;
class APitchingZone;
class UAIFeedbackService;

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
class MOTIONBASE_API AVRBattingPawn : public APawn, public ISessionResultView
{
	GENERATED_BODY()

public:
	AVRBattingPawn();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * ISessionResultView — 트리거로 세션 리포트를 요청한 동안 결과 요약을 채운다.
	 *
	 * 헤드셋 안에는 VrPanel 이 압축본을 그리고, 이 인터페이스는 **데스크톱 미러**(AModeSelectHUD)
	 * 가 전체 결과 패널을 그리는 데 쓴다. 전시 부스에서 운영자가 모니터로 참가자 성적을
	 * 보는 경로라서, VR 폰에도 이게 있어야 한다 (예전엔 PC 폰에만 있었다).
	 */
	virtual bool GetSessionSummary(FSessionSummary& OutSummary) const override;

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

	/** 타격 결과("HIT!"/"HOME RUN!" 등) 를 헤드셋 안에 띄우는 3D 텍스트 (토스트 — 헤드락 허용). */
	UPROPERTY(VisibleAnywhere, Category = "VRBatting")
	TObjectPtr<UTextRenderComponent> ResultText;

	/** 상태·세션 정보를 담는 월드 고정 3D 패널 (기존 화면 디버그 텍스트를 승격). */
	UPROPERTY(VisibleAnywhere, Category = "VRBatting")
	TObjectPtr<UVRInfoPanel> VrPanel;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "VRBatting")
	TObjectPtr<ABat> Bat;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "VRBatting")
	TObjectPtr<APitchingZone> PitchingZone;

	UPROPERTY(EditAnywhere, Category = "VRBatting")
	FScoringConfig ScoringConfig;

	/**
	 * 도달 후 이 시간(초) 뒤에 스윙을 분석한다 — 늦은 컨택까지 궤적에 담기게.
	 *
	 * ⚠️ USwingAnalyzer::ContactTimeWindowSec(±) 보다 짧으면 안 된다. 분석은 "지금까지 쌓인
	 * 궤적"만 보므로, 창의 후반부(도달 +ContactTimeWindowSec 까지)가 되기 전에 분석해버리면
	 * 그 구간에 있었어야 할 늦은 스윙 표본이 아예 버퍼에 없어 TAKE로 오분류된다
	 * (버그 재발 이력 있음 — 짧게 만들지 말 것). 기본값을 그 창에서 직접 유도해 항상 맞물리게 한다.
	 */
	UPROPERTY(EditAnywhere, Category = "VRBatting")
	float PostContactDelaySec = USwingAnalyzer::ContactTimeWindowSec;

	// ── 컨택 지점(공이 도착할 곳) — 플레이어 기준 오프셋 (cm) ──
	// 공이 몸 정중앙으로 날아오지 않게, 앞/옆/위로 옮겨 스윙하기 좋은 위치에 도착시킨다.
	/** 플레이어 앞쪽 거리 (cm). 배트를 앞으로 내밀어 맞히는 지점. */
	UPROPERTY(EditAnywhere, Category = "VRBatting|Plate")
	float ContactForwardCm = 45.0f;

	/** 좌우 오프셋 (cm). 0=정면. 우타는 -, 좌타는 + 로 살짝 밀 수 있음. */
	UPROPERTY(EditAnywhere, Category = "VRBatting|Plate")
	float ContactSideCm = 0.0f;

	// ── 컨택 햅틱 (배트를 쥔 손) ──
	//
	// ⚠️ **컨택했을 때만** 울린다. 헛스윙에 진동을 주면 플레이어는 맞았다고 느끼는데
	//    화면은 MISS 라 판정을 의심하게 된다 — 측정 신뢰를 스스로 깎는 셈이다.
	//
	// ⚠️ 진동은 실제 임팩트 순간이 아니라 **스윙 분석 시점**(도달 + PostContactDelaySec)에
	//    울린다. 컨택 여부를 궤적 분석이 끝나야 알 수 있기 때문이다. 현재 기본값 0.12초 =
	//    120ms 지연으로, 임팩트 큐로는 감지 한계선 근처다. 더 줄이려면 PostContactDelaySec 을
	//    낮춰야 하는데 그러면 늦은 컨택이 궤적에서 잘리므로, 채점을 희생하지 않는 선에서만 조정할 것.

	/** 컨택 시 컨트롤러를 진동시킬지. 끄면 진동 관련 코드가 전부 건너뛴다. */
	UPROPERTY(EditAnywhere, Category = "VRBatting|Haptics")
	bool bContactHaptics = true;

	/** 진동 주파수 0~1. 낮을수록 묵직한 '텅' — 배트 임팩트는 낮은 쪽이 어울린다. */
	UPROPERTY(EditAnywhere, Category = "VRBatting|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HapticFrequency = 0.35f;

	/** 빗맞은 컨택(타구 속도 ≒ 0)의 진동 세기 0~1. */
	UPROPERTY(EditAnywhere, Category = "VRBatting|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HapticMinAmplitude = 0.35f;

	/** 정타(아래 기준 타구 속도 이상)의 진동 세기 0~1. */
	UPROPERTY(EditAnywhere, Category = "VRBatting|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HapticMaxAmplitude = 1.0f;

	/**
	 * 진동이 최대 세기가 되는 타구 속도 (m/s).
	 * 타구 속도는 배트 속도 × 컨택 품질이라, 빗맞으면 자동으로 약하게 울린다 —
	 * 세기가 곧 "얼마나 잘 맞았는가"의 촉각 피드백이 된다.
	 */
	UPROPERTY(EditAnywhere, Category = "VRBatting|Haptics", meta = (ClampMin = "1.0"))
	float HapticFullExitVelocityMps = 35.0f;

	/** 진동 지속 시간 (초). 임팩트는 짧아야 '충격'으로 느껴진다 — 길면 그냥 떨림이다. */
	UPROPERTY(EditAnywhere, Category = "VRBatting|Haptics", meta = (ClampMin = "0.01", ClampMax = "0.5"))
	float HapticDurationSec = 0.09f;

	/** 컨택 높이 (cm). 가슴~허리. */
	UPROPERTY(EditAnywhere, Category = "VRBatting|Plate")
	float ContactHeightCm = 110.0f;

	UFUNCTION()
	void HandlePitchThrown(EPitchType PitchType, FVector InPlateLocation, float InArrivalWorldTime);

	UFUNCTION()
	void HandlePitchArrived(FVector InPlateLocation);

	void ReturnToModeSelect();
	void ResetSession();

	/** OnFeedbackReady 수신 — AI 코칭 문장(성공) 또는 사유(실패)를 패널에 띄운다. */
	UFUNCTION()
	void HandleCoachingReady(bool bSuccess, const FString& Text);

	/** 컨트롤러 트리거(아래 검지 버튼) 이상을 이 값으로 본다. */
	UPROPERTY(EditAnywhere, Category = "VRBatting|AI", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float TriggerPressThreshold = 0.6f;

	/**
	 * 정보 패널을 정면에서 좌우로 비켜 놓는 각도 (도).
	 *
	 * 타격은 **정면에서 공이 날아오므로** 패널이 정면에 있으면 투구를 가린다.
	 * 타자가 선 반대쪽 타석 위로 옮겨, 고개만 돌리면 읽히되 스윙 시야는 비워 둔다.
	 * (우타=+, 좌타=− 방향. 0 이면 정면 — 가림 문제가 돌아온다.)
	 */
	UPROPERTY(EditAnywhere, Category = "VRBatting|VR", meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float PanelSideYawDeg = 42.0f;

private:
	void AnalyzeSwingNow();

	/** 세션 스윙들 → 약점 판별 + 드릴 추천 + AI 코칭 요청 (SwingTestPawn 과 동일 파이프라인). */
	void RequestCoaching();

	/** 이번 세션의 약점 리포트. 코칭 요청과 저장이 같은 값을 쓰도록 한 곳에서 만든다. */
	FWeaknessReport BuildSessionReport() const;

	/**
	 * 진행 중인 세션을 저장 슬롯에 확정한다 ([M] 복귀·[R] 리셋·앱 종료 시).
	 * 이걸 부르지 않으면 다음 모드 진입의 SetActiveMode 가 누적을 지워 **기록이 통째로 사라진다.**
	 * (시도가 0건이면 ModeManager 가 빈 세션으로 스스로 무시하므로 무조건 불러도 안전하다.)
	 */
	void FlushSessionToSave();

	/** 타격 결과를 3D 텍스트로 띄운다 (헤드셋 안에서 보이게). 영문/기호라 폰트 의존 없음. */
	void ShowResultText(const FString& Text, const FLinearColor& Color);

	/** 상태·세션 정보를 3D 패널에 갱신 (기존 AddOnScreenDebugMessage 대체). */
	void RefreshVrPanel();

	/** 결과 텍스트가 남아 있는 시간 (초). */
	float ResultTimer = 0.0f;

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

	/** 실제로 휘두른 횟수 (컨택 + 헛스윙). 지켜본 공은 포함하지 않는다. */
	int32 SwingCount = 0;
	int32 ContactCount = 0;
	int32 HomeRunCount = 0;
	int32 HitCount = 0;

	/** 휘둘렀는데 못 맞힌 횟수. 컨택률 약점의 분모를 만드는 값이라 따로 센다. */
	int32 WhiffCount = 0;

	/** 스윙 자체를 하지 않고 지켜본 공. 시도가 아니므로 세션 통계·난이도에 넣지 않는다. */
	int32 TakeCount = 0;

	// 비거리 집계 (결과 화면용) — 컨택한 타구만.
	float MaxCarryDistanceM = 0.0f;
	float SumCarryDistanceM = 0.0f;

	bool bHasResult = false;

	// ── AI 운동 추천 ──
	UPROPERTY(Transient)
	TObjectPtr<UAIFeedbackService> FeedbackService;

	/** 이번 요청으로 뽑힌 추천 드릴 (패널 표시용). */
	TArray<FTrainingDrill> LastDrills;

	/** 마지막 요청 시점의 약점 리포트·만성 추세 (결과 화면이 그대로 그린다). */
	FWeaknessReport LastReport;
	FChronicWeaknessReport LastChronic;

	/** 마지막 코칭 문장(성공) 또는 사유(실패). */
	FString CoachingText;

	bool bAwaitingCoaching = false;   // 요청 후 응답 대기 중
	bool bTriggerHeldPrev = false;    // 트리거 눌림 에지 검출용
	float CoachingShowTimer = 0.0f;   // 코칭 오버레이를 패널에 띄워두는 잔여 시간(초)

	/** VR '배트 위로 들어 나가기' 제스처 상태 (헤드셋만으로 모드 선택 복귀). */
	FVRExitGesture ExitGesture;

	/** 컨택 진동 상태 (Tick 이 지속시간을 재고 EndPlay 가 끈다). */
	FVRHapticPulse ContactHaptic;

	/** 타구 결과에 맞는 세기로 배트 손을 진동시킨다. 컨택했을 때만 호출할 것. */
	void PlayContactHaptic(float ExitVelocityMps);
};
