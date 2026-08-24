#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Core/Defense/CatchBall/CatchBallTypes.h"
#include "UI/VRExitGesture.h"
#include "CatchBallPawn.generated.h"

class UCameraComponent;
class UCapsuleComponent;
class UMotionControllerComponent;
class UStaticMeshComponent;
class UVRInfoPanel;
class ACatchBall;

/**
 * 포구 훈련 폰 (1인칭).
 *
 * 흐름: 공 발사 → 낙구지점 마커 표시 → WASD 로 이동 → 스페이스바로 포구 시도
 *       → 판정(FCatchBallJudge) → 10구 반복 → "N / 10" 집계.
 *
 * 공 궤적은 ACatchBall(ProjectileMovement)이 굴리고, 낙구지점·도달시간 예측은
 * 발사 속도로부터 이 폰이 포물선 공식으로 계산한다. 판정은 FCatchBallJudge 가 한다.
 *
 * 지금은 키보드 Mock 이다. LiDAR 붙으면 이동 입력만 실제 몸 위치로 교체한다.
 */
UCLASS()
class MOTIONBASE_API ACatchBallPawn : public APawn
{
	GENERATED_BODY()

public:
	ACatchBallPawn();

	virtual void Tick(float DeltaSeconds) override;

	// ── HUD 가 읽는 상태 접근자 ──
	ECatchBallType GetSessionType() const { return SessionType; }
	int32 GetTotalPitches() const { return TotalPitches; }
	int32 GetSuccessCount() const { return SuccessCount; }
	int32 GetPitchNumber() const { return FMath::Min(PitchIndex + 1, TotalPitches); } // 1-based 표시용

	/** 마지막 판정 결과 문구/색. 표시할 게 있으면 true. */
	bool GetLastOutcomeText(FString& OutText, FLinearColor& OutColor) const;
	
protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, Category = "CatchBall")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, Category = "CatchBall")
	TObjectPtr<UCameraComponent> Camera;

	/** VR 글러브 = 오른손 컨트롤러. 공에 가까이 가져가면 포구된다. */
	UPROPERTY(VisibleAnywhere, Category = "CatchBall|VR")
	TObjectPtr<UMotionControllerComponent> GloveController;

	/** 글러브 시각 표시 (손 위치 구체). */
	UPROPERTY(VisibleAnywhere, Category = "CatchBall|VR")
	TObjectPtr<UStaticMeshComponent> GloveMesh;

	/** VR 헤드셋 안 상태 패널 (진행·결과·안내). 월드 고정. PC 모드에선 숨김. */
	UPROPERTY(VisibleAnywhere, Category = "CatchBall|VR")
	TObjectPtr<UVRInfoPanel> VrPanel;

	// ── 설정값 ──

	/** 이 세션에서 던질 타구 유형. Mixed 면 매 구 랜덤. */
	UPROPERTY(EditAnywhere, Category = "CatchBall")
	ECatchBallType SessionType = ECatchBallType::Mixed;

	/** 총 시행 수. */
	UPROPERTY(EditAnywhere, Category = "CatchBall")
	int32 TotalPitches = 10;

	/** 플레이어 좌우/앞뒤 이동 속도 (cm/s). */
	UPROPERTY(EditAnywhere, Category = "CatchBall")
	float MoveSpeed = 500.0f;

	/** 타이밍 허용 오차 (±초). */
	UPROPERTY(EditAnywhere, Category = "CatchBall")
	float TimingTolerance = 0.25f;

	/** 한 구 사이 간격 (초) — 결과 보여주고 다음 공 준비. */
	UPROPERTY(EditAnywhere, Category = "CatchBall")
	float IntervalBetweenPitches = 1.5f;

	/** 공을 던지는 정면 거리 (cm) — 플레이어 앞쪽. */
	UPROPERTY(EditAnywhere, Category = "CatchBall")
	float PitchDistance = 2000.0f;

	/** 발사 지점 높이 (cm, 플레이어 기준). */
	UPROPERTY(EditAnywhere, Category = "CatchBall")
	float PitchHeight = 180.0f;

	/** 낙구지점이 좌우로 퍼지는 최대 폭 (cm). 이동해서 잡게 만든다. */
	UPROPERTY(EditAnywhere, Category = "CatchBall")
	float SideSpread = 500.0f;

	// ── 유형별 포구 허용 반경 (cm) — 작을수록 정밀하게 잡아야 한다 ──
	// ⚠️ 실측 캘리브레이션 대상 (CLAUDE §규칙). 에디터에서 바로 튜닝.
	/** 땅볼: 낮고 빠름, 그나마 관대. */
	UPROPERTY(EditAnywhere, Category = "CatchBall|Radius", meta = (ClampMin = "10.0"))
	float GroundBallCatchRadius = 90.0f;

	/** 뜬공: 높이 뜨고 체공이 김. */
	UPROPERTY(EditAnywhere, Category = "CatchBall|Radius", meta = (ClampMin = "10.0"))
	float FlyBallCatchRadius = 75.0f;

	/** 라인드라이브: 빠르고 빡셈. */
	UPROPERTY(EditAnywhere, Category = "CatchBall|Radius", meta = (ClampMin = "10.0"))
	float LineDriveCatchRadius = 55.0f;

	// ── 유형별 체공시간 (초) — 짧을수록 공이 빨라 반응이 빡세다 ──
	// ⚠️ 실측 캘리브레이션 대상. 발사 속도를 이 시간으로 역산하므로 0 이면 안 된다.
	/** 땅볼: 낮고 빠름. */
	UPROPERTY(EditAnywhere, Category = "CatchBall|Flight", meta = (ClampMin = "0.2"))
	float GroundBallFlightSec = 1.2f;

	/** 뜬공: 높이 뜨고 체공이 김. */
	UPROPERTY(EditAnywhere, Category = "CatchBall|Flight", meta = (ClampMin = "0.2"))
	float FlyBallFlightSec = 2.4f;

	/** 라인드라이브: 빠르고 낮게 쏘아온다. */
	UPROPERTY(EditAnywhere, Category = "CatchBall|Flight", meta = (ClampMin = "0.2"))
	float LineDriveFlightSec = 1.0f;

	/** 공 액터 클래스. 미지정 시 ACatchBall 기본 사용. */
	UPROPERTY(EditAnywhere, Category = "CatchBall")
	TSubclassOf<ACatchBall> CatchBallClass;

private:
	// ── 입력 핸들러 (BindKey 눌림/뗌 → 플래그) ──
	void OnRightPressed()  { bMoveRight = true; }
	void OnRightReleased() { bMoveRight = false; }
	void OnLeftPressed()   { bMoveLeft = true; }
	void OnLeftReleased()  { bMoveLeft = false; }
	void OnFwdPressed()    { bMoveFwd = true; }
	void OnFwdReleased()   { bMoveFwd = false; }
	void OnBackPressed()   { bMoveBack = true; }
	void OnBackReleased()  { bMoveBack = false; }
	void OnCatchPressed();           // Space
	void ReturnToModeSelect();       // M

	// 타구 유형 선택 (숫자 1~4)
	void SelectGround();   // 1 땅볼
	void SelectFly();      // 2 뜬공
	void SelectLine();     // 3 라인드라이브
	void SelectRandom();   // 4 랜덤

	// ── 세션 진행 ──
	void StartSession();
	void SpawnNextPitch();
	void FinishPitch(const FCatchResult& Result);
	void EndSession();

	/** 유형에 맞는 발사 파라미터(속도) + 예측(낙구지점·도달시간)을 채운다. */
	FCatchTrial BuildTrial(ECatchBallType Type) const;

	/** Mixed → 실제 셋 중 하나로 확정. */
	ECatchBallType ResolveType(ECatchBallType Type) const;

	// ── 상태 ──
	UPROPERTY(Transient)
	TObjectPtr<ACatchBall> ActiveBall;

	FCatchTrial CurrentTrial;

	/** 세션 시작 시점의 플레이어 위치 — 모든 발사·낙구지점의 기준. */
	FVector HomeLocation = FVector::ZeroVector;

	// 이동 입력 플래그 (BindKey 눌림 상태 유지용)
	bool bMoveRight = false;
	bool bMoveLeft  = false;
	bool bMoveFwd   = false;
	bool bMoveBack  = false;

	int32 PitchIndex = 0;    // 현재 지 시행 번호 (0-based)
	int32 SuccessCount = 0;

	bool  bPitchActive = false;   // 공이 날아가는 중 (스페이스바 대기)
	bool  bSessionOver = false;
	float IntervalTimer = 0.0f;   // 다음 공까지 대기 타이머
	bool  bWaitingNext = false;

	FCatchResult LastResult;
	FString StatusLine;   // 화면 하단 상태 문구

	/** HMD 연결 시 true — 글러브(컨트롤러) 근접으로 포구, 이동은 실제 몸으로. */
	bool bVR = false;

	/** VR 포구 판정 — 글러브가 공에 닿았는지 매 틱 확인. */
	void TickVRCatch();

	/** VR 상태 패널 내용 갱신 (bVR 일 때 매 틱). */
	void RefreshVrPanel();

	/** VR '글러브 위로 들어 나가기' 제스처 상태 (헤드셋만으로 모드 선택 복귀). */
	FVRExitGesture ExitGesture;
};