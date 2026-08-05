#include "UI/ModeSelectHUD.h"
#include "Core/ModeSelectPawn.h"
#include "Core/ModeManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

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

	// 모드 플레이 중이면 그리지 않는다 (빙의된 폰으로 판단).
	AModeSelectPawn* SelectPawn = Cast<AModeSelectPawn>(GetOwningPawn());
	if (!SelectPawn)
	{
		bCursorInitialized = false; // 다음 복귀 때 커서가 미끄러져 들어오지 않도록 초기화.
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

	// 부제는 단계(모드/난이도)에 따라 폰이 정한다.
	const FString Subtitle = SelectPawn->GetHeaderSubtitle().ToString();
	const float SubScale = FitScale(Subtitle, FontBody, 0.95f * S, ContentW);
	GetTextSize(Subtitle, TW, TH, FontBody, SubScale);
	const float SubY = TitleY + TitleH + 10.0f * S;
	DrawText(Subtitle, TextSecondary, ContentX + 24.0f * S, SubY, FontBody, SubScale);

	// 구분선
	const float DividerY = SubY + TH + 26.0f * S;
	DrawRect(Divider, ContentX, DividerY, ContentW, FMath::Max(1.5f * S, 1.0f));

	// ── 선택 목록 (모드 단계 또는 난이도 단계 — 폰이 행 데이터를 준다) ──
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

		// 행 이름 (모드 이름 또는 난이도 이름)
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
