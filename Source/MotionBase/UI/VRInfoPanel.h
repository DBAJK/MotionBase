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
	void SetBackBelowRows(int32 RowCount, const FString& Text, const FColor& Color, bool bVisible);

	// ── 드웰 겨눔 판정용 접근자 ──
	UTextRenderComponent* GetRowText(int32 Index) const;
	UTextRenderComponent* GetBackText() const { return BackText; }

private:
	/** 공통 스타일(카메라 향함·중앙정렬·월드사이즈·초기 숨김)로 텍스트 하나 생성·등록. */
	UTextRenderComponent* CreateText(const TCHAR* Name, float WorldSize);

	UPROPERTY() TObjectPtr<UFont> PanelFont;
	UPROPERTY() TObjectPtr<UTextRenderComponent> TitleText;
	UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> RowTexts;
	UPROPERTY() TObjectPtr<UTextRenderComponent> BackText;
	UPROPERTY() TObjectPtr<UTextRenderComponent> FooterText;
	UPROPERTY() TObjectPtr<UTextRenderComponent> HintText;

	bool bBuilt = false;

	/** BuildPanel 이전에 SetPlacement 로 받은 값 — 생성 시 반영. */
	float PendingDistanceCm = DefaultDistanceCm;
	float PendingHeightCm   = DefaultHeightCm;
};
