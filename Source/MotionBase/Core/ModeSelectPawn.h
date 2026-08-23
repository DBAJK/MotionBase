#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Data/MotionBaseTypes.h"
#include "ModeSelectPawn.generated.h"

class UCameraComponent;
class USceneComponent;

/**
 * 시작 화면(모드 선택) 폰. **2단계 선택**: 모드 → 난이도.
 *
 *   [모드 단계]  6개 모드 목록 → 타격 확정 → [난이도 단계] 로 진입
 *   [난이도 단계] 초보/아마추어/프로 → 확정 → GameMode 가 폰 교체
 *   Back 키로 난이도 단계에서 모드 단계로 되돌아간다.
 *
 * 폰은 **선택 상태만** 들고, 그리기는 AModeSelectHUD 가 한다. HUD 가 모드/난이도를
 * 구분하지 않도록, 폰이 "현재 단계의 행 데이터"를 일반 접근자(GetRow*)로 넘긴다 —
 * 같은 렌더러가 두 단계를 모두 그린다 (입력·상태 / 렌더 분리).
 *
 * ⚠️ **키보드 전용 — PC 개발/시연용이다. 최종 조작 방식이 아니다.**
 *    비착용형(Non-HMD)이라 실제 플레이어에겐 키보드가 없다. 목표 조작(Phase 2 이후)은
 *    드웰 선택 — 배트로 카드를 겨누고 잠시 유지하면 선택. 지금 키보드인 이유는
 *    프로젝트에 버튼 입력 경로 자체가 아직 없기 때문(ROADMAP Phase 2).
 *
 * ⚠️ UMG 에셋 없이 동작하도록 레거시 BindKey + Canvas HUD 로 구성. 정식 UI 는 Phase 1
 *    UMG HUD 작업에서 교체 예정 (투사 환경 가독성 기준으로 재설계).
 */
UCLASS()
class MOTIONBASE_API AModeSelectPawn : public APawn
{
	GENERATED_BODY()

public:
	AModeSelectPawn();

	virtual void Tick(float DeltaSeconds) override;

	/** 현재 커서 위치 (현재 단계 기준). */
	int32 GetSelectedIndex() const { return SelectedIndex; }

	// ── HUD 가 읽는 일반 행 데이터 (모드/난이도 단계 공통) ──

	/** 현재 단계 행 개수. */
	int32 GetRowCount() const;

	/** 행 라벨 (모드 이름 또는 난이도 이름). */
	FText GetRowLabel(int32 Index) const;

	/** 이 행이 선택 가능한지 (모드 단계: 구현됨?; 난이도 단계: 항상 true). */
	bool IsRowAvailable(int32 Index) const;

	/** 오른쪽 상태 태그. 비어 있으면 태그를 그리지 않는다 (난이도 행은 태그 없음). */
	FText GetRowTag(int32 Index) const;

	/** 헤더 부제 (단계에 따라 달라진다). */
	FText GetHeaderSubtitle() const;

	/** 현재 선택 항목 설명. */
	FText GetSelectedDescription() const;

	/** 푸터 우측 현황 문구. */
	FText GetFooterStatus() const;

	/** 안내 문구(미구현 모드 선택 시). 비어 있으면 표시 안 함. */
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
	void Back();

	/** Vive 브링업 진단 하네스 열기 (개발 도구 — 목록에 없다). */
	void OpenViveBringup();

private:
	/** 선택 단계. 타격 모드만 Stance 단계를 거친다 (그 외는 난이도에서 바로 시작). */
	enum class EStage : uint8 { Mode, Difficulty, Stance };

	void MoveSelection(int32 Delta);

	/** 커서를 잡아둘 기본 위치 — 모드 단계는 첫 구현 모드, 난이도 단계는 아마추어. */
	int32 FindFirstImplementedIndex() const;

	EGameModeId ModeAt(int32 Index) const;
	EDifficultyLevel DifficultyAt(int32 Index) const;
	EBattingStance StanceAt(int32 Index) const;

	/** 난이도 확정 후: 타격이면 Stance 단계로, 아니면 바로 시작. */
	void ConfirmDifficulty();

	EStage Stage = EStage::Mode;

	TArray<EGameModeId> MenuModes;
	TArray<EDifficultyLevel> MenuDifficulties;
	TArray<EBattingStance> MenuStances;

	int32 SelectedIndex = 0;

	/** 모드 단계에서 확정한 모드 (이후 단계에서 사용). */
	EGameModeId PendingMode = EGameModeId::Batting;

	/** 난이도 단계에서 확정한 난이도 (스탠스 단계에서 사용). */
	EDifficultyLevel PendingDifficulty = EDifficultyLevel::Amateur;

	FString NoticeText;
	float NoticeTimer = 0.0f;
};
