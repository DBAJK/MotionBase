#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Data/MotionBaseTypes.h"
#include "Data/OverallScore.h"
#include "ModeSelectPawn.generated.h"

class UCameraComponent;
class USceneComponent;
class UMotionControllerComponent;
class UTextRenderComponent;
class UVRInfoPanel;

/**
 * 시작 화면(모드 선택) 폰. **단계형 선택**:
 *
 *   [모드 단계]   모드 목록 → 확정
 *                  · 타격  → [난이도] → [타석(좌타/우타)] → 시작
 *                  · 수비  → [수비 세부 종목] → 시작
 *                  · 그 외 → [난이도] → 시작
 *   Back 키로 한 단계씩 되돌아간다.
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

	// ── HUD 가 읽는 일반 행 데이터 (모드/난이도/타석/수비종목 단계 공통) ──

	/** 현재 단계 행 개수. */
	int32 GetRowCount() const;

	/** 행 라벨 (모드/난이도/타석/수비종목 이름). */
	FText GetRowLabel(int32 Index) const;

	/** 이 행이 선택 가능한지 (모드 단계: 구현됨?; 그 외 단계: 항상 true). */
	bool IsRowAvailable(int32 Index) const;

	/** 오른쪽 상태 태그. 비어 있으면 태그를 그리지 않는다 (모드 단계 외에는 태그 없음). */
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

	// ── VR 인메뉴 (헤드셋 안 3D 메뉴 + 컨트롤러 드웰 선택) ──
	// HMD 가 켜져 있을 때만 활성화. 없을 땐 기존 키보드 + 평면 HUD 로 동작.

	/** 겨눔 포인터로 쓰는 컨트롤러 (기본 오른손). */
	UPROPERTY(VisibleAnywhere, Category = "ModeSelect|VR")
	TObjectPtr<UMotionControllerComponent> PointerController;

	/**
	 * VR 3D 패널 (제목·행 카드·뒤로·설명·힌트). SceneRoot 에 붙여 **월드 고정**.
	 * HMD 없으면 숨긴다. 드웰 겨눔 판정은 패널의 GetRowText/GetBackText 를 쓴다.
	 */
	UPROPERTY(VisibleAnywhere, Category = "ModeSelect|VR")
	TObjectPtr<UVRInfoPanel> VrPanel;

	/** 카드를 이 시간(초)만큼 계속 겨누고 있으면 선택 확정 (트리거를 안 쓸 때의 폴백). */
	UPROPERTY(EditAnywhere, Category = "ModeSelect|VR")
	float DwellTimeSec = 1.5f;

	/** 컨트롤러 트리거(아래쪽 검지 버튼)를 이 값 이상 당기면 눌림으로 본다. */
	UPROPERTY(EditAnywhere, Category = "ModeSelect|VR", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float TriggerPressThreshold = 0.6f;

	/** 이 각도(도) 안쪽으로 겨누면 그 카드에 호버된 것으로 본다. */
	UPROPERTY(EditAnywhere, Category = "ModeSelect|VR")
	float DwellAngleDeg = 8.0f;

	/** 메뉴를 플레이어 앞 몇 cm 에 띄울지. (너무 가까우면 카드가 커 보여 부담 → 3.2m) */
	UPROPERTY(EditAnywhere, Category = "ModeSelect|VR")
	float MenuDistanceCm = 320.0f;

	/** 메뉴 중심 높이 (cm, 바닥 기준). 눈높이쯤에 두면 자연스럽다. */
	UPROPERTY(EditAnywhere, Category = "ModeSelect|VR")
	float MenuHeightCm = 150.0f;

	/**
	 * 메뉴가 시야 중심에서 이 각도(도) 밖으로 벗어나면 천천히 따라온다 (body-locked + 데드존).
	 *
	 * 완전 월드 고정은 스테이지 트래킹에서 플레이어가 몸을 틀면 메뉴가 등 뒤로 사라진다.
	 * 반대로 머리에 붙이면(헤드락) 고개를 돌려도 안 움직여 멀미가 난다.
	 * 그래서 **데드존 안에서는 고정, 밖으로 나가면 경계까지만 느리게 끌려온다** — VR UI 의 표준 절충.
	 * 0 이면 항상 정면 추종, 180 이면 완전 고정.
	 */
	UPROPERTY(EditAnywhere, Category = "ModeSelect|VR", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MenuFollowDeadzoneDeg = 35.0f;

	/** 따라오는 속도 (1/초). 낮을수록 느긋하다 — 급히 따라오면 그 자체로 멀미를 유발한다. */
	UPROPERTY(EditAnywhere, Category = "ModeSelect|VR", meta = (ClampMin = "0.1"))
	float MenuFollowSpeed = 1.6f;

	/** 곡면(원통) 배치 사용 — 위/아래 카드가 눈을 바라보게 기울어진다. */
	UPROPERTY(EditAnywhere, Category = "ModeSelect|VR")
	bool bCurvedMenu = true;

	// 입력 핸들러 (BindKey 는 인자 없는 멤버 함수만 받는다)
	void SelectPrev();
	void SelectNext();
	void Confirm();
	void Back();   // 한 단계 뒤로 (타석→난이도→모드, 수비종목→모드)

	/** Vive 브링업 진단 하네스 열기 (개발 도구 — 목록에 없다). */
	void OpenViveBringup();

private:
	/**
	 * 선택 단계.
	 *   타격 → Difficulty → Stance, 수비 → DefenseDrill,
	 *     └ 백업 위치 판단(DefenseDrill 의 index 2) 선택 시 → DefensePositionGroup → DefensePosition,
	 *   그 외 → Difficulty 에서 바로 시작.
	 *
	 * DefensePositionGroup 을 따로 두는 이유: 7개 포지션을 한 목록에 다 넣으면
	 * `UVRInfoPanel::MaxRows`(6)를 넘는다. 패널 레이아웃 상수를 건드리는 대신
	 * 내야(4)/외야(3) 두 그룹으로 나눠 매 단계 4행 이내로 유지한다 (설계 노트 참고).
	 */
	enum class EStage : uint8 { Mode, Difficulty, Stance, DefenseDrill, DefensePositionGroup, DefensePosition };

	void MoveSelection(int32 Delta);

	/** 커서를 잡아둘 기본 위치 — 모드 단계는 첫 구현 모드, 난이도 단계는 아마추어. */
	int32 FindFirstImplementedIndex() const;

	EGameModeId ModeAt(int32 Index) const;
	EDifficultyLevel DifficultyAt(int32 Index) const;
	EBattingStance StanceAt(int32 Index) const;

	/** 수비 세부 종목 이름/설명 (인덱스 안전). */
	FText DefenseDrillNameAt(int32 Index) const;
	FText DefenseDrillDescAt(int32 Index) const;

	/** 백업 위치 판단 — 포지션 그룹(0=내야/1=외야) 이름·설명. */
	FText PositionGroupNameAt(int32 Index) const;
	FText PositionGroupDescAt(int32 Index) const;

	/** 현재 선택된 그룹(PendingPositionGroup)에 속한 포지션 목록. */
	TArray<EFieldPosition> PositionsInGroup() const;

	/** 현재 그룹 안에서의 포지션 이름·설명 (인덱스 안전). */
	FText FieldPositionNameAt(int32 Index) const;
	FText FieldPositionDescAt(int32 Index) const;

	/** 난이도 확정 후: 타격이면 Stance 단계로, 아니면 바로 시작. */
	void ConfirmDifficulty();

	// ── VR 인메뉴 ──
	static constexpr int32 VrMaxRows = 6;   // 단계별 최대 행 수 (수비 종목 4 < 6)

	/** HMD 켜져 있으면 3D 카드/포인터를 켜고 텍스트 풀을 만든다. */
	void InitVRMenu();

	/** 매 틱: 겨눔 판정 + 드웰 타이머 + 하이라이트 갱신. */
	void UpdateVRMenu(float DeltaSeconds);

	/** 현재 단계의 3D 텍스트(제목/행/설명/뒤로)를 다시 채운다. */
	void RefreshVRMenuTexts();

	/** 컨트롤러가 겨누는 카드 인덱스 (0..RowCount-1, 뒤로=RowCount, 없음=INDEX_NONE). */
	int32 PickHoveredCard() const;

	/** 머리 방향을 보고 패널을 게으르게 재정렬한다 (데드존 밖일 때만 끌어온다). */
	void UpdateMenuAnchor(float DeltaSeconds);

	/** 현재 패널 방위각 (폰 기준, 도). UpdateMenuAnchor 가 관리한다. */
	float MenuYawDeg = 0.0f;
	bool  bMenuYawInit = false;

	bool  bVRMenu = false;
	int32 VrHoverIndex = INDEX_NONE;
	float VrDwellTimer = 0.0f;
	float VrCooldown = 0.0f;   // 확정 직후 오선택 방지용 짧은 잠금
	bool  bTriggerHeldPrev = false;   // 트리거 눌림 에지 검출용 (직전 프레임 상태)

	/**
	 * UpdateVRMenu 가 매 틱 읽는 트리거 키(제네릭/Vive). 손이 세션 내내 안 바뀌므로
	 * InitVRMenu 에서 한 번만 만들어 둔다 — 매 프레임 FString::Printf 로 FKey 를 새로 만드는
	 * 비용을 없앤다.
	 */
	FKey TriggerGenericKey;
	FKey TriggerViveKey;

	EStage Stage = EStage::Mode;

	/**
	 * ModeManager 를 못 찾았을 때 쓰는 빈 종합 점수 (bValid=false → "no records yet").
	 *
	 * ⚠️ 진짜 값은 UModeManager::GetOverallScore() 가 이력 버전으로 캐시한다. 예전엔 이 폰의
	 *    BeginPlay 에서 계산했는데, 폰 교체가 **새 폰을 먼저 스폰하고 옛 폰을 나중에 파괴**하고
	 *    세션 저장은 그 옛 폰의 EndPlay 에서 일어나므로 **항상 한 세션 뒤처진 값**이 잡혔다.
	 */
	FOverallScore CachedOverall;

	TArray<EGameModeId> MenuModes;
	TArray<EDifficultyLevel> MenuDifficulties;
	TArray<EBattingStance> MenuStances;
	TArray<FText> DefenseDrills;   // 수비 세부 종목 이름 (수비 모드 전용)

	int32 SelectedIndex = 0;

	/** 모드 단계에서 확정한 모드 (이후 단계에서 사용). */
	EGameModeId PendingMode = EGameModeId::Batting;

	/** 난이도 단계에서 확정한 난이도 (스탠스 단계에서 사용). */
	EDifficultyLevel PendingDifficulty = EDifficultyLevel::Amateur;

	/** 백업 위치 판단 — 확정한 포지션 그룹(0=내야/1=외야) / 최종 포지션. */
	int32 PendingPositionGroup = 0;
	EFieldPosition PendingFieldPosition = EFieldPosition::First;

	FString NoticeText;
	float   NoticeTimer = 0.0f;
};
