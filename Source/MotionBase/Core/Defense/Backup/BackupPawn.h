#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Core/Defense/Backup/BackupTypes.h"
#include "Core/Defense/Backup/BaseballField.h"
#include "Data/BodyPose.h"
#include "Data/TrainingFeedback.h"
#include "UI/VRExitGesture.h"
#include "UI/VREndCardMenu.h"
#include "Core/DynamicDifficulty.h"
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

	/**
	 * 마지막 시행의 정답 해설 (영문).
	 * AI 확장 해설이 도착해 있으면 그걸, 아니면 규칙 테이블의 저작 한 줄을 돌려준다 —
	 * **화면이 비는 경우는 없다.** (키 없음·네트워크 실패·응답 지연 전부 저작 해설로 수렴)
	 */
	FString GetLastExplainText() const
	{
		return CurrentAIExplain.IsEmpty() ? CurrentTrial.CorrectZone.Explain : CurrentAIExplain;
	}

	/** 탑다운 미니맵 그리기에 필요한 것들 (BackupHUD 가 읽는다). */
	const FBaseballField& GetField() const { return Field; }
	const FBackupTrial& GetCurrentTrial() const { return CurrentTrial; }
	FVector GetPlayerXY() const;

	/** 세션 종료 후 AI 판단 코칭 문구. */
	const FString& GetCoachingText() const { return CoachingText; }
	const TArray<FTrainingDrill>& GetRecommendedDrills() const { return LastDrills; }

	/** 트리거를 못 잡아 스틱 단독 이동으로 저하됐는지 — 패널/HUD 경고용. */
	bool IsGateDegraded() const { return bVR && !bGateRequired; }

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

	// ── 동적 난이도 (기록·성적 기반 자동 상승) ──
	// PitchingZone(타격)과 같은 계약: 세션 시작 시 과거 평균으로 시드, 시도마다 성과로 조정.
	// ⚠️ 헤드룸 값은 실측 캘리브레이션 대상 — 하드코딩 확정 금지.
	//
	// 범위: "제한시간 단축"만 다룬다(Field.SlackFactor — 폰마다 독립된 값이라 협동 세션에서
	// 다른 플레이어에게 안 새어 나간다). "핵심 시나리오 비중 상승"은 시나리오 선택이
	// ABackupGameState(협동 세션 전체가 공유)에 있어, 플레이어 한 명의 개인 난이도로
	// 공유 상태를 흔드는 게 맞는지 설계 결정이 더 필요해 이번 범위에서 제외했다.

	/** 기록·성적 기반으로 도착 제한시간을 자동 조절할지. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backup|Difficulty")
	bool bDynamicDifficulty = true;

	/** 동적 상승 최대 여유배율 축소 — Field.SlackFactor 에서 이만큼 줄어든다(하한은 1.0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backup|Difficulty")
	float DynamicSlackReduction = 0.25f;

	/** 정답 1회당 동적 수준 상승폭. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backup|Difficulty")
	float DynamicStepUp = 0.12f;

	/** 오답/시간초과 1회당 동적 수준 하강폭. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backup|Difficulty")
	float DynamicStepDown = 0.08f;

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

	/**
	 * 게이트 자동 저하 문턱(초). 트리거가 **한 번도** 안 잡히는데 스틱 입력만 이 시간만큼
	 * 누적되면 게이트 요구를 내리고 스틱 단독 이동으로 전환한다.
	 *
	 * ⚠️ 편의 기능이 아니라 안전장치다. 이 프로젝트는 순수 OpenXR 이라 런타임/컨트롤러
	 *    조합에 따라 트리거가 `MotionController_*_Trigger` 이름으로 안 올라올 수 있다.
	 *    저하가 없으면 그 순간 플레이어는 "화면은 멀쩡한데 한 발짝도 못 움직이는" 상태가
	 *    되어 드릴 자체가 성립하지 않는다. 대신 조용히 넘어가지 않고 패널에 경고를 띄운다 —
	 *    입력이 깨졌다는 사실은 표면화되어야 튜닝 대상이 된다.
	 */
	UPROPERTY(EditAnywhere, Category = "Backup|VR", meta = (ClampMin = "1.0"))
	float GateProbeSec = 4.0f;

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
	// ⚠️ **진행권은 이 폰이 아니라 ABackupGameState 에 있다.** 협동 멀티플레이에서 전원이
	//    같은 타구를 같은 순간에 봐야 하기 때문. 이 폰은 (a) 시행 시리얼이 바뀌면 그 플레이로
	//    자기 시행을 시작하고, (b) 자기 시행이 끝나면 보고할 뿐이다.
	//    싱글플레이에서도 같은 경로로 돈다(스탠드얼론은 HasAuthority()==true).
	void StartSession();

	/** GameState 가 정한 플레이 인덱스로 내 시행을 시작한다. */
	void SpawnNextTrial(int32 PlayIndex);

	void FinishTrial(EBackupOutcome Outcome);
	void EndSession();

	/** 종료 화면의 PLAY AGAIN — 끝난 판을 저장하고 같은 폰에서 새 세션을 시작한다. */
	void RestartSession();

	/** 이 월드의 백업 GameState. 없으면 nullptr (다른 모드에서 스폰된 비정상 상황). */
	class ABackupGameState* GetBackupGameState() const;

	/**
	 * **이 폰의 시행 진행을 실제로 맡고 있는** GameState. 단독 모드면 nullptr.
	 *
	 * ⚠️ 진행 주체 판단은 반드시 이 함수 하나만 쓴다. 예전엔 Tick 이 래치된
	 *    bLocalSessionFallback 으로, FinishTrial 은 GetBackupGameState() 조회로 갈라져
	 *    있었다. GameState 가 BeginPlay 이후에 나타나면(클라이언트 복제 지연 등) 두 경로가
	 *    엇갈려 — Tick 은 로컬 타이머를 기다리는데 FinishTrial 은 등록도 안 된 GameState 에
	 *    보고만 하고 타이머를 안 걸어 — **시행 1회 후 아무 에러 없이 영구 정지했다.**
	 */
	class ABackupGameState* GetOwningGameState() const;

	/**
	 * 단독 모드로 시작했는데 GameState 가 뒤늦게 나타났으면 그쪽으로 넘긴다.
	 * (클라이언트에서 GameState 복제가 BeginPlay 보다 늦는 경우의 복구 경로.)
	 */
	void TryAttachToLateGameState();

	/** GameState 의 복제 상태를 로컬 표시용 값(TrialIndex/bSessionOver 등)에 반영한다. */
	void SyncFromGameState();

public:
	/**
	 * **혼자 플레이할 때만** 쓰는 편향 추첨 — 내 포지션에서 할 일이 있는 플레이를 우선하되
	 * HoldTrialFraction 확률로 전체 주머니에서 뽑는다. GameState 가 등록 폰이 하나일 때
	 * 이 함수에 위임한다(협동에선 전체 테이블에서 그냥 뽑는다 — 설계 노트 참고).
	 * @return PlayTable 인덱스. 테이블이 비었으면 INDEX_NONE.
	 */
	int32 DrawSoloPlayIndex();

private:
	void RefillBags();

	/** 플레이어를 자기 수비 위치로 되돌리고 홈을 보게 한다 (매 시행 재적용 — 실내 드리프트 보정). */
	void SnapToFieldingSpot();

	/** 플레이어가 서 있는 바닥면 Z. VR: 트래킹 원점=바닥. PC: 캡슐 중심이라 반높이 아래. */
	float FloorZ() const;

	// ── 판정 진행 (Tick 에서 매 프레임) ──
	void TickTrial(float DeltaSeconds);

	/** 지금 이동 게이트(VR 트리거 / PC WASD)가 눌려 있는지 — 부수효과 없이 순수 조회만. */
	bool IsGateCurrentlyHeld() const;

	/**
	 * VR 스틱/트랙패드 축 원시값(데드존 적용 전)을 읽는다. 컨트롤러/PC 가 없으면 false.
	 *
	 * 게이트 성립 여부와 **독립적으로** 읽을 수 있어야 한다 — 자동 저하 판정이
	 * "트리거는 안 잡히는데 스틱은 움직이고 있다"를 관측해야 하기 때문.
	 */
	bool ReadVRStickAxes(float& OutX, float& OutY) const;

	/**
	 * 지금 "이동 의도"가 있는지 — 정상 상태엔 트리거, 저하 상태엔 스틱 밀림.
	 * 선출발 방지 래치가 저하 상태에서도 같은 의미로 동작하게 하는 단일 정의다.
	 */
	bool IsMoveIntentHeld() const;

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

	// ── AI 플레이 해설 (판정이 아니라 설명) ──

	/**
	 * 이번 시행의 해설을 준비한다. 캐시에 있으면 즉시, 없으면 요청을 건다.
	 *
	 * ⚠️ **시행이 끝날 때가 아니라 시작할 때 부른다(prefetch).** 정답은 BuildTrial 시점에
	 *    이미 확정돼 있으므로 미리 물어볼 수 있고, 플레이어가 뛰는 2~10초 동안 응답이
	 *    도착한다. 판정 후에 요청하면 결과 표시(3초)를 왕복 지연이 잡아먹는다.
	 *    표시는 IsAnswerRevealed() 이후에만 일어나므로 정답이 미리 새지 않는다.
	 */
	void PrepareExplanationForCurrentTrial();

	UFUNCTION()
	void HandlePlayExplanationReady(bool bSuccess, const FString& CacheKey, const FString& Explanation);

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

	/** 동적 난이도 상태 (0~1) — SpawnNextTrial 이 Field.SlackFactor 계산에 쓴다. */
	FDynamicDifficultyLevel DynamicDifficulty;

	/** StartSession 에서 캡처한 디자이너 기본 여유배율 — 매 시행 재계산이 누적되지 않게 기준선으로 쓴다. */
	float BaseSlackFactor = 1.35f;

	/**
	 * 마지막으로 처리한 GameState 시행 시리얼. 이 값과 달라지면 새 시행으로 본다.
	 * -1 로 시작해 "아직 아무 시행도 못 봤음"을 나타낸다 (시리얼은 0부터 시작).
	 */
	int32 LastSeenTrialSerial = -1;

	/**
	 * GameState 를 못 찾아 이 폰이 직접 시행을 진행하는 중인가.
	 *
	 * 협동 기능은 여러 대가 실제로 붙었을 때만 얹히는 것이고, **혼자 하는 경우는 어떤
	 * 경우에도 깨지면 안 된다.** GameState 스폰이 실패하거나 다른 GameMode 를 쓰는 맵에서
	 * 열려도 드릴은 예전 방식 그대로 돌아야 한다.
	 */
	bool bLocalSessionFallback = false;

	/** 이번 시행의 AI 해설. 비어 있으면 저작 해설로 표시된다. */
	FString CurrentAIExplain;

	/**
	 * 이번 시행의 해설 캐시 키. 응답이 늦게 와서 이미 다음 시행으로 넘어갔다면 키가
	 * 달라지므로 버린다 — **안 버리면 틀린 상황의 해설이 붙는다.**
	 */
	FString CurrentExplainKey;
	bool  bSessionOver = false;
	bool  bWaitingNext = false;
	float IntervalTimer = 0.0f;

	// ── 판단(1단계) 상태 ──
	float CueTimeSec = -1.0f;       // 월드 시간 기준, 시행 시작.
	FVector CueXY = FVector::ZeroVector;
	bool  bGateLatchedAtCue = false; // 큐 시점에 이미 게이트가 눌려 있었다 — 한 번 떼야 인정.
	bool  bGateEverReleased = false;
	float GateElapsedSec = 0.0f;    // 큐 이후 게이트가 눌려 있던 누적 시간.

	// ── 게이트 자동 저하 상태 ──
	// 세션 단위로 유지한다(시행마다 리셋 금지) — 한 번 저하됐으면 남은 시행 내내 유지돼야
	// 하고, 프로브 누적도 시행 경계에서 끊기면 GateProbeSec 을 영영 못 채운다.
	bool  bGateRequired = true;      // false = 트리거 없이 스틱만으로 이동.
	bool  bGateEverObserved = false; // 트리거가 한 번이라도 잡힌 적 있는가.
	float AxisWithoutGateSec = 0.0f; // 게이트 없이 스틱만 들어온 누적 시간.
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
