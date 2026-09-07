#include "UI/VRInfoPanel.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** 기본 행 크기 / 겨눌 때 커지는 크기 (cm). 헤드셋 가독성 기준(2.5m 앞). */
	constexpr float VrRowSizeIdle  = 14.0f;
	constexpr float VrRowSizeHover = 16.5f;
	/** 크기 보간 속도 (1/초). 너무 빠르면 튀고, 느리면 반응이 없어 보인다. */
	constexpr float VrRowSizeLerp  = 12.0f;

	/**
	 * 헤드셋 가독성: 너무 어두운 UI 색을 최소 밝기까지 끌어올린다(색상 유지, 명도만 상승).
	 * 어두운 배경에 어두운 회색 텍스트(예: 힌트 110,116,128)가 파묻혀 안 보이던 문제 해결.
	 * 이미 밝은 색(경고 빨강·강조 노랑 등 최대 채널 ≥ 175)은 그대로 둔다.
	 */
	FColor LiftColor(const FColor& In)
	{
		const uint8 MaxCh = FMath::Max3(In.R, In.G, In.B);
		constexpr uint8 MinBright = 180;
		if (MaxCh >= MinBright || MaxCh == 0) { return In; }
		const float Scale = static_cast<float>(MinBright) / static_cast<float>(MaxCh);
		return FColor(
			static_cast<uint8>(FMath::Min(255.0f, In.R * Scale)),
			static_cast<uint8>(FMath::Min(255.0f, In.G * Scale)),
			static_cast<uint8>(FMath::Min(255.0f, In.B * Scale)),
			In.A);
	}
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

	TitleText = CreateText(TEXT("VrTitle"), 16.0f);
	if (TitleText) { TitleText->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f)); }

	RowTexts.Reset();
	RowSizes.Reset();
	RowCache.Reset();
	for (int32 i = 0; i < MaxRows; ++i)
	{
		UTextRenderComponent* Row = CreateText(*FString::Printf(TEXT("VrRow%d"), i), VrRowSizeIdle);
		if (Row) { Row->SetRelativeLocation(FVector(0.0f, 0.0f, RowTopZ - i * RowStepZ)); }
		RowTexts.Add(Row);
		RowSizes.Add(VrRowSizeIdle);
		RowCache.AddDefaulted();
	}

	BackText = CreateText(TEXT("VrBack"), 13.0f);

	// 푸터엔 결과·수치(타격 거리/속도, 포구·송구 판정, 백업 해설)가 들어간다 —
	// 값을 읽기 쉽게 행에 근접한 크기로. (헤드셋 가독성 개선)
	// 행이 6줄까지 내려오면(모드 목록) 가장 아래 카드가 Z=-89 까지 온다 —
	// 푸터는 그보다 더 아래에 둬야 카드 테두리와 겹치지 않는다.
	FooterText = CreateText(TEXT("VrFooter"), 12.0f);
	if (FooterText) { FooterText->SetRelativeLocation(FVector(0.0f, 0.0f, -100.0f)); }

	HintText = CreateText(TEXT("VrHint"), 10.0f);
	if (HintText) { HintText->SetRelativeLocation(FVector(0.0f, 0.0f, -118.0f)); }
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
	if (TitleCache.Update(Text, Color))
	{
		TitleText->SetText(FText::FromString(Text));
		TitleText->SetTextRenderColor(Color);
	}
	TitleText->SetVisibility(true);
}

void UVRInfoPanel::SetRow(int32 Index, const FString& Text, const FColor& Color)
{
	if (!RowTexts.IsValidIndex(Index) || !RowTexts[Index]) { return; }
	if (!RowCache.IsValidIndex(Index) || RowCache[Index].Update(Text, Color))
	{
		RowTexts[Index]->SetText(FText::FromString(Text));
		RowTexts[Index]->SetTextRenderColor(Color);
	}
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
	if (BackCache.Update(Text, Color))
	{
		BackText->SetText(FText::FromString(Text));
		BackText->SetTextRenderColor(LiftColor(Color));
	}
}

void UVRInfoPanel::SetStatusCompact()
{
	// 상태 패널(행이 적은 액션 모드)은 행을 다 쓰지 않아, 기본 배치면 푸터·힌트가
	// 패널 아래쪽(바닥 근처)까지 내려가 헤드셋에서 안 보인다. 요소들을 눈높이 근처
	// 좁은 띠로 모아 준다. (모드 선택 메뉴는 이 함수를 부르지 않아 영향 없음.)
	if (TitleText) { TitleText->SetRelativeLocation(FVector(0.0f, 0.0f, 48.0f)); }
	for (int32 i = 0; i < RowTexts.Num(); ++i)
	{
		if (RowTexts[i]) { RowTexts[i]->SetRelativeLocation(FVector(0.0f, 0.0f, 22.0f - i * 18.0f)); }
	}
	if (FooterText) { FooterText->SetRelativeLocation(FVector(0.0f, 0.0f, -50.0f)); }
	if (HintText)   { HintText->SetRelativeLocation(FVector(0.0f, 0.0f, -68.0f)); }
	BackCardZ = -88.0f;
	if (BackText) { BackText->SetRelativeLocation(FVector(0.0f, 0.0f, BackCardZ)); }
}

void UVRInfoPanel::ShowBackCard(const FString& Label, const FColor& Color)
{
	if (!BackText) { return; }
	BackText->SetRelativeLocation(FVector(0.0f, 0.0f, BackCardZ));
	if (BackCache.Update(Label, Color))
	{
		BackText->SetText(FText::FromString(Label));
		BackText->SetTextRenderColor(LiftColor(Color));
	}
	BackText->SetVisibility(true);
}

void UVRInfoPanel::HideBackCard()
{
	if (BackText) { BackText->SetVisibility(false); }
	BackDwellTimer = 0.0f;
}

bool UVRInfoPanel::UpdateBackDwell(const FVector& AimOrigin, const FVector& AimDir, bool bTracked,
	float DwellSec, float AngleDeg, float DeltaSeconds)
{
	if (!BackText || !BackText->IsVisible())
	{
		BackDwellTimer = 0.0f;
		return false;
	}

	// 조준 광선이 카드를 겨누는가 — 각도(코사인) 판정. 추적이 끊기면 오발동 방지로 호버 아님.
	bool bHovering = false;
	if (bTracked && DwellSec > 0.0f)
	{
		const FVector ToCard = (BackText->GetComponentLocation() - AimOrigin).GetSafeNormal();
		const float C = FVector::DotProduct(AimDir.GetSafeNormal(), ToCard);
		bHovering = (C >= FMath::Cos(FMath::DegreesToRadians(AngleDeg)));
	}

	bool bTriggered = false;
	if (bHovering)
	{
		BackDwellTimer += DeltaSeconds;
		if (BackDwellTimer >= DwellSec)
		{
			BackDwellTimer = 0.0f;
			bTriggered = true;
		}
	}
	else
	{
		BackDwellTimer = 0.0f;
	}

	const float Progress = (DwellSec > 0.0f) ? FMath::Clamp(BackDwellTimer / DwellSec, 0.0f, 1.0f) : 0.0f;

	// 카드 테두리(호버 시 안쪽이 차오른다) + 조준 광선·조준점·드웰 링.
	// (이 '뒤로/EXIT' 카드는 호버·드웰 상호작용 경로라 Phase 2 저빈도화 대상에서 제외 —
	//  Duration=-1(매 프레임)을 그대로 유지한다.)
	DrawCardFrame(BackText, bHovering ? FColor(255, 190, 90) : FColor(120, 130, 150),
		bHovering ? 1.3f : 0.5f, bHovering ? Progress : 0.0f, -1.0f);
	if (bTracked)
	{
		DrawPointerRay(AimOrigin, AimDir, Progress, bHovering);
	}

	return bTriggered;
}

void UVRInfoPanel::SetFooter(const FString& Text, const FColor& Color)
{
	if (!FooterText) { return; }
	if (FooterCache.Update(Text, Color))
	{
		FooterText->SetText(FText::FromString(Text));
		FooterText->SetTextRenderColor(LiftColor(Color));
	}
	FooterText->SetVisibility(true);
}

void UVRInfoPanel::SetHint(const FString& Text, const FColor& Color)
{
	if (!HintText) { return; }
	if (HintCache.Update(Text, Color))
	{
		HintText->SetText(FText::FromString(Text));
		HintText->SetTextRenderColor(LiftColor(Color));
	}
	HintText->SetVisibility(true);
}

UTextRenderComponent* UVRInfoPanel::GetRowText(int32 Index) const
{
	return RowTexts.IsValidIndex(Index) ? RowTexts[Index] : nullptr;
}

// ══ 편안한 배치(VR 멀미 방지) ═════════════════════════════════════════

void UVRInfoPanel::UpdateComfortAnchor(const USceneComponent* Head, float DistanceCm, float HeightCm, float RecenterDeg,
	float YawOffsetDeg)
{
	if (!Head) { return; }

	// 머리(HMD)의 부모(폰) 기준 방위·위치. 패널도 같은 부모라 좌표계가 같다.
	const float   HeadYaw = Head->GetRelativeRotation().Yaw;
	const FVector HeadRel = Head->GetRelativeLocation();

	if (!bAnchorInit)
	{
		AnchorYaw = HeadYaw;   // 진입 순간 '지금 보는 방향' 정면에 배치.
		AnchorPos = HeadRel;
		bAnchorInit = true;
	}
	else if (FMath::Abs(FMath::FindDeltaAngleDegrees(AnchorYaw, HeadYaw)) > RecenterDeg)
	{
		// 크게 돌아봐서 패널을 안 보는 상태 → 정면으로 스냅(안 보는 사이 옮겨져 어지럽지 않다).
		AnchorYaw = HeadYaw;
		AnchorPos = HeadRel;
	}

	// 캡처한 머리 위치(AnchorPos) 앞쪽에 건다 — 그 사이엔 고정이라 머리 흔들림(sway)이
	// 패널로 옮겨오지 않는다. 높이는 흔들리지 않게 HeightCm 로 못박는다(수직 bob 방지).
	// YawOffsetDeg 만큼 옆으로 비켜 정면(공이 오는 길)을 비워둘 수 있다.
	const float PlaceYaw = AnchorYaw + YawOffsetDeg;
	const float Rad = FMath::DegreesToRadians(PlaceYaw);
	SetRelativeLocation(FVector(
		AnchorPos.X + FMath::Cos(Rad) * DistanceCm,
		AnchorPos.Y + FMath::Sin(Rad) * DistanceCm,
		HeightCm));
	SetRelativeRotation(FRotator(0.0f, PlaceYaw, 0.0f));
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
	float Thickness, float FillProgress, float Duration) const
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

	DrawDebugLine(World, P0, P1, Color, false, Duration, 0, Thickness);
	DrawDebugLine(World, P1, P2, Color, false, Duration, 0, Thickness);
	DrawDebugLine(World, P2, P3, Color, false, Duration, 0, Thickness);
	DrawDebugLine(World, P3, P0, Color, false, Duration, 0, Thickness);

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
			DrawDebugLine(World, Base, Base + Span, Color, false, Duration, 0, Thickness * 0.6f);
		}
	}
}

void UVRInfoPanel::DrawChrome(int32 VisibleRowCount, int32 HoverIndex, float DwellProgress, bool bBackVisible) const
{
	const UWorld* World = GetWorld();
	if (!World) { return; }

	// 아무도 호버 중이 아니면 이 함수가 그리는 모든 것(테두리·카드 프레임·구분선)이 완전히
	// 정적이다 — 저빈도로만 다시 그린다. 호버 중엔 그 카드의 드웰 채움이 매 프레임 바뀌므로
	// 예전처럼 매 프레임 그린다(끊기면 "차오르는" 애니메이션이 뚝뚝 끊겨 보인다).
	const bool bAnyHover = (HoverIndex != INDEX_NONE);
	if (!bAnyHover)
	{
		if (World->GetTimeSeconds() < ChromeValidUntilSec) { return; }
		ChromeValidUntilSec = World->GetTimeSeconds() + ChromeRedrawIntervalSec;
	}
	else
	{
		ChromeValidUntilSec = 0.0f; // 호버가 끝나는 즉시 다음 저빈도 판단을 새로 시작하게.
	}
	const float Duration = bAnyHover ? -1.0f : (ChromeRedrawIntervalSec * 1.5f);

	const FColor Frame(60, 70, 88);
	const FColor FrameHover(255, 190, 90);
	const FColor AccentColor(90, 170, 230);

	// ── 바깥 패널 테두리 — 목록 전체를 한 판 위에 올려 '띄워둔 화면'으로 보이게 한다 ──
	{
		const FVector C  = GetComponentLocation();
		const FVector Rt = GetRightVector() * (CardHalfW + 16.0f);
		const FVector Up = GetUpVector();
		const FVector Top = C + Up * 78.0f;
		const FVector Bot = C - Up * 148.0f;

		const FVector A = Top - Rt, B = Top + Rt, D = Bot + Rt, E = Bot - Rt;
		DrawDebugLine(World, A, B, AccentColor, false, Duration, 0, 0.8f);
		DrawDebugLine(World, B, D, Frame,  false, Duration, 0, 0.5f);
		DrawDebugLine(World, D, E, AccentColor, false, Duration, 0, 0.8f);
		DrawDebugLine(World, E, A, Frame,  false, Duration, 0, 0.5f);
	}

	// ── 제목 밑줄 ──
	if (TitleText && TitleText->IsVisible())
	{
		const FVector C  = TitleText->GetComponentLocation();
		const FVector Rt = TitleText->GetRightVector() * (CardHalfW + 10.0f);
		const FVector Dn = TitleText->GetUpVector() * -12.0f;
		DrawDebugLine(World, C - Rt + Dn, C + Rt + Dn, AccentColor, false, Duration, 0, 0.7f);
	}

	// ── 행 카드 프레임 ──
	for (int32 i = 0; i < VisibleRowCount && i < RowTexts.Num(); ++i)
	{
		if (!RowTexts[i] || !RowTexts[i]->IsVisible()) { continue; }
		const bool bHovered = (HoverIndex == i);
		DrawCardFrame(RowTexts[i], bHovered ? FrameHover : Frame,
			bHovered ? 1.2f : 0.4f, bHovered ? DwellProgress : 0.0f, Duration);
	}

	// ── 뒤로 카드 ──
	if (bBackVisible && BackText && BackText->IsVisible())
	{
		const bool bHovered = (HoverIndex == VisibleRowCount);
		DrawCardFrame(BackText, bHovered ? FrameHover : Frame,
			bHovered ? 1.2f : 0.4f, bHovered ? DwellProgress : 0.0f, Duration);
	}

	// ── 푸터 구분선 ──
	if (FooterText && FooterText->IsVisible())
	{
		const FVector C  = FooterText->GetComponentLocation();
		const FVector Rt = FooterText->GetRightVector() * (CardHalfW + 10.0f);
		const FVector Up = FooterText->GetUpVector() * 14.0f;
		DrawDebugLine(World, C - Rt + Up, C + Rt + Up, Frame, false, Duration, 0, 0.4f);
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
