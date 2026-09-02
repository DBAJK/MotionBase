#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Core/Defense/Throw/ThrowTypes.h"
#include "Data/TrainingFeedback.h"
#include "UI/VRExitGesture.h"
#include "ThrowPawn.generated.h"

class UCameraComponent;
class UCapsuleComponent;
class UMotionControllerComponent;
class UStaticMeshComponent;
class UVRInfoPanel;
class ACatchBall;
class UAIFeedbackService;

/** 한 시행의 진행 단계. 전환 시간(transfer)을 재려면 "잡은 순간"이 상태로 있어야 한다. */
UENUM(BlueprintType)
enum class EThrowPhase : uint8
{
	Feed      UMETA(DisplayName = "급구 대기"),   // 공이 날아오는 중 — 포구해야 함
	Ready     UMETA(DisplayName = "송구 준비"),   // 공을 든 상태 — 목표 베이스로 던진다
	InFlight  UMETA(DisplayName = "송구 중"),     // 던진 공이 날아가는 중
	Done      UMETA(DisplayName = "결과")         // 판정 표시 후 다음 시행 대기
};

/**
 * 송구 훈련 폰 (1인칭).
 *
 * 한 시행 = **급구 포구 → 전환 → 목표 베이스로 송구**.
 *   1) 목표 베이스(1루/2루/3루/홈)가 지정·표시된다.
 *   2) 공이 날아온다 → 잡는다 (VR: 글러브 근접 / PC: Space).
 *   3) 잡은 순간부터 시계가 돈다 → 던진다 (VR: 컨트롤러 스윙 / PC: Space 홀드-릴리스).
 *   4) 착지점으로 정확도 판정 → 10구 반복.
 *
 * 스펙 측정 지표 3종을 모두 남긴다: 정확도(목표 zone 도달) · 구속 · 전환 시간.
 * 굳이 급구부터 시작하는 이유는 **전환 시간이 "잡은 순간" 없이는 정의되지 않기 때문**이다.
 *
 * 방향은 자동 조준(목표 베이스 쪽). 플레이어는 파워(거리)만 맞춘다.
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

	/** 이번 목표 베이스 거리를 정확히 맞히는 파워 (0~1). 게이지 위 정답 표시선. */
	float GetIdealPower() const { return CurrentTrial.IdealPower; }

	/** 현재 진행 단계 (HUD 안내 문구 분기용). */
	EThrowPhase GetPhase() const { return Phase; }

	/** 이번 시행의 목표 베이스와 그 표시 이름. */
	EBaseType GetTargetBase() const { return CurrentTrial.TargetBase; }
	static FString BaseName(EBaseType Base);

	/** 공을 잡은 뒤 현재까지 흐른 전환 시간 (초). Ready 단계에서만 의미 있다. */
	float GetLiveTransferTime() const;

	// ── 측정 지표 접근자 (세션 누적) ──
	/** 목표 베이스별 시행/성공 (측정 지표 ①의 분해). */
	void  GetBaseStats(EBaseType Base, int32& OutAttempt, int32& OutSuccess) const;
	/** 세션 평균 구속 (km/h). 표본 없으면 0. */
	float GetAverageReleaseKmh() const;
	/** 세션 평균 전환 시간 (초). 정상 포구한 시행만 평균낸다. 표본 없으면 -1. */
	float GetAverageTransferSec() const;

	/** 마지막 판정 결과 문구/색. 표시할 게 있으면 true. */
	bool GetLastOutcomeText(FString& OutText, FLinearColor& OutColor) const;

	/** 마지막 시행의 측정값 3종 요약 (HUD 한 줄). 값이 없으면 빈 문자열. */
	FString GetLastMetricsLine() const;

	/** 세션 종료 후 AI 운동 추천 문구 (없으면 빈 문자열). */
	const FString& GetCoachingText() const { return CoachingText; }

	/** 세션 종료 후 추천된 드릴 목록. */
	const TArray<FTrainingDrill>& GetRecommendedDrills() const { return LastDrills; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, Category = "Throw")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, Category = "Throw")
	TObjectPtr<UCameraComponent> Camera;

	/** VR 송구 손 = 오른손 컨트롤러. 앞으로 던지는 동작 속도로 파워가 정해진다. */
	UPROPERTY(VisibleAnywhere, Category = "Throw|VR")
	TObjectPtr<UMotionControllerComponent> ThrowController;

	/** 손에 든 공 시각 표시. 공을 잡고 있는 동안(Ready)만 보인다. */
	UPROPERTY(VisibleAnywhere, Category = "Throw|VR")
	TObjectPtr<UStaticMeshComponent> BallInHandMesh;

	/** VR 헤드셋 안 상태 패널 (진행·파워·결과·안내). 월드 고정. PC 모드에선 숨김. */
	UPROPERTY(VisibleAnywhere, Category = "Throw|VR")
	TObjectPtr<UVRInfoPanel> VrPanel;

	// ── VR 송구 튜닝 ──
	/**
	 * 이 속도(cm/s) 이상으로 컨트롤러를 휘두르면 **송구 동작 시작**으로 본다(발사 아님).
	 *
	 * ⚠️ 판정은 컨트롤러의 **위치 이동 속도만** 본다 (타격과 달리 회전 ω×r 보정이 없다).
	 *    그래서 손목만 까딱하면 컨트롤러가 실제로 이동한 거리가 짧아 이 값을 못 넘고,
	 *    플레이어 입장에서는 "던졌는데 공이 안 나간다"로 보인다.
	 *    250 → 180 으로 낮춰 팔을 크게 못 쓰는 사람도 동작이 인식되게 했다.
	 */
	UPROPERTY(EditAnywhere, Category = "Throw|VR")
	float ThrowTriggerSpeedCms = 180.0f;

	/** 파워 0 에 대응하는 손 속도 (cm/s). ThrowTriggerSpeedCms 와 같이 움직여야 한다 —
	 *  트리거보다 크면 동작은 인식됐는데 파워가 0 인 구간이 생긴다. */
	UPROPERTY(EditAnywhere, Category = "Throw|VR")
	float MinThrowSpeedCms = 180.0f;

	/**
	 * 파워 1 에 대응하는 손 속도 (cm/s). 이 이상은 최대 파워.
	 * 실측 기준: VR 컨트롤러를 힘껏 휘두르면 대략 800~1100 cm/s 가 나온다.
	 * (예전 1500 은 사람이 도달하기 어려운 값이라 항상 저파워로 눌렸다.)
	 */
	UPROPERTY(EditAnywhere, Category = "Throw|VR")
	float MaxThrowSpeedCms = 900.0f;

	/**
	 * 손이 최고 속도의 이 비율 아래로 **감속하면 릴리스**로 본다 (0~1).
	 *
	 * ⚠️ 이게 없으면 던지기가 성립하지 않는다: 손 속도가 트리거를 넘는 순간 곧바로 발사하면
	 *    그 시점은 **팔을 막 뻗기 시작한 지점**이라 속도가 트리거값과 같고 → 파워 ≈ 0 →
	 *    공이 발밑에 툭 떨어진다. 실제 릴리스는 "가장 빠른 순간 직후"이므로,
	 *    피크 속도를 기억했다가 감속이 시작될 때 그 **피크값**으로 발사한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Throw|VR", meta = (ClampMin = "0.1", ClampMax = "0.95"))
	float ReleaseDecelRatio = 0.65f;

	/** 던지기 동작이 이 시간(초)을 넘기면 피크값으로 강제 발사 (감속을 못 잡는 경우 대비). */
	UPROPERTY(EditAnywhere, Category = "Throw|VR", meta = (ClampMin = "0.1"))
	float MaxThrowMotionSec = 0.6f;

	/** VR 포구 인정 반경 (cm) — 글러브(컨트롤러)와 급구 사이 거리. */
	UPROPERTY(EditAnywhere, Category = "Throw|VR")
	float FeedCatchRadius = 60.0f;

	/**
	 * 급구를 잡은 직후 송구 입력을 잠그는 시간 (초).
	 *
	 * 공을 잡으러 뻗던 손의 관성이 그대로 송구 동작으로 오인되는 것을 막는 장치다.
	 * ⚠️ 다만 **전환 시간(transfer)이 이 종목의 측정 지표**라, 이 값이 그대로
	 *    측정 가능한 최소 전환 시간의 바닥이 된다. 잠금이 길수록 "빠른 전환"을
	 *    아무리 잘해도 그 아래로는 기록될 수 없다 → 지표를 갉아먹지 않는 선까지만 잡는다.
	 *    (0.25 → 0.12. 목표 전환 시간이 1.2초이므로 바닥이 10% 에서 1% 로 내려간다.)
	 */
	UPROPERTY(EditAnywhere, Category = "Throw|VR", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float PostCatchThrowLockSec = 0.12f;

	// ── 설정값 ──

	/** 총 시행 수. */
	UPROPERTY(EditAnywhere, Category = "Throw")
	int32 TotalThrows = 10;

	/** 파워가 0→1 까지 차오르는 시간 (초). */
	UPROPERTY(EditAnywhere, Category = "Throw")
	float ChargeTime = 1.2f;

	/** 목표 zone 반경 (cm) — 이 안에 떨어지면 도달(명중). */
	UPROPERTY(EditAnywhere, Category = "Throw")
	float HitRadius = 200.0f;

	/**
	 * 베이스 배치 (폰 기준 로컬 오프셋: X=정면(홈 쪽), Y=오른쪽, 단위 cm).
	 *
	 * ⚠️ 실측 다이아몬드가 아니라 **정면 고정 시야에 네 베이스가 모두 들어오도록**
	 *    외야 중계 위치에서 본 다이아몬드를 약 0.7배로 줄인 연출 좌표다.
	 *    (시점이 +X 로 고정이라 뒤쪽 베이스는 존재해도 보이지 않는다.)
	 *    실측 스케일은 플레이 공간 확정 후 캘리브레이션 대상 — 하드코딩 확정 아님.
	 */
	UPROPERTY(EditAnywhere, Category = "Throw|Field")
	FVector2D FirstBaseOffset = FVector2D(2360.0f, 1360.0f);

	UPROPERTY(EditAnywhere, Category = "Throw|Field")
	FVector2D SecondBaseOffset = FVector2D(1000.0f, 0.0f);

	UPROPERTY(EditAnywhere, Category = "Throw|Field")
	FVector2D ThirdBaseOffset = FVector2D(2360.0f, -1360.0f);

	UPROPERTY(EditAnywhere, Category = "Throw|Field")
	FVector2D HomePlateOffset = FVector2D(3720.0f, 0.0f);

	/** 파워 1.0 이 도달시키는 최대 수평 거리 (cm). 거리→파워 역산 기준. */
	UPROPERTY(EditAnywhere, Category = "Throw")
	float MaxThrowRange = 5000.0f;

	/** 한 구 사이 간격 (초). */
	UPROPERTY(EditAnywhere, Category = "Throw")
	float IntervalBetweenThrows = 1.5f;

	/**
	 * 첫 시행까지의 대기 (초).
	 *
	 * ⚠️ 0 으로 두면 안 된다: VR 은 **BeginPlay 시점에 HMD 포즈가 카메라에 아직 안 들어와**
	 *    플레이어가 어디를 보는지 알 수 없다. 그 상태로 그라운드를 깔면 베이스와 급구가
	 *    엉뚱한 방향(월드 +X)에 생겨 "공이 왜 저기서 날아오지"가 된다.
	 */
	UPROPERTY(EditAnywhere, Category = "Throw", meta = (ClampMin = "0.2"))
	float FirstTrialDelaySec = 1.5f;

	// ── 급구(feed) 설정 — 전환 시간 측정을 위한 "잡는 공" ──
	/** 급구가 출발하는 정면 거리 (cm). */
	UPROPERTY(EditAnywhere, Category = "Throw|Feed")
	float FeedDistance = 900.0f;

	/** 급구 체공시간 (초). 짧을수록 급하게 잡아야 한다. */
	UPROPERTY(EditAnywhere, Category = "Throw|Feed", meta = (ClampMin = "0.3"))
	float FeedFlightSec = 1.1f;

	/** 급구 포구 판정 창 (±초). PC 모드에서 Space 타이밍 허용 오차. */
	UPROPERTY(EditAnywhere, Category = "Throw|Feed")
	float FeedCatchWindowSec = 0.35f;

	// ── 코칭 기준값 (⚠️ 실측 캘리브레이션 대상 — 하드코딩 확정 금지) ──
	/** 이 구속(km/h)이면 구속 만점. */
	UPROPERTY(EditAnywhere, Category = "Throw|Scoring", meta = (ClampMin = "10.0"))
	float TargetReleaseKmh = 90.0f;

	/** 이 전환 시간(초) 이하면 전환 만점. VR 컨트롤러 조작이라 실전보다 관대하게 잡았다. */
	UPROPERTY(EditAnywhere, Category = "Throw|Scoring", meta = (ClampMin = "0.2"))
	float TargetTransferSec = 1.2f;

	/** 공 액터 클래스. 미지정 시 ACatchBall 기본 사용. */
	UPROPERTY(EditAnywhere, Category = "Throw")
	TSubclassOf<ACatchBall> BallClass;

private:
	static constexpr int32 NumBases = 4;

	// ── 입력 핸들러 ──
	void OnSpacePressed();     // Feed=포구 시도 / Ready=충전 시작
	void OnSpaceReleased();    // Ready=발사
	void ReturnToModeSelect(); // M

	// ── 세션 진행 ──
	void StartSession();
	void SpawnNextTrial();     // 목표 베이스 지정 + 급구 발사
	void CatchFeed(bool bClean);
	void ThrowBall(float Power);
	void FinishThrow(const FThrowResult& Result);
	void EndSession();

	/** 파워(0~1) → 발사 속도 벡터. 방향은 목표 베이스 자동 조준. */
	FVector PowerToVelocity(float Power) const;

	/** 타겟 거리를 정확히 맞히는 정답 파워(0~1). */
	float DistanceToIdealPower(float Distance) const;

	/** 베이스 → 월드 위치 (그라운드 기준점·정면 방향 적용). */
	FVector BaseLocation(EBaseType Base) const;

	/**
	 * 그라운드(베이스 배치·급구 방향)의 기준점과 정면을 **한 번만** 확정한다.
	 *
	 * ⚠️ 예전엔 월드 +X 를 정면으로 고정했다. VR 플레이어는 룸 안에서 아무 방향이나 보고 서
	 *    있으므로, 그 가정이 깨지면 베이스가 등 뒤에 깔리고 급구가 옆에서 스쳐 지나간다
	 *    ("공이 왜 날아오는지 모르겠다"의 실제 원인).
	 *
	 * ⚠️ **매 시행 다시 잡으면 안 된다.** 그라운드는 고정된 지형이라, 고개를 돌릴 때마다
	 *    베이스가 따라 돌면 어디로 던지는지 감각이 사라진다. 첫 시행에서 한 번만 잡는다.
	 */
	void EnsureFieldAnchor();

	/**
	 * 플레이어가 서 있는 **바닥면 Z** (월드).
	 * VR: 트래킹 원점이 Stage 라 바닥 = 폰 루트 Z. PC: 루트가 몸 중심이라 바닥 = 루트 - 캡슐 반높이.
	 */
	float FloorZ() const;

	/** 급구가 도착하는 높이(=글러브가 닿는 가슴 높이, 월드 Z). */
	float CatchHeightZ() const;

	/** 송구가 출발하는 손 높이 (월드 Z). */
	float ThrowHandZ() const;

	/** 송구 궤적 예측선을 그린다 (지금 파워로 던지면 어디로 가는지). */
	void DrawPredictedArc(float Power) const;

	/** EBaseType → 집계 배열 인덱스. */
	static int32 BaseIndexOf(EBaseType Base);

	// ── AI 운동 추천 ──
	/** 세션 송구 결과들 → 약점(정확도·구속·전환) 리포트 (결정론적). */
	FWeaknessReport BuildThrowReport() const;

	/** 리포트 → 드릴 추천 + AI 코칭 요청 (세션 종료 시 1회). */
	void RequestThrowFeedback();

	/** 진행 중인 세션을 저장 슬롯에 확정한다 (모드 복귀·앱 종료 시). */
	void FlushSessionToSave();

	UFUNCTION()
	void HandleCoachingReady(bool bSuccess, const FString& Text);

	// ── 상태 ──
	UPROPERTY(Transient)
	TObjectPtr<ACatchBall> ActiveBall;   // 급구 or 송구 공 (단계에 따라 역할이 다르다)

	FThrowTrial CurrentTrial;
	FVector HomeLocation = FVector::ZeroVector;

	// ── 그라운드 기준 (EnsureFieldAnchor 가 첫 시행에 한 번 잡는다) ──
	/** 내가 서 있는 자리(월드 XY, Z=바닥) — 베이스·급구가 여기를 중심으로 배치된다. */
	FVector FieldAnchor = FVector::ZeroVector;
	/** 내 정면(월드 yaw, 도). VR 은 HMD 방향, PC 는 폰 정면(+X). */
	float   FieldYawDeg = 0.0f;
	bool    bFieldAnchored = false;

	EThrowPhase Phase = EThrowPhase::Feed;

	int32 ThrowIndex = 0;
	int32 SuccessCount = 0;

	bool  bCharging = false;    // 스페이스바 홀드 중
	float CurrentPower = 0.0f;  // 0~1

	bool  bSessionOver = false;

	bool  bWaitingNext = false;
	float IntervalTimer = 0.0f;

	// 전환 시간 측정 — 잡은 순간과 정상 포구 여부.
	float CatchTimeSec = -1.0f;   // 월드 시간 기준
	bool  bCleanCatch = false;

	/**
	 * 날아가는 동안 매 틱 갱신하는 공 위치.
	 * ACatchBall 은 자기 GroundZ(기본 0) 에 닿으면 그 자리에 멈췄다가 스스로 사라진다 —
	 * 폰의 바닥면이 그보다 낮으면 착지 감지 전에 액터가 없어질 수 있어, 마지막 위치를 들고 있다가 판정한다.
	 */
	FVector LastBallLoc = FVector::ZeroVector;

	/** 릴리스 시점에 확정되는 값 (착지 판정 때 결과에 합친다). */
	float PendingReleaseSpeedCms = 0.0f;
	float PendingTransferSec = -1.0f;
	float PendingPower = 0.0f;

	FThrowResult LastResult;
	bool bHasResult = false;

	// 세션 누적 (측정 지표 집계).
	TArray<FThrowResult> SessionResults;
	int32 BaseAttempts[NumBases] = { 0, 0, 0, 0 };
	int32 BaseSuccess[NumBases]  = { 0, 0, 0, 0 };

	// ── AI 운동 추천 상태 ──
	UPROPERTY(Transient)
	TObjectPtr<UAIFeedbackService> FeedbackService;

	TArray<FTrainingDrill> LastDrills;
	FString CoachingText;
	bool bAwaitingCoaching = false;

	// ── VR 상태 ──
	bool    bVR = false;
	FVector PrevControllerLoc = FVector::ZeroVector;
	bool    bHasPrevControllerLoc = false;
	float   ThrowCooldown = 0.0f;   // 던진 직후 재던짐 방지

	/**
	 * 송구 손 컨트롤러 추적이 끊겼는지 (TickVRThrow 가 매 프레임 갱신).
	 *
	 * 추적이 끊기면 위치가 고정돼 손 속도가 0 으로 잡히고, 아무리 던져도 조용히 무시된다.
	 * 화면에 아무 표시가 없으면 플레이어는 "게임이 고장났다"고 판단하므로 패널에 알린다.
	 */
	bool    bControllerLost = false;

	// 던지기 동작 추적 — 피크 속도에서 릴리스를 잡기 위한 상태 (TickVRThrow).
	bool  bThrowMotionActive = false;  // 트리거 속도를 넘어 동작이 시작됨
	float ThrowPeakSpeedCms  = 0.0f;   // 동작 중 관측한 최고 손 속도
	float ThrowMotionSec     = 0.0f;   // 동작이 시작된 뒤 흐른 시간

	/** VR: 컨트롤러 속도로 송구 인식 + 파워 산출. */
	void TickVRThrow(float DeltaSeconds);

	/** VR: 글러브(컨트롤러) 근접으로 급구 포구. */
	void TickVRFeedCatch();

	/** VR 상태 패널 내용 갱신 (bVR 일 때 매 틱). */
	void RefreshVrPanel();

	/** VR '컨트롤러 위로 들어 나가기' 제스처 상태 (헤드셋만으로 모드 선택 복귀). */
	FVRExitGesture ExitGesture;
};
