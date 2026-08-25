#include "UI/VRInfoPanel.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** 기본 행 크기 / 겨눌 때 커지는 크기 (cm). 헤드셋 가독성 기준. */
	constexpr float VrRowSizeIdle  = 12.0f;
	constexpr float VrRowSizeHover = 14.5f;
	/** 크기 보간 속도 (1/초). 너무 빠르면 튀고, 느리면 반응이 없어 보인다. */
	constexpr float VrRowSizeLerp  = 12.0f;
}

UVRInfoPanel::UVRInfoPanel()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 패널 기본 위치 — 소유 폰이 SetPlacement 로 덮어쓸 수 있다.
	SetRelativeLocation(FVector(DefaultDistanceCm, 0.0f, DefaultHeightCm));

	// 한글 폰트 (Content/Fonts/KRFont). 없으면 엔진 기본으로 폴백(한글 깨질 수 있음).
	// 폰트 로드는 생성자 컨텍스트에서만 가능하므로 여기서 잡아 둔다.
	static ConstructorHelpers::FObjectFinder<UFont> KRFontFinder(TEXT("/Game/Fonts/KRFont.KRFont"));
	PanelFont = KRFontFinder.Succeeded() ? KRFontFinder.Object : nullptr;
}

void UVRInfoPanel::BuildPanel()
{
	if (bBuilt) { return; }
	bBuilt = true;

	// 생성자(CDO) 시점에 폰트가 없었더라도, 나중에 추가된 KRFont 를 런타임에 다시 찾는다.
	// (에디터 재시작 없이 폰트를 넣어도 잡히게 — 없으면 null 로 폴백.)
	if (!PanelFont)
	{
		PanelFont = LoadObject<UFont>(nullptr, TEXT("/Game/Fonts/KRFont.KRFont"));
	}

	SetRelativeLocation(FVector(PendingDistanceCm, 0.0f, PendingHeightCm));

	TitleText = CreateText(TEXT("VrTitle"), 14.0f);
	if (TitleText) { TitleText->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f)); }

	RowTexts.Reset();
	RowSizes.Reset();
	for (int32 i = 0; i < MaxRows; ++i)
	{
		UTextRenderComponent* Row = CreateText(*FString::Printf(TEXT("VrRow%d"), i), VrRowSizeIdle);
		if (Row) { Row->SetRelativeLocation(FVector(0.0f, 0.0f, RowTopZ - i * RowStepZ)); }
		RowTexts.Add(Row);
		RowSizes.Add(VrRowSizeIdle);
	}

	BackText = CreateText(TEXT("VrBack"), 10.0f);

	// 푸터엔 결과·수치(타격 거리/속도, 포구·송구 판정, 백업 해설)가 들어간다 —
	// 값을 읽기 쉽게 행(11)에 근접한 크기로. (헤드셋 가독성 개선)
	// 행이 6줄까지 내려오면(모드 목록) 가장 아래 카드가 Z=-89 까지 온다 —
	// 푸터는 그보다 더 아래에 둬야 카드 테두리와 겹치지 않는다.
	FooterText = CreateText(TEXT("VrFooter"), 10.0f);
	if (FooterText) { FooterText->SetRelativeLocation(FVector(0.0f, 0.0f, -110.0f)); }

	HintText = CreateText(TEXT("VrHint"), 6.0f);
	if (HintText) { HintText->SetRelativeLocation(FVector(0.0f, 0.0f, -132.0f)); }
}

UTextRenderComponent* UVRInfoPanel::CreateText(const TCHAR* Name, float WorldSize)
{
	AActor* Owner = GetOwner();
	UTextRenderComponent* T = NewObject<UTextRenderComponent>(Owner ? (UObject*)Owner : (UObject*)this, Name);
	if (!T) { return nullptr; }

	T->SetHorizontalAlignment(EHTA_Center);
	T->SetVerticalAlignment(EVRTA_TextCenter);
	T->SetWorldSize(WorldSize);
	if (PanelFont) { T->SetFont(PanelFont); }
	T->SetVisibility(false);

	// 런타임 생성 컴포넌트는 등록 후 부착한다 (SetupAttachment 는 생성자 전용).
	T->RegisterComponent();
	T->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	// 텍스트가 카메라를 향하도록 180 회전 (부착 후 설정).
	T->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	return T;
}

void UVRInfoPanel::SetPlacement(float DistanceCm, float HeightCm)
{
	PendingDistanceCm = DistanceCm;
	PendingHeightCm   = HeightCm;
	SetRelativeLocation(FVector(DistanceCm, 0.0f, HeightCm));
}

void UVRInfoPanel::HideAll()
{
	if (TitleText)  { TitleText->SetVisibility(false); }
	if (BackText)   { BackText->SetVisibility(false); }
	if (FooterText) { FooterText->SetVisibility(false); }
	if (HintText)   { HintText->SetVisibility(false); }
	for (UTextRenderComponent* Row : RowTexts)
	{
		if (Row) { Row->SetVisibility(false); }
	}
}

void UVRInfoPanel::SetTitle(const FString& Text, const FColor& Color)
{
	if (!TitleText) { return; }
	TitleText->SetText(FText::FromString(Text));
	TitleText->SetTextRenderColor(Color);
	TitleText->SetVisibility(true);
}

void UVRInfoPanel::SetRow(int32 Index, const FString& Text, const FColor& Color)
{
	if (!RowTexts.IsValidIndex(Index) || !RowTexts[Index]) { return; }
	RowTexts[Index]->SetText(FText::FromString(Text));
	RowTexts[Index]->SetTextRenderColor(Color);
	RowTexts[Index]->SetVisibility(true);
}

void UVRInfoPanel::HideRowsFrom(int32 FirstHiddenIndex)
{
	for (int32 i = FMath::Max(0, FirstHiddenIndex); i < RowTexts.Num(); ++i)
	{
		if (RowTexts[i]) { RowTexts[i]->SetVisibility(false); }
	}
}

void UVRInfoPanel::SetBackBelowRows(int32 RowCount, const FString& Text, const FColor& Color, bool bShow)
{
	if (!BackText) { return; }
	BackText->SetVisibility(bShow);
	if (!bShow) { return; }
	BackText->SetRelativeLocation(FVector(0.0f, 0.0f, RowTopZ - RowCount * RowStepZ - 14.0f));
	BackText->SetText(FText::FromString(Text));
	BackText->SetTextRenderColor(Color);
}

void UVRInfoPanel::SetFooter(const FString& Text, const FColor& Color)
{
	if (!FooterText) { return; }
	FooterText->SetText(FText::FromString(Text));
	FooterText->SetTextRenderColor(Color);
	FooterText->SetVisibility(true);
}

void UVRInfoPanel::SetHint(const FString& Text, const FColor& Color)
{
	if (!HintText) { return; }
	HintText->SetText(FText::FromString(Text));
	HintText->SetTextRenderColor(Color);
	HintText->SetVisibility(true);
}

UTextRenderComponent* UVRInfoPanel::GetRowText(int32 Index) const
{
	return RowTexts.IsValidIndex(Index) ? RowTexts[Index] : nullptr;
}

// ══ 공간감(VR) 연출 ══════════════════════════════════════════════════

void UVRInfoPanel::ApplyCurvedLayout(float EyeOffsetZ, float RadiusCm)
{
	CurveEyeZ     = EyeOffsetZ;
	CurveRadiusCm = FMath::Max(RadiusCm, 1.0f);

	// 눈은 패널 로컬 좌표에서 (-R, 0, EyeOffsetZ) 에 있다 (패널 +X 가 플레이어 반대쪽).
	// 각 요소를 그 눈을 중심으로 한 반지름 R 원통면 위로 끌어당기고, 눈을 바라보게 돌린다.
	auto Place = [this](UTextRenderComponent* T)
	{
		if (!T) { return; }
		const FVector Loc = T->GetRelativeLocation();
		const float dz = Loc.Z - CurveEyeZ;

		// 원통면: 눈에서 수평거리 sqrt(R² - dz²) → 패널 평면(X=0) 기준 오프셋.
		const float Horiz = FMath::Sqrt(FMath::Max(CurveRadiusCm * CurveRadiusCm - dz * dz, 0.0f));
		const float X = Horiz - CurveRadiusCm;   // 항상 0 이하 = 플레이어 쪽으로 당겨진다
		T->SetRelativeLocation(FVector(X, Loc.Y, Loc.Z));

		// 텍스트의 +X 가 눈 쪽을 향해야 읽힌다 (평면일 땐 yaw 180 과 같은 값이 나온다).
		const FVector ToEye(-(CurveRadiusCm + X), -Loc.Y, -dz);
		T->SetRelativeRotation(ToEye.GetSafeNormal().Rotation());
	};

	Place(TitleText);
	for (UTextRenderComponent* Row : RowTexts) { Place(Row); }
	Place(BackText);
	Place(FooterText);
	Place(HintText);
}

void UVRInfoPanel::TickHoverAnim(float DeltaSeconds, int32 HoverIndex, int32 VisibleRowCount)
{
	for (int32 i = 0; i < RowTexts.Num(); ++i)
	{
		if (!RowTexts[i] || !RowSizes.IsValidIndex(i)) { continue; }

		const bool bHovered = (i == HoverIndex) && (i < VisibleRowCount);
		const float Target = bHovered ? VrRowSizeHover : VrRowSizeIdle;

		// 지수 보간 — 프레임레이트가 흔들려도 같은 속도로 수렴한다.
		RowSizes[i] = FMath::FInterpTo(RowSizes[i], Target, DeltaSeconds, VrRowSizeLerp);
		RowTexts[i]->SetWorldSize(RowSizes[i]);
	}
}

void UVRInfoPanel::DrawCardFrame(const UTextRenderComponent* Card, const FColor& Color,
	float Thickness, float FillProgress) const
{
	const UWorld* World = GetWorld();
	if (!World || !Card) { return; }

	const FVector C  = Card->GetComponentLocation();
	const FVector Rt = Card->GetRightVector() * CardHalfW;
	const FVector Up = Card->GetUpVector() * CardHalfH;

	const FVector P0 = C - Rt - Up;   // 좌하
	const FVector P1 = C + Rt - Up;   // 우하
	const FVector P2 = C + Rt + Up;   // 우상
	const FVector P3 = C - Rt + Up;   // 좌상

	DrawDebugLine(World, P0, P1, Color, false, -1.0f, 0, Thickness);
	DrawDebugLine(World, P1, P2, Color, false, -1.0f, 0, Thickness);
	DrawDebugLine(World, P2, P3, Color, false, -1.0f, 0, Thickness);
	DrawDebugLine(World, P3, P0, Color, false, -1.0f, 0, Thickness);

    // 드웰 채움 — 카드 안쪽을 왼쪽부터 가로선 몇 줄로 칠해 "차오른다"를 보여준다.
	if (FillProgress > 0.0f)
	{
		constexpr int32 Scanlines = 5;
		const FVector Left = C - Rt;
		const FVector Span = Rt * 2.0f * FMath::Clamp(FillProgress, 0.0f, 1.0f);
		for (int32 i = 1; i <= Scanlines; ++i)
		{
			const float T = -1.0f + 2.0f * (static_cast<float>(i) / (Scanlines + 1));
			const FVector Base = Left + Card->GetUpVector() * (CardHalfH * T);
			DrawDebugLine(World, Base, Base + Span, Color, false, -1.0f, 0, Thickness * 0.6f);
		}
	}
}

void UVRInfoPanel::DrawChrome(int32 VisibleRowCount, int32 HoverIndex, float DwellProgress, bool bBackVisible) const
{
	const UWorld* World = GetWorld();
	if (!World) { return; }

	const FColor Frame(60, 70, 88);
	const FColor FrameHover(255, 190, 90);
	const FColor Accent(90, 170, 230);

	// ── 바깥 패널 테두리 — 목록 전체를 한 판 위에 올려 '띄워둔 화면'으로 보이게 한다 ──
	{
		const FVector C  = GetComponentLocation();
		const FVector Rt = GetRightVector() * (CardHalfW + 16.0f);
		const FVector Up = GetUpVector();
		const FVector Top = C + Up * 78.0f;
		const FVector Bot = C - Up * 148.0f;

		const FVector A = Top - Rt, B = Top + Rt, D = Bot + Rt, E = Bot - Rt;
		DrawDebugLine(World, A, B, Accent, false, -1.0f, 0, 0.8f);
		DrawDebugLine(World, B, D, Frame,  false, -1.0f, 0, 0.5f);
		DrawDebugLine(World, D, E, Accent, false, -1.0f, 0, 0.8f);
		DrawDebugLine(World, E, A, Frame,  false, -1.0f, 0, 0.5f);
	}

	// ── 제목 밑줄 ──
	if (TitleText && TitleText->IsVisible())
	{
		const FVector C  = TitleText->GetComponentLocation();
		const FVector Rt = TitleText->GetRightVector() * (CardHalfW + 10.0f);
		const FVector Dn = TitleText->GetUpVector() * -12.0f;
		DrawDebugLine(World, C - Rt + Dn, C + Rt + Dn, Accent, false, -1.0f, 0, 0.7f);
	}

	// ── 행 카드 프레임 ──
	for (int32 i = 0; i < VisibleRowCount && i < RowTexts.Num(); ++i)
	{
		if (!RowTexts[i] || !RowTexts[i]->IsVisible()) { continue; }
		const bool bHovered = (HoverIndex == i);
		DrawCardFrame(RowTexts[i], bHovered ? FrameHover : Frame,
			bHovered ? 1.2f : 0.4f, bHovered ? DwellProgress : 0.0f);
	}

	// ── 뒤로 카드 ──
	if (bBackVisible && BackText && BackText->IsVisible())
	{
		const bool bHovered = (HoverIndex == VisibleRowCount);
		DrawCardFrame(BackText, bHovered ? FrameHover : Frame,
			bHovered ? 1.2f : 0.4f, bHovered ? DwellProgress : 0.0f);
	}

	// ── 푸터 구분선 ──
	if (FooterText && FooterText->IsVisible())
	{
		const FVector C  = FooterText->GetComponentLocation();
		const FVector Rt = FooterText->GetRightVector() * (CardHalfW + 10.0f);
		const FVector Up = FooterText->GetUpVector() * 14.0f;
		DrawDebugLine(World, C - Rt + Up, C + Rt + Up, Frame, false, -1.0f, 0, 0.4f);
	}
}

void UVRInfoPanel::DrawPointerRay(const FVector& Origin, const FVector& Dir, float DwellProgress, bool bHovering) const
{
	const UWorld* World = GetWorld();
	if (!World) { return; }

	const FVector Normal = GetForwardVector();          // 패널 면의 법선 (플레이어 반대쪽)
	const FVector PanelC = GetComponentLocation();

	// 광선과 패널 평면의 교점. 평행하거나 뒤쪽이면 고정 길이로만 그린다.
	const float Denom = FVector::DotProduct(Dir, Normal);
	float T = 0.0f;
	bool bHit = false;
	if (FMath::Abs(Denom) > KINDA_SMALL_NUMBER)
	{
		T = FVector::DotProduct(PanelC - Origin, Normal) / Denom;
		bHit = (T > 0.0f && T < 2000.0f);
	}

	const FVector End = Origin + Dir * (bHit ? T : 300.0f);
	const FColor RayColor = bHovering ? FColor(255, 190, 90) : FColor(80, 200, 255);
	DrawDebugLine(World, Origin, End, RayColor, false, -1.0f, 0, bHovering ? 0.6f : 0.35f);

	if (!bHit) { return; }

	// 조준점 — 어디를 겨누는지 손으로 알 수 있게 패널 면 위에 점을 찍는다.
	DrawDebugPoint(World, End, bHovering ? 9.0f : 6.0f, RayColor, false, -1.0f, 0);

	// 드웰 링 — 채워지는 원호가 "지금 고르는 중"을 시간으로 보여준다.
	if (bHovering && DwellProgress > 0.0f)
	{
		constexpr int32 Segments = 24;
		const float Radius = 4.5f;
		const FVector Rt = GetRightVector() * Radius;
		const FVector Up = GetUpVector() * Radius;
		const int32 Filled = FMath::Clamp(FMath::RoundToInt(DwellProgress * Segments), 1, Segments);

		FVector Prev = End + Up; // 12시에서 시작
		for (int32 i = 1; i <= Filled; ++i)
		{
			const float Ang = 2.0f * PI * (static_cast<float>(i) / Segments);
			const FVector P = End + Up * FMath::Cos(Ang) + Rt * FMath::Sin(Ang);
			DrawDebugLine(World, Prev, P, FColor(120, 235, 140), false, -1.0f, 0, 0.5f);
			Prev = P;
		}
	}
}
