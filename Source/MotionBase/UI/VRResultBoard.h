#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "VRResultBoard.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UFont;
struct FVREndCardMenu;

/** 결과 보드 왼쪽 날개의 막대 하나 (정확도·효율·일관성 / 타구 유형별 성공률 등). */
struct FVRResultMeter
{
	FString Label;        // "Accuracy"
	FString ValueText;    // "72%" · "84 km/h" · "1.35 s"
	float Value01 = 0.0f; // 막대 채움 0~1
	FLinearColor Color = FLinearColor(1.0f, 0.76f, 0.24f);
};

/** 결과 보드에 올릴 내용. 폰이 자기 세션 값으로 채워 매 틱 Show() 에 넘긴다. */
struct FVRResultBoardData
{
	/** 가운데 맨 위 — 종목·난이도. */
	FString Heading;

	/** 대표 점수 0~100. bScoreValid=false 면 "--" 로 그린다 ("0점"과 "기록 없음"은 다른 말). */
	float Score = 0.0f;
	bool  bScoreValid = false;

	/** 점수 아래 — 이번 판 결과 한 줄 ("Caught 7 / 10"). */
	FString ResultLine;

	/** 신기록/최고 기록 한 줄. bNewRecord 면 강조색. */
	FString RecordLine;
	bool    bNewRecord = false;

	/** ⚠️ 기준 상수가 실측 보정 전이면 반드시 표시한다 (FScoreResult::bUncalibrated). */
	bool bUncalibrated = false;

	/** 왼쪽 날개 막대 (최대 UVRResultBoard::MaxMeters). */
	TArray<FVRResultMeter> Meters;

	/** 왼쪽 날개 아래 — 보조 수치 (비거리·공 속도 배율 등). 길면 두 줄로 접힌다. */
	FString StatLine;

	/** 오른쪽 날개 — AI 코칭 문장 + 추천 드릴. */
	FString CoachingText;
	bool    bAwaitingCoaching = false;
	TArray<FString> Drills;
};

/**
 * VR 세션 종료 **결과 보드** — 헤드셋 안에서 점수를 크게, 공간에 세워 보여준다.
 *
 * 왜 필요한가: 예전 종료 화면은 UVRInfoPanel 의 컴팩트 행(작은 글자 4줄)에 점수를 제목 한 줄로
 * 끼워 넣었다. 뒤판이 없어 경기장 배경 위에 글자가 떠 있고, 점수·결과·코칭이 같은 크기라
 * "몇 점인가"가 한눈에 안 읽혔다. (데스크톱 결과 패널 AModeSelectHUD::DrawSessionResult 는
 * 평면 Canvas 라 HMD 에서는 아예 그리지 않는다.)
 *
 * 구성 — 눈을 중심으로 접힌 **3면 보드**(각 면이 눈을 향해 기울어 있다):
 *   [왼쪽 날개] 세부 막대 + 보조 수치   [가운데] 큰 점수 · 결과 · 기록   [오른쪽 날개] AI 코칭 · 드릴
 *   가운데 아래에 큼직한 버튼 두 장(PLAY AGAIN / BACK TO MENU) — 겨눠서 유지하면 확정.
 *
 * - 어두운 불투명 뒤판을 깔아 경기장 배경과 상관없이 대비가 확보된다.
 * - 점수·막대는 등장 시 차오르는 애니메이션으로 시선을 점수로 먼저 끈다.
 * - UMG 에셋 없이 동작한다 (엔진 기본 Cube + BasicShapeMaterial, KRFont TextRender).
 *
 * 사용: 소유 폰이 BeginPlay 에서 BuildBoard(), 세션 종료 동안 매 틱
 *   CopyAnchorFrom(VrPanel) → Show(Data), 입력은 UpdateButtons(EndMenu, ...).
 *   재시작/플레이 중에는 Hide().
 */
UCLASS(ClassGroup = (MotionBase), meta = (BlueprintSpawnableComponent))
class MOTIONBASE_API UVRResultBoard : public USceneComponent
{
	GENERATED_BODY()

public:
	UVRResultBoard();

	static constexpr int32 MaxMeters      = 3;
	static constexpr int32 MaxStatLines   = 2;
	static constexpr int32 MaxCoachLines  = 6;
	static constexpr int32 MaxDrillLines  = 2;
	static constexpr int32 NumButtons     = 2;

	/** 자식 메시·텍스트를 생성·등록한다 (한 번). 재호출은 무시. */
	void BuildBoard();

	/**
	 * 보드 원점을 다른 컴포넌트(같은 부모의 VrPanel)의 상대 트랜스폼에 맞춘다.
	 * 패널이 이미 '정면 고정 + 크게 돌아보면 재정렬' 배치를 하고 있으므로 그대로 따라간다.
	 */
	void CopyAnchorFrom(const USceneComponent* Anchor);

	/** 보드를 띄우고 내용을 갱신한다. 숨겨져 있다가 처음 뜨는 순간 등장 애니메이션을 시작한다. */
	void Show(const FVRResultBoardData& Data);

	/** 보드를 숨긴다 (이미 숨겨져 있으면 아무 것도 안 함). */
	void Hide();

	bool IsShowing() const { return bShowing; }

	/**
	 * 매 틱: 등장 애니메이션을 진행하고, 조준 광선으로 버튼 겨눔/드웰을 판정·표시한다.
	 * @return 이번 프레임에 확정된 버튼 (0 = PLAY AGAIN, 1 = BACK TO MENU). 없으면 INDEX_NONE.
	 */
	int32 UpdateButtons(FVREndCardMenu& Menu, const FVector& AimOrigin, const FVector& AimDir,
		bool bTracked, float DeltaSeconds);

private:
	UTextRenderComponent* MakeText(const TCHAR* Name, EHorizTextAligment Align);
	UStaticMeshComponent* MakeQuad(const TCHAR* Name, UMaterialInstanceDynamic* Material);
	UMaterialInstanceDynamic* MakeMaterial(const FLinearColor& Color);

	/**
	 * 면 Seg(0 = 왼쪽 날개, 1 = 가운데, 2 = 오른쪽 날개)의 평면 좌표(가로 U, 높이 Z)에 놓고 눈을 향하게 돌린다.
	 * Depth 는 눈 쪽(+)으로 띄우는 양.
	 */
	void Place(USceneComponent* C, int32 Seg, float U, float Z, float Depth, bool bIsText) const;

	/** 판(얇은 큐브)을 면 위에 폭 W · 높이 H 로 놓는다. */
	void PlaceQuad(UStaticMeshComponent* Q, int32 Seg, float U, float Z, float W, float H, float Depth) const;

	/** 막대 채움을 왼쪽 끝 기준으로 Value01 만큼 늘린다. */
	void PlaceFill(UStaticMeshComponent* Q, int32 Seg, float LeftU, float Z, float FullW, float H,
		float Depth, float Value01) const;

	/** 텍스트를 갱신한다 — 바뀐 경우에만 다시 올리고, MaxW 를 넘으면 글자 크기를 줄여 맞춘다. */
	void SetLine(UTextRenderComponent* T, const FString& Text, const FColor& Color, float BaseSize, float MaxW) const;

	/** 애니메이션 값(점수 카운트업·막대 채움)을 현재 진행도로 반영한다. */
	void ApplyAnimated();

	/** 보드 소속 컴포넌트 전부의 표시 여부. */
	void SetAllVisible(bool bVisible);

	UPROPERTY() TObjectPtr<UFont> BoardFont;
	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY() TObjectPtr<UMaterialInterface> ShapeMaterial;

	// ── 판 ──
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Plates;       // 3면 뒤판
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> PlateStrips;  // 3면 윗줄 액센트
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> MeterTracks;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> MeterFills;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ButtonPlates;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ButtonFills;

	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> MeterFillMats;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> ButtonPlateMats;

	// ── 가운데 ──
	UPROPERTY() TObjectPtr<UTextRenderComponent> HeadingText;
	UPROPERTY() TObjectPtr<UTextRenderComponent> ScoreText;
	UPROPERTY() TObjectPtr<UTextRenderComponent> ScoreMaxText;
	UPROPERTY() TObjectPtr<UTextRenderComponent> RecordText;
	UPROPERTY() TObjectPtr<UTextRenderComponent> ResultText;
	UPROPERTY() TObjectPtr<UTextRenderComponent> UncalText;
	UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> ButtonTexts;
	UPROPERTY() TObjectPtr<UTextRenderComponent> HintText;

	// ── 왼쪽 날개 ──
	UPROPERTY() TObjectPtr<UTextRenderComponent> LeftHeader;
	UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> MeterLabels;
	UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> MeterValues;
	UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> StatTexts;

	// ── 오른쪽 날개 ──
	UPROPERTY() TObjectPtr<UTextRenderComponent> RightHeader;
	UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> CoachTexts;
	UPROPERTY() TObjectPtr<UTextRenderComponent> DrillHeader;
	UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> DrillTexts;

	/** Hide/Show 대상 전부. */
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> AllParts;

	FVRResultBoardData Current;

	/** 코칭 문장 줄바꿈 캐시 — 원문이 바뀔 때만 다시 접는다. */
	FString WrappedCoachSource;
	TArray<FString> WrappedCoachLines;

	bool  bBuilt = false;
	bool  bShowing = false;

	/** 등장 후 흐른 시간 (초). 점수 카운트업·막대 채움·입력 지연이 이 값을 본다. */
	float AnimTime = 0.0f;

	/** 지금 겨누는 버튼과 진행도 (시각 표시용 — 판정 상태는 FVREndCardMenu 가 쥔다). */
	int32 HoverButton = INDEX_NONE;
	float HoverProgress = 0.0f;
};
