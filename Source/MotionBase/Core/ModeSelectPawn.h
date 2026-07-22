#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Data/MotionBaseTypes.h"
#include "ModeSelectPawn.generated.h"

class UCameraComponent;
class USceneComponent;

/**
 * 시작 화면(모드 선택) 폰.
 *
 * 게임 시작 시 AMotionBaseGameMode 의 DefaultPawnClass 로 빙의되어,
 * 6개 모드 목록을 보여주고 선택을 받는다. 그리기는 AModeSelectHUD 가 담당하고
 * 이 폰은 **선택 상태만** 들고 있다 (입력·상태 / 렌더 분리).
 *
 * 확정하면 GameMode 가 해당 모드의 폰으로 교체한다 — 레벨 이동 없음.
 *
 * ⚠️ **키보드 전용 — PC 개발/시연용이다. 최종 조작 방식이 아니다.**
 *    이 제품은 비착용형(Non-HMD)이라 실제 플레이어에겐 키보드가 없다:
 *    배트(Vive 컨트롤러)를 쥐고 있거나, LiDAR 전신 추적만 쓴다.
 *
 *    지금 키보드인 이유는 프로젝트에 **버튼 입력 경로 자체가 아직 없기 때문**이다
 *    (EnhancedInput 은 의존성에만 있고 미사용, 스윙 트리거도 미배선 — ROADMAP Phase 2).
 *    SteamVR/OpenXR 을 올려 기존 ABat/UViveMotionInputProvider 경로를 실기로 검증하기
 *    전까지는 하드웨어 입력을 추가하지 않는다. 검증 못 하는 Vive 코드를 한 겹 더 쌓으면
 *    나중에 어느 계층이 틀렸는지 분리할 수 없게 된다.
 *
 *    목표 조작(Phase 2 이후): 드웰 선택 — 배트로 카드를 겨누고 잠시 유지하면 선택.
 *    버튼이 필요 없어 컨트롤러 포즈로도 LiDAR 위치로도 같은 코드가 돌아간다.
 *
 * ⚠️ UMG 에셋 없이 동작하도록 레거시 BindKey + Canvas HUD 로 구성했다.
 *    정식 UI 는 Phase 1 의 UMG HUD 작업에서 교체 예정. 투사 환경(바닥·공간)에서는
 *    현재 타이포/행 높이가 작다 — 화면 디자인도 그때 함께 다시 잡을 것.
 */
UCLASS()
class MOTIONBASE_API AModeSelectPawn : public APawn
{
	GENERATED_BODY()

public:
	AModeSelectPawn();

	virtual void Tick(float DeltaSeconds) override;

	/** 메뉴에 표시할 모드 목록 (HUD 가 읽는다). */
	const TArray<EGameModeId>& GetMenuModes() const { return MenuModes; }

	/** 현재 커서 위치. */
	int32 GetSelectedIndex() const { return SelectedIndex; }

	EGameModeId GetSelectedMode() const;

	/**
	 * 미구현 모드를 고르려 했을 때 남는 안내 문구.
	 * 비어 있으면 표시하지 않는다.
	 */
	const FString& GetNoticeText() const { return NoticeText; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, Category = "ModeSelect")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "ModeSelect")
	TObjectPtr<UCameraComponent> Camera;

	/** 안내 문구가 화면에 남아 있는 시간 (초). */
	UPROPERTY(EditAnywhere, Category = "ModeSelect")
	float NoticeDurationSec = 2.0f;

	// 입력 핸들러 (BindKey 는 인자 없는 멤버 함수만 받는다)
	void SelectPrev();
	void SelectNext();
	void Confirm();

	/** Vive 브링업 진단 하네스 열기 (개발 도구 — 모드 목록에 없다). */
	void OpenViveBringup();

private:
	void MoveSelection(int32 Delta);

	/** 커서를 잡아둘 기본 위치 — 첫 번째 "플레이 가능한" 모드. */
	int32 FindFirstImplementedIndex() const;

	TArray<EGameModeId> MenuModes;
	int32 SelectedIndex = 0;

	FString NoticeText;
	float NoticeTimer = 0.0f;
};
