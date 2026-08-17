#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ModeSelectHUD.generated.h"

class UFont;
struct FSessionSummary;

/**
 * 시작 화면(모드 선택) 렌더러.
 *
 * AModeSelectPawn 이 들고 있는 선택 상태를 Canvas 로 그리기만 한다 (상태 없음 —
 * 커서 글라이드용 보간값만 예외).
 * 빙의된 폰이 AModeSelectPawn 이 아니면 아무것도 그리지 않으므로,
 * 모드 진입 후에는 각 모드 폰이 자기 화면을 그대로 그린다 — HUD 교체가 필요 없다.
 *
 * 디자인은 전부 Canvas 프리미티브(사각형·선·텍스트)로만 만든다. UMG 위젯도
 * 텍스처도 쓰지 않으므로 에디터에서 에셋을 만들 필요가 없다.
 * 레이아웃은 1080p 기준으로 잡고 화면 높이에 비례해 확대/축소한다.
 *
 * ⚠️ 엔진 기본 폰트(Roboto)는 한글 글리프가 없을 수 있다. 네모로 깨져 보이면
 *    이 클래스의 블루프린트 자식을 만들어 MenuFont 에 한글 폰트를 지정하고
 *    AMotionBaseGameMode 의 HUDClass 를 그 BP 로 바꾸면 된다 (코드 수정 불필요).
 */
UCLASS()
class MOTIONBASE_API AModeSelectHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	/** 지정하면 메뉴 전체에 이 폰트를 쓴다. 비우면 엔진 기본 폰트. */
	UPROPERTY(EditDefaultsOnly, Category = "ModeSelect|Style")
	TObjectPtr<UFont> MenuFont;

	/** 선택 강조가 행 사이를 미끄러지는 속도. 클수록 빠르게 따라붙는다. */
	UPROPERTY(EditDefaultsOnly, Category = "ModeSelect|Style")
	float CursorGlideSpeed = 16.0f;

	/** 배경 야구장 워터마크를 그릴지. */
	UPROPERTY(EditDefaultsOnly, Category = "ModeSelect|Style")
	bool bDrawFieldWatermark = true;

private:
	UFont* ResolveFont(bool bLarge) const;

	// ── 그리기 헬퍼 ──

	/** 가로로 꽉 찬 세로 그라데이션. 얇은 띠를 겹쳐 만든다. */
	void DrawVerticalGradient(float X, float Y, float W, float H,
		const FLinearColor& Top, const FLinearColor& Bottom, int32 Steps);

	/** 사각형 테두리 (사각형 4개). */
	void DrawOutlineRect(float X, float Y, float W, float H, const FLinearColor& Color, float Thickness);

	/** 배경 장식 — 야구 내야 다이아몬드 + 파울라인. */
	void DrawFieldWatermark(float CenterX, float CenterY, float Radius, float S);

	/** 키캡 박스 + 설명. 소비한 가로 폭을 돌려준다. */
	float DrawKeyHint(const FString& Key, const FString& Desc, float X, float Y, float S, UFont* Font);

	/** MaxWidth 를 넘으면 넘치지 않게 줄인 배율을 돌려준다. */
	float FitScale(const FString& Text, UFont* Font, float DesiredScale, float MaxWidth);

	// ── 세션 결과 화면 (모드 선택이 아닌, ISessionResultView 폰이 빙의됐을 때) ──

	/** 세션 요약을 중앙 패널로 그린다 (점수 3축·집계·약점·드릴·AI 코칭·신기록). */
	void DrawSessionResult(const FSessionSummary& Sum);

	/** 가로 막대 미터 하나 (라벨 + 0~1 게이지 + 퍼센트). */
	void DrawMeter(const FString& Label, float Value01, float X, float Y, float W, float S,
		UFont* Font, const FLinearColor& Fill);

	/** MaxW 를 넘지 않게 문자 단위로 줄바꿈해 그린다 (한글은 공백이 없어 문자 단위). 소비한 높이를 돌려준다. */
	float DrawWrapped(const FString& Text, const FLinearColor& Color, float X, float Y,
		float MaxW, float Scale, UFont* Font, float LineH);

	// ── 애니메이션 상태 ──

	/** 현재 강조 막대의 Y (목표 행으로 보간). */
	float CursorY = 0.0f;

	/** 첫 프레임에는 보간 없이 목표 위치로 붙인다. */
	bool bCursorInitialized = false;

	/** 단계 전환(모드↔난이도)으로 행 개수가 바뀌면 커서를 미끄러뜨리지 않고 즉시 붙인다. */
	int32 LastRowCount = -1;
};
