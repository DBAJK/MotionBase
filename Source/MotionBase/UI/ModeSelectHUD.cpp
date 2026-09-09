#include "UI/ModeSelectHUD.h"
#include "UI/SessionResultView.h"
#include "Core/ModeSelectPawn.h"
#include "Core/ModeManager.h"
#include "Analysis/WeaknessDetector.h"
#include "Scoring/ScoringService.h"
#include "Data/SessionSummary.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HeadMountedDisplayFunctionLibrary.h"

namespace ModeSelectStyle
{
	// 야간 구장 톤 — 깊은 남색 배경에 앰버 액센트.
	const FLinearColor BackdropTop   (0.016f, 0.024f, 0.047f, 1.00f);
	const FLinearColor BackdropBottom(0.043f, 0.071f, 0.118f, 1.00f);

	const FLinearColor Accent        (1.000f, 0.760f, 0.240f, 1.00f);
	const FLinearColor FieldLine     (0.45f,  0.62f,  0.85f,  0.085f);

	const FLinearColor TextTitle     (0.960f, 0.975f, 1.000f, 1.00f);
	const FLinearColor TextPrimary   (0.900f, 0.930f, 0.980f, 1.00f);
	const FLinearColor TextSecondary (0.520f, 0.600f, 0.720f, 1.00f);
	const FLinearColor TextLocked    (0.340f, 0.390f, 0.480f, 1.00f);
	const FLinearColor TextOnAccent  (0.050f, 0.070f, 0.110f, 1.00f);

	const FLinearColor CardIdle      (1.000f, 1.000f, 1.000f, 0.035f);
	const FLinearColor CardLocked    (1.000f, 1.000f, 1.000f, 0.015f);
	const FLinearColor CardSelected  (0.130f, 0.360f, 0.680f, 0.520f);
	const FLinearColor CardBorderSel (0.400f, 0.680f, 1.000f, 0.550f);

	const FLinearColor BadgeIdle     (1.000f, 1.000f, 1.000f, 0.070f);

	const FLinearColor PillReady     (0.130f, 0.520f, 0.330f, 0.750f);
	const FLinearColor PillReadyText (0.640f, 0.950f, 0.760f, 1.00f);
	const FLinearColor PillLocked    (1.000f, 1.000f, 1.000f, 0.045f);

	const FLinearColor Divider       (1.000f, 1.000f, 1.000f, 0.090f);
	const FLinearColor Notice        (1.000f, 0.560f, 0.340f, 1.00f);
	const FLinearColor KeycapBg      (1.000f, 1.000f, 1.000f, 0.070f);
	const FLinearColor KeycapBorder  (1.000f, 1.000f, 1.000f, 0.130f);
}

using namespace ModeSelectStyle;

UFont* AModeSelectHUD::ResolveFont(bool bLarge) const
{
	if (MenuFont)
	{
		return MenuFont;
	}
	if (!GEngine)
	{
		return nullptr;
	}
	return bLarge ? GEngine->GetLargeFont() : GEngine->GetMediumFont();
}

float AModeSelectHUD::FitScale(const FString& Text, UFont* Font, float DesiredScale, float MaxWidth)
{
	if (MaxWidth <= 0.0f)
	{
		return DesiredScale;
	}

	float TW = 0.0f, TH = 0.0f;
	GetTextSize(Text, TW, TH, Font, DesiredScale);

	if (TW > MaxWidth && TW > KINDA_SMALL_NUMBER)
	{
		return DesiredScale * (MaxWidth / TW);
	}
	return DesiredScale;
}

void AModeSelectHUD::DrawVerticalGradient(float X, float Y, float W, float H,
	const FLinearColor& Top, const FLinearColor& Bottom, int32 Steps)
{
	Steps = FMath::Max(Steps, 1);
	const float StripH = H / Steps;

	for (int32 i = 0; i < Steps; ++i)
	{
		const float T = (Steps > 1) ? static_cast<float>(i) / (Steps - 1) : 0.0f;
		// +1px 로 띠 사이 이음새가 보이지 않게 겹친다.
		DrawRect(FMath::Lerp(Top, Bottom, T), X, Y + i * StripH, W, StripH + 1.0f);
	}
}

void AModeSelectHUD::DrawOutlineRect(float X, float Y, float W, float H, const FLinearColor& Color, float Thickness)
{
	Thickness = FMath::Max(Thickness, 1.0f);
	DrawRect(Color, X, Y, W, Thickness);                        // 위
	DrawRect(Color, X, Y + H - Thickness, W, Thickness);        // 아래
	DrawRect(Color, X, Y, Thickness, H);                        // 왼쪽
	DrawRect(Color, X + W - Thickness, Y, Thickness, H);        // 오른쪽
}

void AModeSelectHUD::DrawFieldWatermark(float CenterX, float CenterY, float Radius, float S)
{
	const float Thick = FMath::Max(2.0f * S, 1.0f);

	// 내야 다이아몬드 — 홈(아래) → 1루(오른쪽) → 2루(위) → 3루(왼쪽)
	const FVector2D Home  (CenterX,          CenterY + Radius);
	const FVector2D First (CenterX + Radius, CenterY);
	const FVector2D Second(CenterX,          CenterY - Radius);
	const FVector2D Third (CenterX - Radius, CenterY);

	DrawLine(Home.X,   Home.Y,   First.X,  First.Y,  FieldLine, Thick);
	DrawLine(First.X,  First.Y,  Second.X, Second.Y, FieldLine, Thick);
	DrawLine(Second.X, Second.Y, Third.X,  Third.Y,  FieldLine, Thick);
	DrawLine(Third.X,  Third.Y,  Home.X,   Home.Y,   FieldLine, Thick);

	// 파울라인 — 홈에서 1·3루 방향으로 뻗어나간다.
	const float Foul = Radius * 2.1f;
	DrawLine(Home.X, Home.Y, Home.X + Foul, Home.Y - Foul, FieldLine, Thick);
	DrawLine(Home.X, Home.Y, Home.X - Foul, Home.Y - Foul, FieldLine, Thick);

	// 베이스 — 각 꼭짓점에 작은 사각형.
	const float BaseSize = 9.0f * S;
	const FLinearColor BaseCol(FieldLine.R, FieldLine.G, FieldLine.B, FieldLine.A * 2.2f);
	const FVector2D Bases[4] = { Home, First, Second, Third };
	for (const FVector2D& B : Bases)
	{
		DrawRect(BaseCol, B.X - BaseSize * 0.5f, B.Y - BaseSize * 0.5f, BaseSize, BaseSize);
	}

	// 마운드 — 중앙에 작은 사각형.
	const float MoundSize = 13.0f * S;
	DrawRect(BaseCol, CenterX - MoundSize * 0.5f, CenterY - MoundSize * 0.5f, MoundSize, MoundSize);
}

float AModeSelectHUD::DrawKeyHint(const FString& Key, const FString& Desc, float X, float Y, float S, UFont* Font)
{
	const float KeyScale = 0.85f * S;
	const float DescScale = 0.90f * S;
	const float PadX = 10.0f * S;
	const float Gap = 9.0f * S;
	const float TailGap = 26.0f * S;

	float KeyW = 0.0f, KeyH = 0.0f;
	GetTextSize(Key, KeyW, KeyH, Font, KeyScale);

	const float CapW = KeyW + PadX * 2.0f;
	const float CapH = KeyH + 10.0f * S;

	DrawRect(KeycapBg, X, Y, CapW, CapH);
	DrawOutlineRect(X, Y, CapW, CapH, KeycapBorder, FMath::Max(1.0f * S, 1.0f));
	DrawText(Key, TextPrimary, X + PadX, Y + (CapH - KeyH) * 0.5f, Font, KeyScale);

	float DescW = 0.0f, DescH = 0.0f;
	GetTextSize(Desc, DescW, DescH, Font, DescScale);
	DrawText(Desc, TextSecondary, X + CapW + Gap, Y + (CapH - DescH) * 0.5f, Font, DescScale);

	return CapW + Gap + DescW + TailGap;
}

void AModeSelectHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	// ⚠️ VR(HMD)에서는 이 평면 Canvas HUD 를 그리지 않는다.
	// Canvas 는 스테레오에서 눈마다 다른 위치로 찍혀 좌/우 화면이 어긋나 보이고,
	// 3D 월드 패널(UVRInfoPanel)과 겹쳐 어지럽다. 헤드셋 안 UI 는 각 폰의 VrPanel 이 전담한다.
	// (평면 HUD 는 HMD 가 없는 PC 개발/시연 화면 전용.)
	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		return;
	}

	// 모드 플레이 중이면 메뉴를 그리지 않는다 (빙의된 폰으로 판단).
	AModeSelectPawn* SelectPawn = Cast<AModeSelectPawn>(GetOwningPawn());
	if (!SelectPawn)
	{
		bCursorInitialized = false; // 다음 복귀 때 커서가 미끄러져 들어오지 않도록 초기화.

		// 결과 화면을 제공하는 폰(ISessionResultView)이 결과 상태면 결과 패널을 그린다.
		// 모드 폰마다 HUD 를 갈아끼우지 않고, 이 단일 HUD 가 인터페이스로만 읽어 그린다.
		if (ISessionResultView* ResultView = Cast<ISessionResultView>(GetOwningPawn()))
		{
			FSessionSummary Summary;
			if (ResultView->GetSessionSummary(Summary))
			{
				DrawSessionResult(Summary);
			}
		}
		return;
	}

	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	if (W <= 0.0f || H <= 0.0f)
	{
		return;
	}

	// 1080p 기준으로 잡고 화면 높이에 비례 확대/축소.
	const float S = FMath::Clamp(H / 1080.0f, 0.7f, 2.5f);

	UFont* const FontLarge = ResolveFont(true);
	UFont* const FontBody = ResolveFont(false);

	const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float Delta = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;

	float TW = 0.0f, TH = 0.0f;

	// ── 배경 ──
	DrawVerticalGradient(0.0f, 0.0f, W, H, BackdropTop, BackdropBottom, 64);

	if (bDrawFieldWatermark)
	{
		// 오른쪽에 크게 배치. 콘텐츠는 왼쪽 컬럼에 두어 비대칭 균형을 만든다.
		DrawFieldWatermark(W * 0.79f, H * 0.50f, H * 0.27f, S);
	}

	// ── 레이아웃 기준 ──
	const float ContentX = W * 0.085f;
	const float ContentW = FMath::Min(W * 0.58f, 860.0f * S);

	// ── 헤더 ──
	const FString Title = TEXT("MOTIONBASE");
	const float TitleScale = FitScale(Title, FontLarge, 2.4f * S, ContentW);
	GetTextSize(Title, TW, TH, FontLarge, TitleScale);

	const float TitleY = H * 0.115f;
	const float TitleH = TH;

	// 타이틀 왼쪽 앰버 액센트 바
	DrawRect(Accent, ContentX, TitleY + TitleH * 0.08f, 6.0f * S, TitleH * 0.84f);
	DrawText(Title, TextTitle, ContentX + 24.0f * S, TitleY, FontLarge, TitleScale);

	// ── 종합 점수 (오른쪽 컬럼) ──
	// 워터마크가 있던 빈 영역을 쓴다. 메뉴(왼쪽 컬럼)와 겹치지 않는다.
	DrawOverallScore(W * 0.615f, H * 0.145f, FMath::Min(W * 0.30f, 420.0f * S), S, FontLarge, FontBody);

	// 부제는 단계(모드/난이도/타석/수비종목)에 따라 폰이 정한다.
	const FString Subtitle = SelectPawn->GetHeaderSubtitle().ToString();
	const float SubScale = FitScale(Subtitle, FontBody, 0.95f * S, ContentW);
	GetTextSize(Subtitle, TW, TH, FontBody, SubScale);
	const float SubY = TitleY + TitleH + 10.0f * S;
	DrawText(Subtitle, TextSecondary, ContentX + 24.0f * S, SubY, FontBody, SubScale);

	// 구분선
	const float DividerY = SubY + TH + 26.0f * S;
	DrawRect(Divider, ContentX, DividerY, ContentW, FMath::Max(1.5f * S, 1.0f));

	// ── 선택 목록 (단계별 행 데이터를 폰이 준다: 모드/난이도/타석/수비종목) ──
	const int32 RowCount = SelectPawn->GetRowCount();
	const int32 Selected = SelectPawn->GetSelectedIndex();

	const float RowH = 58.0f * S;
	const float RowGap = 9.0f * S;
	const float ListY = DividerY + 34.0f * S;

	// 단계가 바뀌어 행 개수가 달라지면 커서를 미끄러뜨리지 않고 즉시 붙인다
	// (모드 6행 → 난이도 3행으로 줄 때 강조 막대가 빈 공간을 지나는 것을 막는다).
	if (RowCount != LastRowCount)
	{
		bCursorInitialized = false;
		LastRowCount = RowCount;
	}

	// 선택 강조 위치를 목표 행으로 부드럽게 보간 — 스냅보다 눈이 따라가기 쉽다.
	const float TargetY = ListY + Selected * (RowH + RowGap);
	if (!bCursorInitialized)
	{
		CursorY = TargetY;
		bCursorInitialized = true;
	}
	else
	{
		CursorY = FMath::FInterpTo(CursorY, TargetY, Delta, CursorGlideSpeed);
	}

	// 강조 카드 (행 텍스트보다 먼저 = 뒤에 깔린다)
	DrawRect(CardSelected, ContentX, CursorY, ContentW, RowH);
	DrawOutlineRect(ContentX, CursorY, ContentW, RowH, CardBorderSel, FMath::Max(1.0f * S, 1.0f));

	// 선택 행 왼쪽 액센트 바 — 은은하게 맥동시켜 "지금 여기" 신호를 준다.
	const float Pulse = 0.72f + 0.28f * FMath::Sin(Time * 3.2f);
	DrawRect(FLinearColor(Accent.R, Accent.G, Accent.B, Pulse),
		ContentX, CursorY, 5.0f * S, RowH);

	const float BadgeSize = 34.0f * S;
	const float BadgeX = ContentX + 22.0f * S;
	const float NameX = BadgeX + BadgeSize + 20.0f * S;

	for (int32 i = 0; i < RowCount; ++i)
	{
		const bool bSelected = (i == Selected);
		const bool bAvailable = SelectPawn->IsRowAvailable(i);

		const float RowY = ListY + i * (RowH + RowGap);

		// 비선택 카드 배경 (선택 카드는 위에서 이미 그렸다)
		if (!bSelected)
		{
			DrawRect(bAvailable ? CardIdle : CardLocked, ContentX, RowY, ContentW, RowH);
		}

		// 번호 배지
		const float BadgeY = RowY + (RowH - BadgeSize) * 0.5f;
		DrawRect(bSelected ? Accent : BadgeIdle, BadgeX, BadgeY, BadgeSize, BadgeSize);

		const FString Index = FString::Printf(TEXT("%02d"), i + 1);
		const float IndexScale = 0.85f * S;
		GetTextSize(Index, TW, TH, FontBody, IndexScale);
		DrawText(Index, bSelected ? TextOnAccent : (bAvailable ? TextSecondary : TextLocked),
			BadgeX + (BadgeSize - TW) * 0.5f, BadgeY + (BadgeSize - TH) * 0.5f, FontBody, IndexScale);

		// 상태 태그 (오른쪽 정렬) — 비어 있으면 그리지 않는다 (난이도 행).
		// 먼저 폭을 잡아 이름이 침범하지 않게 한다.
		const FString PillText = SelectPawn->GetRowTag(i).ToString();
		const bool bHasPill = !PillText.IsEmpty();
		const float PillScale = 0.78f * S;
		float PillX = ContentX + ContentW - 18.0f * S; // 태그 없으면 이름은 여기까지 쓸 수 있다

		if (bHasPill)
		{
			GetTextSize(PillText, TW, TH, FontBody, PillScale);
			const float PillPadX = 12.0f * S;
			const float PillW = TW + PillPadX * 2.0f;
			const float PillH = TH + 9.0f * S;
			PillX = ContentX + ContentW - PillW - 18.0f * S;
			const float PillY = RowY + (RowH - PillH) * 0.5f;

			DrawRect(bAvailable ? PillReady : PillLocked, PillX, PillY, PillW, PillH);
			DrawText(PillText, bAvailable ? PillReadyText : TextLocked,
				PillX + PillPadX, PillY + (PillH - TH) * 0.5f, FontBody, PillScale);
		}

		// 행 이름 (모드/난이도/타석/수비종목 이름)
		const FString Name = SelectPawn->GetRowLabel(i).ToString();
		const float NameAvail = (PillX - 16.0f * S) - NameX;
		const float NameScale = FitScale(Name, FontBody, 1.25f * S, NameAvail);
		GetTextSize(Name, TW, TH, FontBody, NameScale);
		DrawText(Name, bSelected ? TextTitle : (bAvailable ? TextPrimary : TextLocked),
			NameX, RowY + (RowH - TH) * 0.5f, FontBody, NameScale);
	}

	// ── 선택 항목 설명 ──
	const float DescY = ListY + RowCount * (RowH + RowGap) + 26.0f * S;

	// 설명 앞 짧은 앰버 표식
	DrawRect(Accent, ContentX, DescY + 4.0f * S, 3.0f * S, 18.0f * S);

	const FString Desc = SelectPawn->GetSelectedDescription().ToString();
	const float DescScale = FitScale(Desc, FontBody, 0.95f * S, ContentW - 16.0f * S);
	GetTextSize(Desc, TW, TH, FontBody, DescScale);
	DrawText(Desc, TextSecondary, ContentX + 16.0f * S, DescY, FontBody, DescScale);

	// ── 안내 문구 (미구현 모드 선택 시) ──
	const FString& NoticeMsg = SelectPawn->GetNoticeText();
	if (!NoticeMsg.IsEmpty())
	{
		const float NoticeScale = FitScale(NoticeMsg, FontBody, 0.95f * S, ContentW);
		GetTextSize(NoticeMsg, TW, TH, FontBody, NoticeScale);
		DrawText(NoticeMsg, Notice, ContentX + 16.0f * S, DescY + 34.0f * S, FontBody, NoticeScale);
	}

	// ── 푸터: 조작 안내 + 구현 현황 ──
	const float FooterY = H * 0.885f;
	DrawRect(Divider, ContentX, FooterY - 24.0f * S, ContentW, FMath::Max(1.0f * S, 1.0f));

	// 키캡 라벨은 ASCII / 한글만 쓴다. 화살표(↑↓) 글리프는 엔진 기본 폰트에
	// 없을 수 있어 한글과 별개의 깨짐 원인이 된다.
	float HintX = ContentX;
	HintX += DrawKeyHint(TEXT("방향키"), TEXT("이동"), HintX, FooterY, S, FontBody);
	HintX += DrawKeyHint(TEXT("Enter"), TEXT("선택"), HintX, FooterY, S, FontBody);
	HintX += DrawKeyHint(TEXT("Bksp"), TEXT("뒤로"), HintX, FooterY, S, FontBody);
	HintX += DrawKeyHint(TEXT("V"), TEXT("Vive 진단"), HintX, FooterY, S, FontBody);

	const FString Status = SelectPawn->GetFooterStatus().ToString();
	const float StatusScale = 0.85f * S;
	GetTextSize(Status, TW, TH, FontBody, StatusScale);
	DrawText(Status, TextLocked, ContentX + ContentW - TW, FooterY + 8.0f * S, FontBody, StatusScale);
}

void AModeSelectHUD::DrawMeter(const FString& Label, float Value01, float X, float Y, float W, float S,
	UFont* Font, const FLinearColor& Fill)
{
	const float LabelScale = 0.85f * S;
	DrawText(Label, TextSecondary, X, Y, Font, LabelScale);

	const float BarX = X + 92.0f * S;
	const float PctW = 54.0f * S;
	const float BarW = FMath::Max(W - 92.0f * S - PctW, 10.0f * S);
	const float BarH = 14.0f * S;
	const float BarY = Y + 2.0f * S;
	const float V = FMath::Clamp(Value01, 0.0f, 1.0f);

	DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, 0.06f), BarX, BarY, BarW, BarH);
	DrawRect(Fill, BarX, BarY, BarW * V, BarH);
	DrawText(FString::Printf(TEXT("%.0f%%"), V * 100.0f), TextPrimary, BarX + BarW + 10.0f * S, Y, Font, LabelScale);
}

float AModeSelectHUD::DrawWrapped(const FString& Text, const FLinearColor& Color, float X, float Y,
	float MaxW, float Scale, UFont* Font, float LineH)
{
	if (Text.IsEmpty() || MaxW <= 0.0f)
	{
		return 0.0f;
	}

	FString Line;
	float CurY = Y;
	float TW = 0.0f, TH = 0.0f;

	// 한글은 단어 공백이 드물어 공백 단위 줄바꿈이 통하지 않는다 → 문자 단위로 폭을 재며 접는다.
	for (int32 i = 0; i < Text.Len(); ++i)
	{
		const TCHAR Ch = Text[i];
		Line.AppendChar(Ch);
		GetTextSize(Line, TW, TH, Font, Scale);
		if (TW > MaxW && Line.Len() > 1)
		{
			Line.LeftChopInline(1); // 방금 넘친 글자를 빼고 현재 줄을 확정
			DrawText(Line, Color, X, CurY, Font, Scale);
			CurY += LineH;
			Line.Reset();
			Line.AppendChar(Ch); // 넘친 글자는 다음 줄 첫 글자로
		}
	}

	if (!Line.IsEmpty())
	{
		DrawText(Line, Color, X, CurY, Font, Scale);
		CurY += LineH;
	}

	return CurY - Y;
}

void AModeSelectHUD::DrawSessionResult(const FSessionSummary& Sum)
{
	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	if (W <= 0.0f || H <= 0.0f)
	{
		return;
	}

	const float S = FMath::Clamp(H / 1080.0f, 0.7f, 2.5f);
	UFont* const FontLarge = ResolveFont(true);
	UFont* const FontBody = ResolveFont(false);
	float TW = 0.0f, TH = 0.0f;

	// ── 배경 + 중앙 패널 ──
	DrawVerticalGradient(0.0f, 0.0f, W, H, BackdropTop, BackdropBottom, 64);

	const float PanelW = FMath::Min(W * 0.74f, 1180.0f * S);
	const float PanelH = FMath::Min(H * 0.86f, 940.0f * S);
	const float PX = (W - PanelW) * 0.5f;
	const float PY = (H - PanelH) * 0.5f;
	DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, 0.028f), PX, PY, PanelW, PanelH);
	DrawOutlineRect(PX, PY, PanelW, PanelH, CardBorderSel, FMath::Max(1.0f * S, 1.0f));

	const float x = PX + 44.0f * S;
	const float InnerW = PanelW - 88.0f * S;
	float y = PY + 34.0f * S;

	// ── 헤더 (제목 + 모드·난이도) ──
	DrawRect(Accent, x, y + 2.0f * S, 6.0f * S, 42.0f * S);
	DrawText(TEXT("세션 결과"), TextTitle, x + 20.0f * S, y, FontLarge, 1.7f * S);

	const FString ModeTxt = FString::Printf(TEXT("%s · %s"),
		*UModeManager::GetModeDisplayName(Sum.Mode).ToString(),
		*UModeManager::GetDifficultyDisplayName(Sum.Difficulty).ToString());
	GetTextSize(ModeTxt, TW, TH, FontBody, 0.95f * S);
	DrawText(ModeTxt, TextSecondary, x + InnerW - TW, y + 16.0f * S, FontBody, 0.95f * S);

	y += 62.0f * S;
	DrawRect(Divider, x, y, InnerW, FMath::Max(1.5f * S, 1.0f));
	y += 24.0f * S;

	// ── 총점 (왼쪽) + 3축 미터 (오른쪽) ──
	DrawText(TEXT("총점"), TextSecondary, x, y, FontBody, 0.9f * S);
	const FString TotalStr = FString::Printf(TEXT("%.0f"), Sum.Score.TotalScore);
	DrawText(TotalStr, Sum.bNewRecord ? Accent : TextTitle, x, y + 20.0f * S, FontLarge, 2.5f * S);
	GetTextSize(TotalStr, TW, TH, FontLarge, 2.5f * S);

	const float SideX = x + TW + 26.0f * S;
	const float SideY = y + 26.0f * S;
	if (Sum.bNewRecord)
	{
		const FString Badge = TEXT("신기록!");
		float BW = 0.0f, BH = 0.0f;
		GetTextSize(Badge, BW, BH, FontBody, 1.0f * S);
		const float PadX = 14.0f * S;
		const float PillW = BW + PadX * 2.0f;
		const float PillH = BH + 10.0f * S;
		DrawRect(PillReady, SideX, SideY, PillW, PillH);
		DrawText(Badge, PillReadyText, SideX + PadX, SideY + (PillH - BH) * 0.5f, FontBody, 1.0f * S);
		if (Sum.BestTotalScore >= 0.0f)
		{
			DrawText(FString::Printf(TEXT("이전 최고 %.0f"), Sum.BestTotalScore),
				TextSecondary, SideX, SideY + PillH + 6.0f * S, FontBody, 0.8f * S);
		}
	}
	else if (Sum.BestTotalScore >= 0.0f)
	{
		DrawText(FString::Printf(TEXT("최고 %.0f"), Sum.BestTotalScore),
			TextSecondary, SideX, SideY + 4.0f * S, FontBody, 0.95f * S);
	}
	if (Sum.Score.bUncalibrated)
	{
		DrawText(TEXT("※ 미보정 기준값"), Notice, SideX, SideY + 42.0f * S, FontBody, 0.8f * S);
	}

	// 3축 미터 (오른쪽 컬럼)
	const float MeterX = x + InnerW * 0.46f;
	const float MeterW = InnerW * 0.54f;
	float MY = y + 6.0f * S;
	DrawMeter(TEXT("정확도"), Sum.Score.Accuracy, MeterX, MY, MeterW, S, FontBody, FLinearColor(0.36f, 0.62f, 1.0f, 0.9f));
	MY += 34.0f * S;
	DrawMeter(TEXT("효율"), Sum.Score.Efficiency, MeterX, MY, MeterW, S, FontBody, Accent);
	MY += 34.0f * S;
	DrawMeter(TEXT("일관성"), Sum.Score.Consistency, MeterX, MY, MeterW, S, FontBody, FLinearColor(0.42f, 0.85f, 0.55f, 0.9f));

	y += 128.0f * S;
	DrawRect(Divider, x, y, InnerW, FMath::Max(1.0f * S, 1.0f));
	y += 20.0f * S;

	// ── 집계 카운트 ──
	const FString Counts = FString::Printf(TEXT("스윙 %d    컨택 %d    홈런 %d    안타 %d    삼진 %d    볼넷 %d"),
		Sum.SwingCount, Sum.ContactCount, Sum.HomeRunCount, Sum.HitCount, Sum.StrikeoutCount, Sum.WalkCount);
	const float CountScale = FitScale(Counts, FontBody, 0.95f * S, InnerW);
	DrawText(Counts, TextPrimary, x, y, FontBody, CountScale);
	y += 34.0f * S;

	// 비거리 — 야구 콘텐츠의 대표 지표라 눈에 띄게.
	if (Sum.ContactCount > 0)
	{
		const FString CarryLabel = TEXT("최고 비거리 ");
		DrawText(CarryLabel, TextSecondary, x, y + 8.0f * S, FontBody, 0.9f * S);
		float LW = 0.0f, LH = 0.0f;
		GetTextSize(CarryLabel, LW, LH, FontBody, 0.9f * S);

		const FString CarryBig = FString::Printf(TEXT("%.0f m"), Sum.MaxCarryDistanceM);
		DrawText(CarryBig, Accent, x + LW, y, FontLarge, 1.15f * S);
		float BW = 0.0f, BH = 0.0f;
		GetTextSize(CarryBig, BW, BH, FontLarge, 1.15f * S);

		DrawText(FString::Printf(TEXT("평균 %.0f m"), Sum.AvgCarryDistanceM),
			TextSecondary, x + LW + BW + 22.0f * S, y + 8.0f * S, FontBody, 0.9f * S);
		y += 44.0f * S;
	}
	else
	{
		y += 6.0f * S;
	}

	// ── 신체역학 (최근 스윙) ──
	{
		const FString BodyHeader = Sum.bMockBodyMechanics ? TEXT("신체역학  [MOCK]") : TEXT("신체역학");
		DrawText(BodyHeader, Accent, x, y, FontBody, 0.95f * S);
		y += 26.0f * S;

		if (Sum.BodyMechanics.bValid)
		{
			const FBodyMechanicsMetrics& BM = Sum.BodyMechanics;
			const FString BodyLine = FString::Printf(
				TEXT("X-factor %.0f°    체중이동 %.0fcm    머리흔들림 %.1fcm    척추 %.0f°    체인 %s    신뢰도 %.2f"),
				BM.HipShoulderSeparationDeg, BM.WeightShiftCm, BM.HeadTravelCm, BM.SpineTiltDeg,
				BM.bKineticChainOrdered ? TEXT("정상") : TEXT("흐트러짐"), BM.Confidence);
			const float BodyScale = FitScale(BodyLine, FontBody, 0.88f * S, InnerW - 12.0f * S);
			DrawText(BodyLine, TextPrimary, x + 12.0f * S, y, FontBody, BodyScale);
		}
		else
		{
			DrawText(TEXT("데이터 없음 (포즈 추적 실패/프레임 부족)"), TextSecondary, x + 12.0f * S, y, FontBody, 0.88f * S);
		}
		y += 34.0f * S;
	}

	// ── 기록 / 추세 (이번 판 vs 과거 저장 기록) ──
	{
		const FModeStats& St = Sum.PriorStats;
		DrawText(TEXT("기록"), Accent, x, y, FontBody, 0.95f * S);
		y += 26.0f * S;

		if (St.SessionCount <= 0)
		{
			DrawText(TEXT("이번이 첫 기록입니다."), TextSecondary, x + 12.0f * S, y, FontBody, 0.9f * S);
			y += 30.0f * S;
		}
		else
		{
			// 왼쪽: 요약 + 평균 대비 증감
			const FString Line1 = FString::Printf(TEXT("이번 %d번째 세션    평균 %.0f    직전 %.0f    최고 %.0f"),
				St.SessionCount + 1, St.AverageTotal, St.LastTotal, St.BestTotal);
			DrawText(Line1, TextPrimary, x + 12.0f * S, y, FontBody, 0.88f * S);

			const float DAvg = Sum.Score.TotalScore - St.AverageTotal;
			const float DLast = Sum.Score.TotalScore - St.LastTotal;
			const FString DStr = FString::Printf(TEXT("평균 대비 %+.0f   ·   직전 대비 %+.0f"), DAvg, DLast);
			DrawText(DStr, DAvg >= 0.0f ? PillReadyText : Notice, x + 12.0f * S, y + 24.0f * S, FontBody, 0.9f * S);

			// 오른쪽: 최근 추세 미니 바 (마지막 = 이번 세션, 강조)
			TArray<float> Series = St.RecentTotals;
			Series.Add(Sum.Score.TotalScore);
			float MaxV = 1.0f;
			for (float V : Series)
			{
				MaxV = FMath::Max(MaxV, V);
			}
			const int32 NB = Series.Num();
			const float AreaX = x + InnerW * 0.52f;
			const float AreaW = InnerW * 0.48f;
			const float Slot = AreaW / FMath::Max(NB, 1);
			const float BarW = Slot * 0.6f;
			const float BarMaxH = 48.0f * S;
			const float BaseY = y + BarMaxH;
			for (int32 i = 0; i < NB; ++i)
			{
				const float Hh = BarMaxH * FMath::Clamp(Series[i] / MaxV, 0.0f, 1.0f);
				const bool bCurrent = (i == NB - 1);
				const float Bx = AreaX + i * Slot + (Slot - BarW) * 0.5f;
				DrawRect(bCurrent ? Accent : FLinearColor(1.0f, 1.0f, 1.0f, 0.16f), Bx, BaseY - Hh, BarW, Hh);
			}
			DrawText(TEXT("최근 추세"), TextLocked, AreaX, BaseY + 4.0f * S, FontBody, 0.72f * S);

			y += 62.0f * S;
		}
	}

	// ── 약점 (시급한 순 상위 3) ──
	DrawText(TEXT("약점"), Accent, x, y, FontBody, 0.95f * S);
	y += 26.0f * S;
	const int32 ShowN = FMath::Min(Sum.Report.Weaknesses.Num(), 3);
	if (ShowN == 0)
	{
		DrawText(Sum.Report.bValid ? TEXT("두드러진 약점 없음 — 안정적입니다.") : TEXT("표본이 부족합니다. 더 스윙해 보세요."),
			TextSecondary, x + 12.0f * S, y, FontBody, 0.9f * S);
		y += 28.0f * S;
	}
	for (int32 i = 0; i < ShowN; ++i)
	{
		const FWeakness& Wk = Sum.Report.Weaknesses[i];
		const FString Head = FString::Printf(TEXT("%d) %s"),
			i + 1, *UWeaknessDetector::GetAxisDisplayName(Wk.Axis).ToString());
		DrawText(Head, TextPrimary, x + 12.0f * S, y, FontBody, 0.92f * S);
		float HW = 0.0f, HH = 0.0f;
		GetTextSize(Head, HW, HH, FontBody, 0.92f * S);
		const float EvX = x + 12.0f * S + HW + 14.0f * S;
		const float EvH = DrawWrapped(Wk.Evidence, TextSecondary, EvX, y, InnerW - (EvX - x), 0.85f * S, FontBody, 22.0f * S);
		y += FMath::Max(EvH, 24.0f * S) + 4.0f * S;
	}
	y += 8.0f * S;

	// ── 훈련 추세 (과거 세션 기반 만성 약점) ──
	if (Sum.Chronic.bValid && Sum.Chronic.Trends.Num() > 0)
	{
		DrawText(FString::Printf(TEXT("훈련 추세 (최근 %d세션)"), Sum.Chronic.SessionsAnalyzed),
			Accent, x, y, FontBody, 0.95f * S);
		y += 26.0f * S;

		const int32 TrendN = FMath::Min(Sum.Chronic.Trends.Num(), 3);
		for (int32 i = 0; i < TrendN; ++i)
		{
			const FAxisTrend& Tr = Sum.Chronic.Trends[i];

			// 추세에 따라 색을 달리한다 — 개선은 긍정색(초록), 악화는 주의색(주황).
			FLinearColor TrendColor = TextSecondary;
			switch (Tr.Trend)
			{
			case EWeaknessTrend::Improving: TrendColor = PillReadyText; break;
			case EWeaknessTrend::Worsening: TrendColor = Notice; break;
			default: break;
			}

			const FString ChronicTag = Tr.bChronic ? TEXT(" · 만성") : TEXT("");
			const FString Line = FString::Printf(TEXT("· %s: %d/%d세션 · %s%s"),
				*UWeaknessDetector::GetAxisDisplayName(Tr.Axis).ToString(),
				Tr.AppearanceCount, Tr.WindowSize,
				*UWeaknessDetector::GetTrendDisplayName(Tr.Trend).ToString(),
				*ChronicTag);
			DrawText(Line, TrendColor, x + 12.0f * S, y, FontBody, 0.88f * S);
			y += 24.0f * S;
		}
		y += 8.0f * S;
	}

	// ── 추천 드릴 ──
	DrawText(TEXT("추천 드릴"), Accent, x, y, FontBody, 0.95f * S);
	y += 26.0f * S;
	for (int32 i = 0; i < Sum.Drills.Num(); ++i)
	{
		const FTrainingDrill& D = Sum.Drills[i];
		// 이름 옆에 수행량(3 sets x 15 reps)을 붙인다 — 여기가 결과 화면의 정본이고,
		// AI 코칭 문장이 인용하는 값과 반드시 같아야 한다 (둘 다 카탈로그에서 온다).
		const FString Head = D.Prescription.IsEmpty()
			? FString::Printf(TEXT("· %s"), *D.Name)
			: FString::Printf(TEXT("· %s (%s)"), *D.Name, *D.Prescription);
		DrawText(Head, TextPrimary, x + 12.0f * S, y, FontBody, 0.92f * S);
		float HW = 0.0f, HH = 0.0f;
		GetTextSize(Head, HW, HH, FontBody, 0.92f * S);
		const float DX = x + 12.0f * S + HW + 14.0f * S;
		// 수행 방법 + "무엇이 좋아지는지"를 이어 붙여 한 문단으로 감싼다.
		FString Body = D.Description;
		if (!D.Benefit.IsEmpty())
		{
			Body += FString::Printf(TEXT(" - %s"), *D.Benefit);
		}
		const float DH = DrawWrapped(Body, TextSecondary, DX, y, InnerW - (DX - x), 0.85f * S, FontBody, 22.0f * S);
		y += FMath::Max(DH, 24.0f * S) + 4.0f * S;
	}
	y += 8.0f * S;

	// ── AI 코치 ──
	const FString Coach = Sum.bAwaitingCoaching ? TEXT("AI 코칭 생성 중...") : Sum.CoachingText;
	if (!Coach.IsEmpty())
	{
		DrawText(TEXT("AI 코치"), Notice, x, y, FontBody, 0.95f * S);
		y += 26.0f * S;
		DrawWrapped(Coach, TextPrimary, x + 12.0f * S, y, InnerW - 12.0f * S, 0.9f * S, FontBody, 26.0f * S);
	}

	// ── 푸터 조작 안내 ──
	const float FooterY = PY + PanelH - 52.0f * S;
	DrawRect(Divider, x, FooterY - 14.0f * S, InnerW, FMath::Max(1.0f * S, 1.0f));
	float HintX = x;
	HintX += DrawKeyHint(TEXT("Space"), TEXT("계속"), HintX, FooterY, S, FontBody);
	HintX += DrawKeyHint(TEXT("R"), TEXT("다시"), HintX, FooterY, S, FontBody);
	HintX += DrawKeyHint(TEXT("M"), TEXT("모드 선택"), HintX, FooterY, S, FontBody);
	HintX += DrawKeyHint(TEXT("F"), TEXT("분석 갱신"), HintX, FooterY, S, FontBody);
}

void AModeSelectHUD::DrawOverallScore(float X, float Y, float PanelW, float S, UFont* FontLarge, UFont* FontBody)
{
	UModeManager* MM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr;
	if (!MM)
	{
		return;
	}

	// ⚠️ 여기서 ComputeOverall 을 직접 부르지 않는다 — DrawHUD 는 매 프레임 돌고
	//    ComputeOverall 은 저장 이력 전체를 4번 훑으며 카테고리 배열도 매번 할당한다.
	//    ModeManager 가 이력 버전으로 캐시하므로, 세션이 저장된 뒤 첫 호출에서만 계산된다.
	const FOverallScore& Overall = MM->GetOverallScore();

	float TW = 0.0f, TH = 0.0f;

	// ── 카드 배경 ──
	// ⚠️ 이 자리는 구장 워터마크와 겹친다(워터마크 중심 W*0.79, 반지름 H*0.27). 알파가 낮아도
	//    파울선·베이스 사각형이 점수 글자를 관통해 읽기 나쁘다. 배경 카드를 깔아 뒤를 가린다.
	//    위치를 옮겨 피하는 방법도 있지만, 카드를 두면 해상도·종횡비가 바뀌어도 안전하고
	//    종합 점수가 하나의 덩어리로 읽혀 정보 구조도 더 분명해진다.
	//
	// 높이는 아래 레이아웃과 **같은 상수로** 미리 계산한다 (레이아웃을 바꾸면 여기도 같이).
	const float TotalScale = 2.2f * S;
	float TotalW = 0.0f, TotalH = 0.0f;
	GetTextSize(TEXT("00"), TotalW, TotalH, FontLarge, TotalScale);

	const float Pad = 18.0f * S;
	const float CardH = Overall.bValid
		? (30.0f * S + TotalH + 6.0f * S + 24.0f * S
			+ (Overall.bUncalibrated ? 22.0f * S : 0.0f)
			+ 8.0f * S + 2.0f * 42.0f * S + 4.0f * S
			+ Overall.Categories.Num() * 21.0f * S)
		: (30.0f * S + 26.0f * S);

	DrawRect(FLinearColor(0.03f, 0.05f, 0.09f, 0.72f), X - Pad, Y - Pad * 0.6f, PanelW + Pad * 2.0f, CardH + Pad);
	DrawOutlineRect(X - Pad, Y - Pad * 0.6f, PanelW + Pad * 2.0f, CardH + Pad,
		FLinearColor(1.0f, 1.0f, 1.0f, 0.08f), FMath::Max(1.0f * S, 1.0f));

	float y = Y;

	DrawText(TEXT("종합"), Accent, X, y, FontBody, 0.95f * S);
	y += 30.0f * S;

	if (!Overall.bValid)
	{
		// 기록이 없을 때 0 점을 띄우지 않는다 — "0점"과 "아직 안 함"은 완전히 다른 말이다.
		DrawText(TEXT("기록 없음 — 아무 종목이나 시작하세요"), TextSecondary, X, y, FontBody, 0.85f * S);
		return;
	}

	// 대표 숫자 = 실시한 종목 기준 100점 환산.
	const FString TotalStr = FString::Printf(TEXT("%.0f"), Overall.Total);
	DrawText(TotalStr, TextTitle, X, y, FontLarge, 2.2f * S);
	GetTextSize(TotalStr, TW, TH, FontLarge, 2.2f * S);
	DrawText(TEXT("/ 100"), TextSecondary, X + TW + 10.0f * S, y + TH * 0.45f, FontBody, 0.9f * S);
	y += TH + 6.0f * S;

	// 완료도 — 총점이 "왜 이 숫자인지"를 설명하는 값이라 총점 바로 밑에 붙인다.
	DrawText(FString::Printf(TEXT("완료 %d / %d 종목  ·  실시 종목 기준 환산"),
		Overall.PlayedCount, Overall.CategoryCount), TextSecondary, X, y, FontBody, 0.8f * S);
	y += 24.0f * S;

	if (Overall.bUncalibrated)
	{
		// ⚠️ 반드시 남긴다 — 100점 만점은 정밀해 보이지만 기준 상수는 아직 실측 보정 전이다.
		DrawText(TEXT("* 점수 기준 미보정 — 참고용"), Notice, X, y, FontBody, 0.75f * S);
		y += 22.0f * S;
	}
	y += 8.0f * S;

	// 공격/수비 소계 (절대 점수 — 각 50 만점).
	auto DrawSubtotal = [&](const TCHAR* Label, float Points, float MaxPoints, const FLinearColor& Color)
	{
		DrawText(FString::Printf(TEXT("%s  %.1f / %.0f"), Label, Points, MaxPoints),
			TextPrimary, X, y, FontBody, 0.88f * S);
		y += 20.0f * S;

		const float BarW = PanelW;
		const float BarH = 8.0f * S;
		DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, 0.10f), X, y, BarW, BarH);
		const float Ratio = (MaxPoints > KINDA_SMALL_NUMBER) ? FMath::Clamp(Points / MaxPoints, 0.0f, 1.0f) : 0.0f;
		DrawRect(Color, X, y, BarW * Ratio, BarH);
		y += BarH + 14.0f * S;
	};

	DrawSubtotal(TEXT("공격"), Overall.OffensePoints, Overall.OffenseMaxPoints, Accent);
	DrawSubtotal(TEXT("수비"), Overall.DefensePoints, Overall.DefenseMaxPoints,
		FLinearColor(0.36f, 0.62f, 1.0f, 0.9f));

	// 종목별 — 미실시는 "—" 로. 무엇을 더 하면 되는지가 한눈에 보여야 다음 종목으로 간다.
	y += 4.0f * S;
	for (const FOverallCategoryScore& Cat : Overall.Categories)
	{
		const bool bPlayed = Cat.bPlayed;
		const FLinearColor Col = bPlayed ? TextSecondary : FLinearColor(0.45f, 0.48f, 0.53f, 1.0f);

		FString Line;
		if (bPlayed)
		{
			Line = FString::Printf(TEXT("%s   %.0f   (%s)"), *Cat.DisplayName, Cat.BestScore,
				*UModeManager::GetDifficultyDisplayName(Cat.BestDifficulty).ToString());
		}
		else
		{
			Line = FString::Printf(TEXT("%s   —   미실시"), *Cat.DisplayName);
		}
		DrawText(Line, Col, X + 4.0f * S, y, FontBody, 0.82f * S);
		y += 21.0f * S;
	}
}
