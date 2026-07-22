#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ViveBringupPawn.generated.h"

class UCameraComponent;
class USceneComponent;
class ABat;

/**
 * Vive 브링업 진단 하네스 (Phase 2 착수용).
 *
 * 목적은 게임플레이가 아니라 **한 가지 질문에 답하는 것**:
 *   "SteamVR → OpenXR → UE → ABat → UViveMotionInputProvider → USwingAnalyzer
 *    사슬 중 어디까지 살아 있는가?"
 *
 * 설계 원칙 — **진단 전용 우회로를 만들지 않는다.**
 * 이 폰은 직접 컨트롤러를 읽지 않고 실제 ABat 을 스폰해 그 provider 를 관찰한다.
 * 별도 경로로 값을 읽으면 "진단은 되는데 본 경로는 안 되는" 상황을 놓친다.
 * 속도도 직접 계산하지 않고 USwingAnalyzer 를 호출한다 — 계산 계층까지 함께 검증된다.
 *
 * 화면에는 **원시 값을 전부 그대로** 띄우고, 그 아래에 "지금 상태에서 다음에 뭘
 * 확인해야 하는지"를 한 줄로 제시한다. 브링업은 값을 보는 작업이 아니라
 * 실패 지점을 좁히는 작업이다.
 *
 * 조작: [H] 좌/우손 전환 · [R] 최고속도 리셋 · [M] 모드 선택 복귀
 *
 * ⚠️ 이 폰 자체는 검증되지 않은 코드다. 다만 성격이 다르다 — 하드웨어에 붙는
 *    즉시 실행해 진단하는 것이 존재 목적이므로, 검증 없이 쌓이는 기능 코드와 달리
 *    첫 실행에서 바로 참/거짓이 드러난다.
 */
UCLASS()
class MOTIONBASE_API AViveBringupPawn : public APawn
{
	GENERATED_BODY()

public:
	AViveBringupPawn();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, Category = "Bringup")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Bringup")
	TObjectPtr<UCameraComponent> Camera;

	/** 관찰 대상. BeginPlay 에서 ViveController 소스로 스폰한다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Bringup")
	TObjectPtr<ABat> Bat;

	void ToggleHand();
	void ResetPeak();
	void ReturnToModeSelect();

private:
	/** 현재 상태에서 다음에 확인할 것 한 줄. 실패 지점을 위에서부터 좁힌다. */
	FString BuildDiagnosis(bool bHmdConnected, bool bHmdEnabled,
		bool bProviderOk, bool bTracking, int32 SampleCount) const;

	/** 세션 중 관측한 최고 배트 헤드 속도 (m/s). */
	float SessionPeakMps = 0.0f;

	/** BatTip 이 실제로 움직였는지 — 위치가 고정이면 트래킹이 죽은 것. */
	FVector FirstTipLocation = FVector::ZeroVector;
	float MaxTipDriftCm = 0.0f;
	bool bHasFirstTip = false;
};
