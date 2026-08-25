#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VRInfoPanel.generated.h"

class UTextRenderComponent;
class UFont;

/**
 * VR 헤드셋 안에 띄우는 **월드 고정 3D 정보 패널** (재사용 컴포넌트).
 *
 * 왜 필요한가: AHUD 의 Canvas(DrawText/DrawRect) 와 AddOnScreenDebugMessage 는
 * 데스크톱 뷰포트에만 그려지고 **스테레오 HMD 안에는 렌더되지 않는다.** 그래서 VR
 * 에서 보이는 UI 는 월드 공간의 3D 텍스트여야 한다. 모드 선택·백업 화면이 각자
 * TextRender 를 손으로 만들며 `MakeText` 를 복붙하던 것을 이 컴포넌트로 통합한다.
 *
 * 구성: 제목(Title) · 행 풀(Rows[MaxRows]) · 뒤로(Back) · 푸터(Footer) · 힌트(Hint).
 *   - 상태 패널(포구/송구/타격): Title + 몇 개 Row + Footer + Hint 만 쓴다.
 *   - 선택 메뉴(모드선택/백업): Row 를 보기 카드로, Back/드웰 겨눔에 GetRowText 를 쓴다.
 *
 * 배치 원칙(#3): **월드 고정** — 소유 폰의 루트(정면 +X, Y=0)에 붙여 눈높이 앞에 둔다.
 *   카메라(머리)에 붙이는 헤드락은 지양한다(고개 돌리면 따라와 멀미·부자연). 결과
 *   토스트처럼 잠깐 뜨는 것만 예외적으로 헤드락을 허용한다(이 컴포넌트 밖에서 처리).
 *
 * ⚠️ 자식 텍스트는 **런타임 생성**한다(BuildPanel). 소유 폰이 BeginPlay 에서 한 번
 *    BuildPanel() 을 부른 뒤 Set* 로 내용을 채운다. (중첩 default-subobject 의 등록
 *    누락 위험을 피하려는 의도적 설계.)
 */
UCLASS(ClassGroup = (MotionBase), meta = (BlueprintSpawnableComponent))
class MOTIONBASE_API UVRInfoPanel : public USceneComponent
{
	GENERATED_BODY()

public:
	UVRInfoPanel();

	/** 행 풀 최대 개수 (모드 6개 · 수비 보기 4개 모두 수용). */
	static constexpr int32 MaxRows = 6;

	/** 권장 배치값 — 플레이어 앞 거리 / 중심 높이 (cm). */
	static constexpr float DefaultDistanceCm = 250.0f;
	static constexpr float DefaultHeightCm   = 150.0f;

	/** 행 Z 레이아웃 상수 (뒤로 카드 배치 계산 등에 사용). */
	static constexpr float RowTopZ  = 30.0f;
	static constexpr float RowStepZ = 22.0f;

	/** 카드 한 장의 반너비/반높이 (cm). 프레임·하이라이트를 그릴 때의 기준. */
	static constexpr float CardHalfW = 82.0f;
	static constexpr float CardHalfH = 9.0f;

	/** 자식 텍스트를 생성·등록한다. 소유 폰이 BeginPlay 에서 한 번 호출. 재호출은 무시. */
	void BuildPanel();

	/** 패널을 소유 루트 기준 (정면 Distance, Y=0, 높이 Height) 에 놓는다. */
	void SetPlacement(float DistanceCm, float HeightCm);

	/** 패널의 모든 텍스트를 끈다 (PC 모드 등 비활성 시). */
	void HideAll();

	// ── 내용 채우기 (호출 시 해당 요소가 보이게 된다) ──
	void SetTitle(const FString& Text, const FColor& Color);
	void SetRow(int32 Index, const FString& Text, const FColor& Color);
	void SetFooter(const FString& Text, const FColor& Color);
	void SetHint(const FString& Text, const FColor& Color);

	/** Index 이상의 행을 모두 숨긴다 (현재 단계 행 수보다 뒤쪽 정리). */
	void HideRowsFrom(int32 FirstHiddenIndex);

	/**
	 * 뒤로 카드를 행 아래에 배치·표시한다 (선택 메뉴 전용).
	 * @param RowCount 현재 보이는 행 수 — 그 바로 아래에 놓기 위한 값.
	 */
	void SetBackBelowRows(int32 RowCount, const FString& Text, const FColor& Color, bool bShow);

	// ── 게임 모드용 '뒤로' 카드 (드웰로 나가기) ─────────────────────────────
	// 액션 모드(타격/포구/송구)는 나가는 길이 '컨트롤러 위로 들기' 제스처뿐이라
	// 발견이 어렵다. 그래서 패널에 상시 '뒤로' 카드를 두고, 조준 광선으로 겨눠
	// 잠시 유지(드웰)하면 나가게 한다 — 제스처와 병행하는 두 번째 출구.

	/** 상태 패널(행이 적은 모드)용으로 요소들을 눈높이 근처로 모은다. BuildPanel 뒤 한 번 호출. */
	void SetStatusCompact();

	/** '뒤로' 카드를 패널 하단(눈높이 근처)에 상시 표시한다. */
	void ShowBackCard(const FString& Label, const FColor& Color);

	/** '뒤로' 카드를 숨긴다. */
	void HideBackCard();

	/**
	 * 매 틱: 조준 광선이 '뒤로' 카드를 겨누는지 판정해 드웰을 누적하고,
	 * 카드 프레임(채움)·조준 광선·드웰 링을 그린다.
	 * @param bTracked  조준 컨트롤러 추적 여부(끊기면 진행 리셋).
	 * @param DwellSec  이 시간(초) 유지하면 발동.
	 * @param AngleDeg  이 각도(도) 안이면 호버.
	 * @return 이번 프레임에 드웰이 차서 발동했으면 true (한 번만).
	 */
	bool UpdateBackDwell(const FVector& AimOrigin, const FVector& AimDir, bool bTracked,
		float DwellSec, float AngleDeg, float DeltaSeconds);

	// ── 드웰 겨눔 판정용 접근자 ──
	UTextRenderComponent* GetRowText(int32 Index) const;
	UTextRenderComponent* GetBackText() const { return BackText; }

	// ══ 편안한 배치(VR 멀미 방지) ═════════════════════════════════════
	/**
	 * 패널을 **플레이어 정면(HMD 방위)** 에 놓되, 평소엔 제자리에 고정한다.
	 *
	 * 왜: 폰 고정 +X 에 두면 플레이어가 그쪽을 안 보면 패널이 옆으로 치우쳐 보이고(이질감),
	 * 매 프레임 머리를 좇으면 패널이 헤엄치듯 따라와(swimming) 멀미를 유발한다. 그래서
	 *   ① 처음엔 지금 보는 방향 정면에 배치하고
	 *   ② 작은 머리 움직임은 무시(제자리 고정 — 흔들림 없음)
	 *   ③ 머리가 RecenterDeg 이상 크게 돌아가면(=패널을 안 보는 상태) 그때만 정면으로 스냅
	 * → 볼 땐 안 흔들리고, 크게 돌아봐도 다시 앞에 나타난다. 소유 폰이 매 틱 호출.
	 *
	 * @param Head        소유 폰의 HMD 카메라(패널과 같은 부모 기준). null 이면 아무 것도 안 함.
	 * @param DistanceCm  플레이어 앞 거리.
	 * @param HeightCm    패널 중심 높이(부모 기준).
	 * @param RecenterDeg 이 각도 이상 벗어나면 정면으로 다시 스냅.
	 * @param YawOffsetDeg 정면에서 좌(-)/우(+)로 이만큼 비켜 놓는다.
	 *                     타격처럼 **정면에 공이 날아오는** 종목에서 패널이 시야를 가리지 않게
	 *                     비워둘 때 쓴다. 재정렬 판정은 오프셋 이전의 정면 기준으로 한다.
	 */
	void UpdateComfortAnchor(const USceneComponent* Head, float DistanceCm, float HeightCm, float RecenterDeg,
		float YawOffsetDeg = 0.0f);

	/** 다음 UpdateComfortAnchor 호출에서 정면으로 다시 잡도록 강제(모드 진입/재정렬 시). */
	void RequestRecenter() { bAnchorInit = false; }

	// ══ 공간감(VR) 연출 ══════════════════════════════════════════════
	// 평평한 텍스트 목록은 헤드셋 안에서 "화면을 붙여놨다"처럼 보인다. 아래 셋으로
	// 같은 데이터를 곡면 카드 + 프레임 + 광선/조준점으로 바꿔 공간 UI 로 읽히게 한다.
	// (UMG 에셋 없이 동작해야 하므로 프레임은 디버그 라인으로 그린다 — 스테레오에 렌더된다.)

	/**
	 * 행들을 눈을 중심으로 한 원통면에 올리고, 각 행이 눈을 바라보도록 기울인다.
	 * 위/아래 행이 시야 가장자리에서도 정면으로 보여 목록이 '휘어 감싸는' 느낌이 된다.
	 *
	 * @param EyeOffsetZ 패널 원점 기준 눈높이 (눈높이 - 패널높이). 보통 음수/양수 작은 값.
	 * @param RadiusCm   원통 반지름 = 눈에서 패널까지 거리.
	 */
	void ApplyCurvedLayout(float EyeOffsetZ, float RadiusCm);

	/**
	 * 카드 프레임·선택 하이라이트·구분선을 월드에 그린다 (매 틱 호출).
	 * @param VisibleRowCount 지금 보이는 행 수
	 * @param HoverIndex      겨누고 있는 행 (뒤로 카드는 VisibleRowCount, 없으면 INDEX_NONE)
	 * @param DwellProgress   드웰 진행도 0~1 (하이라이트가 이만큼 채워진다)
	 * @param bBackVisible    뒤로 카드가 떠 있는지
	 */
	void DrawChrome(int32 VisibleRowCount, int32 HoverIndex, float DwellProgress, bool bBackVisible) const;

	/**
	 * 컨트롤러 광선 + 패널 위 조준점 + 드웰 링을 그린다.
	 * 광선이 패널 면에 '닿는 점'이 보여야 어디를 겨누는지 손으로 알 수 있다.
	 */
	void DrawPointerRay(const FVector& Origin, const FVector& Dir, float DwellProgress, bool bHovering) const;

	/** 겨누는 카드가 살짝 커졌다 돌아오는 애니메이션 (Tick 에서 호출). */
	void TickHoverAnim(float DeltaSeconds, int32 HoverIndex, int32 VisibleRowCount);

private:
	/** 카드 사각 테두리 하나를 그린다 (행 컴포넌트의 월드 축 기준). */
	void DrawCardFrame(const UTextRenderComponent* Card, const FColor& Color,
		float Thickness, float FillProgress) const;

	/** 곡면 배치 파라미터 (ApplyCurvedLayout 이 채운다). 0 이면 평면 배치. */
	float CurveRadiusCm = 0.0f;
	float CurveEyeZ     = 0.0f;

	/** 행별 현재 텍스트 크기 (호버 애니메이션 보간 대상). */
	TArray<float> RowSizes;

	/** 공통 스타일(카메라 향함·중앙정렬·월드사이즈·초기 숨김)로 텍스트 하나 생성·등록. */
	UTextRenderComponent* CreateText(const TCHAR* Name, float WorldSize);

	UPROPERTY() TObjectPtr<UFont> PanelFont;
	UPROPERTY() TObjectPtr<UTextRenderComponent> TitleText;
	UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> RowTexts;
	UPROPERTY() TObjectPtr<UTextRenderComponent> BackText;
	UPROPERTY() TObjectPtr<UTextRenderComponent> FooterText;
	UPROPERTY() TObjectPtr<UTextRenderComponent> HintText;

	bool bBuilt = false;

	/** '뒤로' 카드 드웰 누적 시간(초). UpdateBackDwell 이 관리. */
	float BackDwellTimer = 0.0f;

	/** '뒤로' 카드의 패널 로컬 Z (SetStatusCompact 이 조정). */
	float BackCardZ = -72.0f;

	/** 편안한 배치 상태 (UpdateComfortAnchor). 재정렬 순간의 머리 방위·수평위치를 캡처해
	    그 사이엔 고정한다 — 머리 흔들림(sway)이 패널로 옮겨오지 않게. */
	float   AnchorYaw   = 0.0f;
	FVector AnchorPos   = FVector::ZeroVector;
	bool    bAnchorInit = false;

	/** BuildPanel 이전에 SetPlacement 로 받은 값 — 생성 시 반영. */
	float PendingDistanceCm = DefaultDistanceCm;
	float PendingHeightCm   = DefaultHeightCm;
};
