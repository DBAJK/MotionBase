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
	/** 이 속도(cm/s) 이상으로 컨트롤러를 휘두르면 송구로 인식. */
	UPROPERTY(EditAnywhere, Category = "Throw|VR")
	float ThrowTriggerSpeedCms = 250.0f;

	/** 파워 0 에 대응하는 손 속도 (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Throw|VR")
	float MinThrowSpeedCms = 250.0f;

	/** 파워 1 에 대응하는 손 속도 (cm/s). 이 이상은 최대 파워. */
	UPROPERTY(EditAnywhere, Category = "Throw|VR")
	float MaxThrowSpeedCms = 1500.0f;

	/** VR 포구 인정 반경 (cm) — 글러브(컨트롤러)와 급구 사이 거리. */
	UPROPERTY(EditAnywhere, Category = "Throw|VR")
	float FeedCatchRadius = 60.0f;

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

	/** 베이스 → 월드 위치 (폰 기준 오프셋 적용). */
	FVector BaseLocation(EBaseType Base) const;

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

	/** VR: 컨트롤러 속도로 송구 인식 + 파워 산출. */
	void TickVRThrow(float DeltaSeconds);

	/** VR: 글러브(컨트롤러) 근접으로 급구 포구. */
	void TickVRFeedCatch();

	/** VR 상태 패널 내용 갱신 (bVR 일 때 매 틱). */
	void RefreshVrPanel();

	/** VR '컨트롤러 위로 들어 나가기' 제스처 상태 (헤드셋만으로 모드 선택 복귀). */
	FVRExitGesture ExitGesture;
};
