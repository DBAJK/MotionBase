#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Core/Defense/Backup/BackupTypes.h"
#include "Core/Defense/Backup/BaseballField.h"
#include "Data/BodyPose.h"
#include "Data/TrainingFeedback.h"
#include "UI/VRExitGesture.h"
#include "UI/VREndCardMenu.h"
#include "BackupPawn.generated.h"

class UCameraComponent;
class UCapsuleComponent;
class UMotionControllerComponent;
class UVRInfoPanel;
class UAIFeedbackService;
class ACatchBall;
class AFielderMarker;

/** 한 시행의 진행 단계. */
UENUM(BlueprintType)
enum class EBackupPhase : uint8
{
	Live  UMETA(DisplayName = "진행 중"),   // 큐 표시됨 — 플레이어가 판단·이동할 수 있음
	Done  UMETA(DisplayName = "결과")       // 판정 끝 — 다음 시행 대기
};

/**
 * 백업 위치 판단 훈련 폰 — **실제로 이동하는 판단 훈련**.
 *
 * 흐름: 포지션 지정(메뉴에서 확정, EFieldPosition) → 큐(타구 방향+주자 상황) 제시 →
 *       플레이어가 정답 백업 zone 으로 실제 이동 → 2단계 판정:
 *         ① 판단(이동 개시) — 방향 + 개시까지 걸린 시간
 *         ② 실행(도착)     — 제한 시간 안에 정답 zone 도착
 *       → N시행 반복.
 *
 * ⚠️ 기존 4지선다 퀴즈(ACoverPawn)를 대체한다. 스펙의 "올바른 백업 zone으로 이동했는지
 *    판정" / "판단(이동 시작)까지 걸린 시간"은 퀴즈로는 애초에 측정할 수 없었다.
 *
 * ⚠️ **실측 다이아몬드를 그대로 쓴다** — CatchBallPawn/ThrowPawn 처럼 플레이어 HMD 방향
 *    주위에 합성 필드를 세우지 않는다. 다이아몬드가 세계 좌표에 고정돼 있어야
 *    "실제로 그 방향으로 뛰었는가"가 의미를 가지기 때문. 대신 폰이 매 시행 플레이어를
 *    자기 수비 위치(Field.GetFieldingSpot)로 되돌려 놓는다 (SnapToFieldingSpot).
 *
 * ⚠️ **이동은 게이트(누르고 있는 동안만)** — VR 은 트리거를 쥔 동안만 스틱이 먹는다.
 *    가볍게 스친 터치로 미끄러지지 않게 하기 위한 명시적 요구사항. PC 는 WASD 자체가
 *    이미 "누르는 동안만" 이라 별도 게이트가 필요 없다.
 */
UCLASS()
class MOTIONBASE_API ABackupPawn : public APawn
{
	GENERATED_BODY()

public:
	ABackupPawn();

	virtual void Tick(float DeltaSeconds) override;

	/** 폰 스폰 직후(모드 진입 시) 세션 시작 전에 포지션을 지정한다. */
	void SetFieldPosition(EFieldPosition InPosition) { Position = InPosition; }

	// ── HUD 가 읽는 상태 접근자 ──
	int32 GetTotalTrials() const { return TotalTrials; }
	int32 GetSuccessCount() const { return SuccessCount; }
	int32 GetTrialNumber() const { return FMath::Min(TrialIndex + 1, TotalTrials); }
	EFieldPosition GetPosition() const { return Position; }

	FString GetSituationText() const { return CurrentTrial.Play.Situation; }
	FString GetRunnersText() const { return CurrentTrial.Play.RunnerText; }

	EBackupPhase GetPhase() const { return Phase; }
	bool IsHoldTrial() const { return CurrentTrial.bIsHoldTrial; }

	/**
	 * 정답을 공개해도 되는 시점인가 — 판정이 끝난 뒤(복기)에만 true.
	 * 진행 중(Live)에 정답 존을 강조하면 판단 훈련이 아니라 "초록 원 따라가기"가 된다.
	 * 월드 존 그리기(DrawZones)와 HUD 미니맵이 같은 기준을 쓰도록 여기서 한 번만 정의한다.
	 */
	bool IsAnswerRevealed() const { return Phase == EBackupPhase::Done && bHasResult; }

	/** 현재 타구 속도 배율 (연출용, [ / ] 로 조절). 1.0 이 기본. */
	float GetBallSpeedScale() const { return BallSpeedScale; }

	/** 지금까지 흐른 판단 시간 (초). 아직 확정 전이면 라이브 값, 확정 후면 -1(HUD 는 결과값을 따로 읽음). */
	float GetLiveDecisionSec() const;

	/** 마지막 시행의 판단 시간 (초). 없으면 -1. */
	float GetLastDecisionSec() const { return bHasResult ? LastResult.DecisionTimeSec : -1.0f; }

	/** 세션 평균 판단 시간 (정상적으로 개시한 시행만). 표본 없으면 -1. */
	float GetAverageDecisionSec() const;

	/** 세션 평균 경로 효율 (0~1). 표본 없으면 -1. */
	float GetAveragePathEfficiency() const;

	/** 마지막 판정 결과 문구/색. 표시할 게 있으면 true. */
	bool GetLastOutcomeText(FString& OutText, FLinearColor& OutColor) const;

	/** 마지막 시행의 정답 해설 (영문). */
	FString GetLastExplainText() const { return CurrentTrial.CorrectZone.Explain; }

	/** 탑다운 미니맵 그리기에 필요한 것들 (BackupHUD 가 읽는다). */
	const FBaseballField& GetField() const { return Field; }
	const FBackupTrial& GetCurrentTrial() const { return CurrentTrial; }
	FVector GetPlayerXY() const;

	/** 세션 종료 후 AI 판단 코칭 문구. */
	const FString& GetCoachingText() const { return CoachingText; }
	const TArray<FTrainingDrill>& GetRecommendedDrills() const { return LastDrills; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, Category = "Backup")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, Category = "Backup")
	TObjectPtr<UCameraComponent> Camera;

	/** 이동 게이트 겸 조준 포인터로 쓰는 컨트롤러 (기본 오른손). */
	UPROPERTY(VisibleAnywhere, Category = "Backup|VR")
	TObjectPtr<UMotionControllerComponent> MoveController;

	UPROPERTY(VisibleAnywhere, Category = "Backup|VR")
	TObjectPtr<UVRInfoPanel> VrPanel;

	/** 실측 다이아몬드 좌표 + 백업 존 기하 (⚠️ 실측 캘리브레이션 대상 — 전부 EditAnywhere). */
	UPROPERTY(EditAnywhere, Category = "Backup|Field")
	FBaseballField Field;

	// ── 설정값 ──

	/** 한 세션의 총 시행 수 (에디터/디테일 패널에서 조절). */
	UPROPERTY(EditAnywhere, Category = "Backup", meta = (ClampMin = "1"))
	int32 TotalTrials = 6; // VR 멀미 노출 축소 — 기본 10 아님 (설계 노트 참고).

	/** 세션 안에서 "정답=제자리(Hold)" 시행이 나올 최소 비율. 무조건 뛰는 편법을 막는다. */
	UPROPERTY(EditAnywhere, Category = "Backup", meta = (ClampMin = "0.0", ClampMax = "0.6"))
	float HoldTrialFraction = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Backup")
	float IntervalBetweenTrials = 3.0f; // VR 멀미 노출 축소 — 다음 시행 전 정지 프레임으로 회복할 시간.

	/** 이동 개시(1단계) 커밋 조건 — 누적 변위 또는 게이트 경과 시간 중 먼저 찬 쪽. */
	UPROPERTY(EditAnywhere, Category = "Backup", meta = (ClampMin = "10.0"))
	float HeadingCommitDistanceCm = 200.0f;
	UPROPERTY(EditAnywhere, Category = "Backup", meta = (ClampMin = "0.05"))
	float HeadingCommitTimeSec = 0.35f;

	/** 방향 판단 1·2위 후보의 최소 각도差(도). 이보다 좁으면 방향 판정을 자문으로만 쓴다. */
	UPROPERTY(EditAnywhere, Category = "Backup", meta = (ClampMin = "1.0"))
	float DirectionMarginDeg = 12.0f;

	/** VR 입력이 PC(즉시 키 입력) 대비 갖는 구조적 지연 — 판단 시간에서 빼준다. */
	UPROPERTY(EditAnywhere, Category = "Backup", meta = (ClampMin = "0.0"))
	float VRInputLatencyBiasSec = 0.15f;

	/** Hold 시행에서 이 시간(초) 동안 게이트 입력이 없으면 성공. */
	UPROPERTY(EditAnywhere, Category = "Backup", meta = (ClampMin = "0.5"))
	float HoldWindowSec = 2.5f;

	/** 트랙패드/스틱 데드존. */
	UPROPERTY(EditAnywhere, Category = "Backup|VR", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float StickDeadzone = 0.2f;

	/** 첫 프레임 접촉 노이즈 방지 — 게이트 개시 첫 프레임엔 이 값 이상이어야 축을 인정한다. */
	UPROPERTY(EditAnywhere, Category = "Backup|VR", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float FirstFrameAxisFloor = 0.35f;

	/** 이동 게이트로 볼 트리거 임계값. */
	UPROPERTY(EditAnywhere, Category = "Backup|VR", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float GateTriggerThreshold = 0.6f;

	/** PC 이동 속도 (cm/s). VR 은 Field.MoveSpeedCms 를 쓴다(제한 시간 파생과 같은 값이어야 공정). */
	UPROPERTY(EditAnywhere, Category = "Backup")
	float PCMoveSpeedCms = 700.0f;

	// ── PC 시야 조작 (VR 은 고개를 돌리면 되므로 해당 없음) ──
	// PC 는 폰이 항상 홈을 보도록 고정돼 있어서 둘러볼 수단이 아예 없었다. 동료 수비수
	// 마커(AFielderMarker)를 세워 놔도 등 뒤에 있으면 못 보므로 판단 근거가 되지 못한다.

	/** 키(Q/E · ←/→)로 도는 속도 (도/초). */
	UPROPERTY(EditAnywhere, Category = "Backup|PC", meta = (ClampMin = "10.0"))
	float PCTurnSpeedDegPerSec = 110.0f;

	/** 마우스 시야 감도 (도/픽셀). 0 이면 마우스 룩을 끈다 (키보드만 사용). */
	UPROPERTY(EditAnywhere, Category = "Backup|PC", meta = (ClampMin = "0.0"))
	float PCMouseLookSensitivity = 2.0f;

	/** 마우스 상하 반전. */
	UPROPERTY(EditAnywhere, Category = "Backup|PC")
	bool bPCInvertMouseY = false;

	/** 카메라 상하 각도 제한 (도). 뜬공을 올려다볼 수 있을 만큼은 열어 둔다. */
	UPROPERTY(EditAnywhere, Category = "Backup|PC", meta = (ClampMin = "10.0", ClampMax = "89.0"))
	float PCMaxPitchDeg = 75.0f;

	/** 첫 시행까지의 대기 (초). VR 은 HMD 포즈가 BeginPlay 시점에 아직 없어 배치를 못 잡는다. */
	UPROPERTY(EditAnywhere, Category = "Backup", meta = (ClampMin = "0.2"))
	float FirstTrialDelaySec = 1.5f;

	// ── 타구 연출 (코스메틱 — 판정에는 관여하지 않는다) ──
	// 실제 경기처럼 타구가 날아오는 걸 보여주기 위한 것. 판정은 큐 시점(SpawnNextTrial)부터
	// 이미 시작돼 있다 — 실전에서 백업 판단은 타구가 뜨는 "순간" 시작되지, 공이 착지하거나
	// 누가 잡을 때까지 기다리지 않기 때문. 그래서 공의 도착 여부는 이동/판정에 영향을 주지 않는다.

	/** 공 액터 클래스. 미지정 시 ACatchBall 기본 사용. */
	UPROPERTY(EditAnywhere, Category = "Backup|Ball")
	TSubclassOf<ACatchBall> BallClass;

	/** 기준 타구 체공시간 (초). BallSpeedScale 로 나눠 실제 체공시간을 만든다. */
	UPROPERTY(EditAnywhere, Category = "Backup|Ball", meta = (ClampMin = "0.3"))
	float BallFlightSec = 1.4f;

	/**
	 * 타구 속도 배율. 체공시간을 이 값으로 나눈다 (배율↑ = 체공↓ = 빨라짐).
	 * `[` `]` 로 조절 — 다음 시행부터 반영 (CatchBallPawn 의 BallSpeedScale 과 동일 패턴).
	 * 사용자가 실제로 요청한 "느리게 볼 수 있게" 를 만족시키는 손잡이다.
	 */
	UPROPERTY(EditAnywhere, Category = "Backup|Ball", meta = (ClampMin = "0.4", ClampMax = "2.0"))
	float BallSpeedScale = 0.8f; // 판단 훈련이라 기본을 CatchBall(1.0)보다 살짝 느긋하게.

	/**
	 * 커버/백업 시행에서 타구가 **내 수비 위치로부터 최소 이만큼은 떨어져** 떨어지게 한다 (cm).
	 *
	 * 왜 필요한가: 정답이 커버/백업이라는 건 "이 공은 남이 처리한다"는 뜻이다. 그런데 연출용
	 * 타구가 내 발밑으로 날아오면 눈은 "네 공이야"라고 말하는데 정답은 "베이스로 가라"가 되어
	 * 서로 어긋난다 — 플레이어가 룰을 의심하게 되는 자리다. 그래서 그런 시행에서는 타구를
	 * 실제로 처리하는 야수 쪽으로 밀어낸다.
	 *
	 * ⚠️ Hold 시행(= 정답이 제자리 = 내가 처리하는 공)에는 적용하지 않는다. 그쪽은 오히려
	 *    내 쪽으로 와야 맞다 — 그게 "이 공은 내 담당"이라는 신호다.
	 * ⚠️ 판정(FBackupTrial)은 건드리지 않는다. 이건 순수 연출 보정이다.
	 */
	UPROPERTY(EditAnywhere, Category = "Backup|Ball", meta = (ClampMin = "0.0"))
	float BallClearanceFromMeCm = 900.0f;

	UPROPERTY(EditAnywhere, Category = "Backup|Ball", meta = (ClampMin = "0.05", ClampMax = "0.5"))
	float BallSpeedStep = 0.1f;

	// ── 동료 수비수 3D 마커 (판단 근거 — 판정에는 관여하지 않는다) ──
	// 정답 존 강조를 걷어낸 자리를 메우는 것. "누가 어디 서 있는가"가 보여야 백업 판단이
	// 성립한다. HUD 미니맵은 헤드셋에 렌더되지 않으므로 VR 에선 이게 유일한 배치 단서다.

	/** 마커 액터 클래스. 미지정 시 AFielderMarker 기본 사용. */
	UPROPERTY(EditAnywhere, Category = "Backup|Fielders")
	TSubclassOf<AFielderMarker> FielderMarkerClass;

	/** 동료 마커를 세울지. 끄면 예전처럼 빈 필드가 된다 (비교·디버그용). */
	UPROPERTY(EditAnywhere, Category = "Backup|Fielders")
	bool bShowFielderMarkers = true;

private:
	// ── PC 이동 입력 (BindKey 눌림/뗌 → 플래그, 다른 폰들과 동일 패턴) ──
	void OnFwdPressed()    { bMoveFwd = true; }
	void OnFwdReleased()   { bMoveFwd = false; }
	void OnBackPressed()   { bMoveBack = true; }
	void OnBackReleased()  { bMoveBack = false; }
	void OnLeftPressed()   { bMoveLeft = true; }
	void OnLeftReleased()  { bMoveLeft = false; }
	void OnRightPressed()  { bMoveRight = true; }
	void OnRightReleased() { bMoveRight = false; }

	bool bMoveFwd = false;
	bool bMoveBack = false;
	bool bMoveLeft = false;
	bool bMoveRight = false;

	// ── PC 시야 회전 (Q/E · ←/→) ──
	void OnTurnLeftPressed()   { bTurnLeft = true; }
	void OnTurnLeftReleased()  { bTurnLeft = false; }
	void OnTurnRightPressed()  { bTurnRight = true; }
	void OnTurnRightReleased() { bTurnRight = false; }

	bool bTurnLeft = false;
	bool bTurnRight = false;

	/**
	 * PC 시야 회전 — 키 + 마우스. VR 이면 아무것도 하지 않는다.
	 * ⚠️ 좌우는 **폰 자체**를 돌린다. WASD 이동이 GetActorRotation().Yaw 기준이라
	 *    (아래 이동 코드 참고) 이렇게 해야 "보는 방향으로 걷는다"가 유지된다.
	 *    상하는 카메라 상대 회전만 건드린다 — 폰을 기울이면 이동 평면까지 기운다.
	 */
	void TickPCLook(float DeltaSeconds);

	void ReturnToModeSelect(); // M

	// 타구 속도 조절 ([ 느리게 / ] 빠르게) — 다음 시행부터 반영.
	void SpeedDown();
	void SpeedUp();

	/** 큐 시점에 이번 플레이의 대략적인 방향·깊이로 코스메틱 타구를 띄운다 (판정과 무관). */
	void SpawnFlavorBall();

	/**
	 * 커버/백업 시행에서 타구 낙하점이 내 수비 위치에 너무 가까우면 밀어낸다.
	 * 밀어내는 방향은 **실제로 공을 처리하는 야수 쪽** — 그래야 "저 사람 공이다"가 눈에 보인다.
	 * (BallClearanceFromMeCm 주석 참고. 연출 전용 — 판정에는 영향 없음.)
	 */
	FVector ClearBallFromMySpot(const FVector& Target) const;

	// ── 세션 진행 ──
	void StartSession();
	void SpawnNextTrial();
	void FinishTrial(EBackupOutcome Outcome);
	void EndSession();

	/** 종료 화면의 PLAY AGAIN — 끝난 판을 저장하고 같은 폰에서 새 세션을 시작한다. */
	void RestartSession();

	/** 다음 플레이를 무반복 주머니에서 뽑는다 (Hold-비율 롤 포함). */
	FBackupPlay DrawNextPlay();
	void RefillBags();

	/** 플레이어를 자기 수비 위치로 되돌리고 홈을 보게 한다 (매 시행 재적용 — 실내 드리프트 보정). */
	void SnapToFieldingSpot();

	/** 플레이어가 서 있는 바닥면 Z. VR: 트래킹 원점=바닥. PC: 캡슐 중심이라 반높이 아래. */
	float FloorZ() const;

	// ── 판정 진행 (Tick 에서 매 프레임) ──
	void TickTrial(float DeltaSeconds);

	/** 지금 이동 게이트(VR 트리거 / PC WASD)가 눌려 있는지 — 부수효과 없이 순수 조회만. */
	bool IsGateCurrentlyHeld() const;

	/** 이동 개시(1단계) 커밋 시도 — 조건이 차면 방향 판정까지 끝낸다. */
	void TryCommitHeading();

	/**
	 * 후보 백업 존을 디버그 드로우로 그린다 (헤드셋에도 렌더됨).
	 * 진행 중엔 후보 전부를 **같은 중립색**으로 — 선택지는 알려주되 정답은 숨긴다.
	 * 판정이 끝나면(IsAnswerRevealed) 정답만 초록으로 강조해 복기시킨다.
	 */
	void DrawZones() const;

	// ── 동료 수비수 3D 마커 ──
	/** 7개 수비 위치에 마커를 세운다 (본인 자리는 이름표만). 세션 시작 전 1회. */
	void SpawnFielderMarkers();
	void DestroyFielderMarkers();
	/** 이름표가 플레이어를 향하도록 매 프레임 돌린다. */
	void UpdateFielderLabels();

	// ── AI 판단 코칭 ──
	FWeaknessReport BuildBackupReport() const;
	void RequestBackupFeedback();
	void FlushSessionToSave();

	UFUNCTION()
	void HandleCoachingReady(bool bSuccess, const FString& Text);

	/** VR 상태 패널 갱신. */
	void RefreshVrPanel();

	// ── 상태 ──
	TArray<FBackupPlay> PlayTable;
	TArray<FBackupAssignmentRule> RuleTable;

	/** 무반복 주머니 — PlayTable 인덱스. Eligible=비-Hold 전용, All=전체(Hold 롤용). */
	TArray<int32> EligibleBag;
	TArray<int32> AllBag;

	EFieldPosition Position = EFieldPosition::Second;

	FBackupTrial CurrentTrial;
	EBackupPhase Phase = EBackupPhase::Done;

	FVector HomeLocation = FVector::ZeroVector; // 세션 시작 시점의 폰 위치 (스폰 자리 참고용).

	int32 TrialIndex = 0;
	int32 SuccessCount = 0;
	bool  bSessionOver = false;
	bool  bWaitingNext = false;
	float IntervalTimer = 0.0f;

	// ── 판단(1단계) 상태 ──
	float CueTimeSec = -1.0f;       // 월드 시간 기준, 시행 시작.
	FVector CueXY = FVector::ZeroVector;
	bool  bGateLatchedAtCue = false; // 큐 시점에 이미 게이트가 눌려 있었다 — 한 번 떼야 인정.
	bool  bGateEverReleased = false;
	float GateElapsedSec = 0.0f;    // 큐 이후 게이트가 눌려 있던 누적 시간.
	float DisplacedCm = 0.0f;       // 큐 이후 누적 순 변위(직선 거리).
	float AccumulatedPathCm = 0.0f; // 큐 이후 실제 이동한 경로 길이(직선 아님 — 헤맨 만큼 커짐).
	bool  bHeadingCommitted = false;
	bool  bDirectionCorrect = false;
	bool  bAmbiguousDirection = false;
	bool  bFalseStart = false;      // 선출발(큐 전 게이트 유지) 또는 Hold 시행 중 이동.

	/** TryCommitHeading 이 계산한 판단 시간(초, VR 지연 보정 적용) — FinishTrial 이 결과에 옮긴다. */
	float PendingDecisionTimeSec = -1.0f;

	TArray<FBodyPoseSample> TrialSamples; // 이번 시행의 위치 샘플 — Data/BodyPose.h 의 첫 소비자.

	FBackupResult LastResult;
	bool bHasResult = false;

	TArray<FBackupResult> SessionResults;

	// ── AI 운동 추천 상태 ──
	UPROPERTY(Transient)
	TObjectPtr<UAIFeedbackService> FeedbackService;

	TArray<FTrainingDrill> LastDrills;
	FString CoachingText;
	bool bAwaitingCoaching = false;

	// ── VR ──
	bool bVR = false;
	FVector PrevControllerLoc = FVector::ZeroVector;
	bool bHasPrevControllerLoc = false;

	FVRExitGesture ExitGesture;

	/** 세션 종료 화면의 선택 카드(PLAY AGAIN / BACK TO MENU) 겨눔 상태. */
	FVREndCardMenu EndMenu;

	/** 종료 화면에서 선택 카드가 놓이는 첫 행 인덱스 (그 위쪽은 결과 내용). */
	static constexpr int32 EndCardFirstRow = 3;

	/** 플레이 중 나가기 제스처 임계 — 이 종목의 자연 동작과 겹치지 않게 조인 값. */
	static constexpr float LiveExitUpThreshold = 0.85f;
	static constexpr float LiveExitHoldSec     = 2.0f;

	/** 이번 시행의 코스메틱 타구 (판정에 관여하지 않음 — SpawnFlavorBall 참고). */
	UPROPERTY(Transient)
	TObjectPtr<ACatchBall> ActiveBall;

	/** 동료 수비수 마커 (세션 내내 유지 — 시행마다 다시 세우지 않는다). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AFielderMarker>> FielderMarkers;
};
