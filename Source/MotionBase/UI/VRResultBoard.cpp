#include "UI/VRResultBoard.h"
#include "UI/VRInfoPanel.h"
#include "UI/VREndCardMenu.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Font.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// ── 기하 (cm) ──
	// 보드는 VrPanel 자리(눈 앞 DefaultDistanceCm)에 서고, 눈은 보드 로컬 (-R, 0) 에 있다.
	constexpr float BoardRadius = UVRInfoPanel::DefaultDistanceCm;

	/** 3면 배치 인덱스. */
	constexpr int32 SegLeft   = 0;
	constexpr int32 SegCenter = 1;
	constexpr int32 SegRight  = 2;

	constexpr float CenterW = 120.0f;
	constexpr float WingW   = 104.0f;
	constexpr float PlateH  = 128.0f;
	constexpr float SegGap  = 5.0f;

	/** 날개 면의 방위각(rad) — 가운데 판 가장자리 + 틈 + 날개 반폭 만큼 원통을 따라 돈 자리. */
	const float WingYawRad = (CenterW * 0.5f + SegGap + WingW * 0.5f) / BoardRadius;

	// 날개 안쪽 여백 기준 — 글자가 판 테두리에 붙지 않게.
	constexpr float WingPadU  = 44.0f;   // 왼쪽 끝 U = -44, 오른쪽 끝 U = +44
	constexpr float WingTextW = 88.0f;

	/** 큰 점수 — 가운데 판 왼쪽으로 살짝 치우쳐 "/ 100" 이 옆에 붙을 자리를 남긴다. "100" 도 판 안에 들어가는 크기. */
	constexpr float ScoreU    = -12.0f;
	constexpr float ScoreZ    = 14.0f;
	constexpr float ScoreSize = 42.0f;

	constexpr float MeterTopZ   = 28.0f;
	constexpr float MeterStepZ  = 26.0f;
	constexpr float MeterBarDz  = -9.0f;
	constexpr float MeterBarH   = 3.6f;

	constexpr float ButtonZ     = -86.0f;
	constexpr float ButtonU     = 31.0f;
	constexpr float ButtonW     = 58.0f;
	constexpr float ButtonH     = 20.0f;

	// 판 두께 방향 레이어 (눈 쪽 +). 같은 깊이에 겹치면 z-fighting 이 난다.
	constexpr float DepthPlate = 0.0f;
	constexpr float DepthStrip = 0.8f;
	constexpr float DepthTrack = 1.0f;
	constexpr float DepthFill  = 1.6f;
	constexpr float DepthText  = 2.6f;
	constexpr float PlateThick = 1.0f;

	/** 등장 애니메이션 길이 / 이 시간 전에는 버튼 겨눔을 받지 않는다 (들고 있던 손으로 오선택 방지). */
	constexpr float IntroSec      = 0.9f;
	constexpr float InputDelaySec = 0.6f;

	// ── 색 ── (모드 선택 HUD 의 야간 구장 톤과 맞춘다)
	const FLinearColor PlateColor      (0.010f, 0.016f, 0.032f);
	const FLinearColor TrackColor      (0.070f, 0.085f, 0.120f);
	const FLinearColor AccentLinear    (1.000f, 0.600f, 0.120f);
	const FLinearColor WingStripLinear (0.160f, 0.380f, 0.800f);
	const FLinearColor ButtonIdle      (0.045f, 0.060f, 0.100f);
	const FLinearColor ButtonHover     (0.420f, 0.240f, 0.040f);
	const FLinearColor ButtonDone      (0.120f, 0.460f, 0.200f);
	const FLinearColor FillLinear      (0.300f, 0.900f, 0.450f);

	const FColor TextTitle (242, 246, 255);
	const FColor TextBody  (214, 222, 238);
	const FColor TextMuted (150, 162, 184);
	const FColor TextBlue  (150, 200, 255);
	const FColor TextAmber (255, 196,  70);
	const FColor TextNotice(255, 146,  92);

	/** 글리프 폭 추정 (글자 크기 대비). 한글·한자는 정사각, 라틴은 좁다. 넘치면 SetLine 이 줄여 맞춘다. */
	float GlyphEm(TCHAR C)
	{
		if ((C >= 0xAC00 && C <= 0xD7A3) || (C >= 0x1100 && C <= 0x11FF) || (C >= 0x3130 && C <= 0x318F)
			|| (C >= 0x4E00 && C <= 0x9FFF) || (C >= 0xFF00 && C <= 0xFFEF))
		{
			return 0.95f;
		}
		if (C == TEXT(' '))
		{
			return 0.32f;
		}
		return (FChar::IsUpper(C) || FChar::IsDigit(C)) ? 0.62f : 0.52f;
	}

	/**
	 * TextRender 는 자동 줄바꿈이 없다 → 추정 폭으로 접는다. 가능하면 공백에서 끊는다
	 * (한글도 어절 사이는 띄어 쓰므로, 글자 수로 자르던 예전 방식보다 읽기 편하다).
	 */
	TArray<FString> WrapByWidth(const FString& In, float MaxEm, int32 MaxLines)
	{
		TArray<FString> Lines;
		const FString Src = In.Replace(TEXT("\r"), TEXT(""));
		const int32 Len = Src.Len();
		int32 i = 0;

		while (i < Len && Lines.Num() < MaxLines)
		{
			while (i < Len && Src[i] == TEXT(' ')) { ++i; }
			if (i >= Len) { break; }

			float Em = 0.0f;
			int32 j = i;
			int32 LastSpace = INDEX_NONE;
			while (j < Len && Src[j] != TEXT('\n'))
			{
				const float W = GlyphEm(Src[j]);
				if (Em + W > MaxEm && j > i) { break; }
				if (Src[j] == TEXT(' ')) { LastSpace = j; }
				Em += W;
				++j;
			}

			int32 End = j;
			const bool bOverflow = (j < Len && Src[j] != TEXT('\n'));
			if (bOverflow && LastSpace != INDEX_NONE && LastSpace > i + (j - i) / 2)
			{
				End = LastSpace;
			}

			Lines.Add(Src.Mid(i, End - i).TrimEnd());
			i = End;
			if (i < Len && Src[i] == TEXT('\n')) { ++i; }
		}

		while (i < Len && (Src[i] == TEXT(' ') || Src[i] == TEXT('\n'))) { ++i; }
		if (i < Len && Lines.Num() > 0)
		{
			Lines.Last().Append(TEXT("..."));
		}
		return Lines;
	}

	float EaseOutCubic(float T)
	{
		const float U = 1.0f - FMath::Clamp(T, 0.0f, 1.0f);
		return 1.0f - U * U * U;
	}
}

UVRResultBoard::UVRResultBoard()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 에셋 로드는 생성자 컨텍스트에서만 가능 — UVRInfoPanel 과 같은 KRFont, AFielderMarker 와 같은 기본 도형.
	static ConstructorHelpers::FObjectFinder<UFont> KRFontFinder(TEXT("/Game/Fonts/KRFont.KRFont"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMatFinder(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	BoardFont     = KRFontFinder.Succeeded() ? KRFontFinder.Object : nullptr;
	CubeMesh      = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	ShapeMaterial = ShapeMatFinder.Succeeded() ? ShapeMatFinder.Object : nullptr;
}

UMaterialInstanceDynamic* UVRResultBoard::MakeMaterial(const FLinearColor& Color)
{
	if (!ShapeMaterial) { return nullptr; }
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(ShapeMaterial, this);
	if (Mid)
	{
		// BasicShapeMaterial 의 색 파라미터 이름은 "Color" (AFielderMarker 와 동일).
		Mid->SetVectorParameterValue(TEXT("Color"), Color);
	}
	return Mid;
}

UTextRenderComponent* UVRResultBoard::MakeText(const TCHAR* Name, EHorizTextAligment Align)
{
	AActor* Owner = GetOwner();
	UTextRenderComponent* T = NewObject<UTextRenderComponent>(Owner ? (UObject*)Owner : (UObject*)this, Name);
	if (!T) { return nullptr; }

	T->SetHorizontalAlignment(Align);
	T->SetVerticalAlignment(EVRTA_TextCenter);
	if (BoardFont) { T->SetFont(BoardFont); }
	T->SetCastShadow(false);
	T->SetVisibility(false);

	// 런타임 생성 컴포넌트는 등록 후 부착한다 (SetupAttachment 는 생성자 전용).
	T->RegisterComponent();
	T->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	AllParts.Add(T);
	return T;
}

UStaticMeshComponent* UVRResultBoard::MakeQuad(const TCHAR* Name, UMaterialInstanceDynamic* Material)
{
	AActor* Owner = GetOwner();
	UStaticMeshComponent* Q = NewObject<UStaticMeshComponent>(Owner ? (UObject*)Owner : (UObject*)this, Name);
	if (!Q) { return nullptr; }

	if (CubeMesh) { Q->SetStaticMesh(CubeMesh); }
	if (Material) { Q->SetMaterial(0, Material); }
	Q->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Q->SetGenerateOverlapEvents(false);
	Q->SetCastShadow(false);
	Q->SetVisibility(false);

	Q->RegisterComponent();
	Q->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	AllParts.Add(Q);
	return Q;
}

void UVRResultBoard::BuildBoard()
{
	if (bBuilt) { return; }
	bBuilt = true;

	if (!BoardFont)
	{
		BoardFont = LoadObject<UFont>(nullptr, TEXT("/Game/Fonts/KRFont.KRFont"));
	}

	UMaterialInstanceDynamic* const PlateMat = MakeMaterial(PlateColor);
	UMaterialInstanceDynamic* const TrackMat = MakeMaterial(TrackColor);
	UMaterialInstanceDynamic* const AccentMat = MakeMaterial(AccentLinear);
	UMaterialInstanceDynamic* const WingStripMat = MakeMaterial(WingStripLinear);
	UMaterialInstanceDynamic* const FillMat = MakeMaterial(FillLinear);

	// ── 3면 뒤판 + 윗줄 액센트 ──
	for (int32 Seg = 0; Seg < 3; ++Seg)
	{
		const float W = (Seg == SegCenter) ? CenterW : WingW;

		UStaticMeshComponent* Plate = MakeQuad(*FString::Printf(TEXT("ResultPlate%d"), Seg), PlateMat);
		PlaceQuad(Plate, Seg, 0.0f, 0.0f, W, PlateH, DepthPlate);
		Plates.Add(Plate);

		UStaticMeshComponent* Strip = MakeQuad(*FString::Printf(TEXT("ResultStrip%d"), Seg),
			(Seg == SegCenter) ? AccentMat : WingStripMat);
		PlaceQuad(Strip, Seg, 0.0f, PlateH * 0.5f - 1.2f, W, 2.4f, DepthStrip);
		PlateStrips.Add(Strip);
	}

	// ── 가운데: 종목 · 큰 점수 · 기록 · 결과 ──
	HeadingText  = MakeText(TEXT("ResultHeading"), EHTA_Center);
	ScoreText    = MakeText(TEXT("ResultScore"), EHTA_Center);
	ScoreMaxText = MakeText(TEXT("ResultScoreMax"), EHTA_Left);
	RecordText   = MakeText(TEXT("ResultRecord"), EHTA_Center);
	ResultText   = MakeText(TEXT("ResultLine"), EHTA_Center);
	UncalText    = MakeText(TEXT("ResultUncal"), EHTA_Center);

	Place(HeadingText, SegCenter, 0.0f, 50.0f, DepthText, true);
	Place(ScoreText,   SegCenter, ScoreU, ScoreZ, DepthText, true);
	Place(RecordText,  SegCenter, 0.0f, -20.0f, DepthText, true);
	Place(ResultText,  SegCenter, 0.0f, -34.0f, DepthText, true);
	Place(UncalText,   SegCenter, 0.0f, -50.0f, DepthText, true);
	// ScoreMaxText 는 점수 폭에 따라 매 갱신마다 옆에 붙인다 (ApplyAnimated).
	if (ScoreMaxText) { ScoreMaxText->SetWorldSize(11.0f); }

	// ── 버튼 ──
	for (int32 i = 0; i < NumButtons; ++i)
	{
		const float U = (i == 0) ? -ButtonU : ButtonU;

		UMaterialInstanceDynamic* const BtnMat = MakeMaterial(ButtonIdle);
		ButtonPlateMats.Add(BtnMat);

		UStaticMeshComponent* Btn = MakeQuad(*FString::Printf(TEXT("ResultButton%d"), i), BtnMat);
		PlaceQuad(Btn, SegCenter, U, ButtonZ, ButtonW, ButtonH, DepthPlate);
		ButtonPlates.Add(Btn);

		ButtonFills.Add(MakeQuad(*FString::Printf(TEXT("ResultButtonFill%d"), i), FillMat));

		UTextRenderComponent* Label = MakeText(*FString::Printf(TEXT("ResultButtonText%d"), i), EHTA_Center);
		Place(Label, SegCenter, U, ButtonZ + 1.0f, DepthText, true);
		ButtonTexts.Add(Label);
	}

	HintText = MakeText(TEXT("ResultHint"), EHTA_Center);
	Place(HintText, SegCenter, 0.0f, ButtonZ - ButtonH * 0.5f - 8.0f, DepthText, true);

	// ── 왼쪽 날개: 막대 ──
	LeftHeader = MakeText(TEXT("ResultLeftHeader"), EHTA_Left);
	Place(LeftHeader, SegLeft, -WingPadU, 50.0f, DepthText, true);

	for (int32 i = 0; i < MaxMeters; ++i)
	{
		const float Z = MeterTopZ - i * MeterStepZ;

		UTextRenderComponent* Label = MakeText(*FString::Printf(TEXT("ResultMeterLabel%d"), i), EHTA_Left);
		Place(Label, SegLeft, -WingPadU, Z, DepthText, true);
		MeterLabels.Add(Label);

		UTextRenderComponent* Value = MakeText(*FString::Printf(TEXT("ResultMeterValue%d"), i), EHTA_Right);
		Place(Value, SegLeft, WingPadU, Z, DepthText, true);
		MeterValues.Add(Value);

		UStaticMeshComponent* Track = MakeQuad(*FString::Printf(TEXT("ResultMeterTrack%d"), i), TrackMat);
		PlaceQuad(Track, SegLeft, 0.0f, Z + MeterBarDz, WingTextW, MeterBarH, DepthTrack);
		MeterTracks.Add(Track);

		UMaterialInstanceDynamic* const MeterMat = MakeMaterial(AccentLinear);
		MeterFillMats.Add(MeterMat);
		MeterFills.Add(MakeQuad(*FString::Printf(TEXT("ResultMeterFill%d"), i), MeterMat));
	}

	for (int32 i = 0; i < MaxStatLines; ++i)
	{
		UTextRenderComponent* Stat = MakeText(*FString::Printf(TEXT("ResultStat%d"), i), EHTA_Left);
		Place(Stat, SegLeft, -WingPadU, -46.0f - i * 9.0f, DepthText, true);
		StatTexts.Add(Stat);
	}

	// ── 오른쪽 날개: AI 코칭 + 드릴 ──
	RightHeader = MakeText(TEXT("ResultRightHeader"), EHTA_Left);
	Place(RightHeader, SegRight, -WingPadU, 50.0f, DepthText, true);

	for (int32 i = 0; i < MaxCoachLines; ++i)
	{
		UTextRenderComponent* Line = MakeText(*FString::Printf(TEXT("ResultCoach%d"), i), EHTA_Left);
		Place(Line, SegRight, -WingPadU, 36.0f - i * 9.0f, DepthText, true);
		CoachTexts.Add(Line);
	}

	DrillHeader = MakeText(TEXT("ResultDrillHeader"), EHTA_Left);
	Place(DrillHeader, SegRight, -WingPadU, -24.0f, DepthText, true);

	for (int32 i = 0; i < MaxDrillLines; ++i)
	{
		UTextRenderComponent* Line = MakeText(*FString::Printf(TEXT("ResultDrill%d"), i), EHTA_Left);
		Place(Line, SegRight, -WingPadU, -36.0f - i * 10.0f, DepthText, true);
		DrillTexts.Add(Line);
	}
}

void UVRResultBoard::Place(USceneComponent* C, int32 Seg, float U, float Z, float Depth, bool bIsText) const
{
	if (!C) { return; }

	// 면 중심은 눈(-R,0)을 중심으로 한 반지름 R 원 위, 면은 그 자리에서 눈을 향한다.
	const float Theta = (Seg == SegLeft) ? -WingYawRad : (Seg == SegRight ? WingYawRad : 0.0f);
	const float CosT = FMath::Cos(Theta);
	const float SinT = FMath::Sin(Theta);

	const FVector Center(BoardRadius * CosT - BoardRadius, BoardRadius * SinT, 0.0f);
	const FVector Tangent(-SinT, CosT, 0.0f);   // 면 위 '오른쪽'(플레이어 기준)
	const FVector ToEye(-CosT, -SinT, 0.0f);

	C->SetRelativeLocation(Center + Tangent * U + ToEye * Depth + FVector(0.0f, 0.0f, Z));
	// TextRender 는 +X 쪽이 읽히는 면 → 눈을 향하려면 180 을 더한다 (UVRInfoPanel 과 같은 규칙).
	C->SetRelativeRotation(FRotator(0.0f, FMath::RadiansToDegrees(Theta) + (bIsText ? 180.0f : 0.0f), 0.0f));
}

void UVRResultBoard::PlaceQuad(UStaticMeshComponent* Q, int32 Seg, float U, float Z, float W, float H, float Depth) const
{
	if (!Q) { return; }
	Place(Q, Seg, U, Z, Depth, false);
	// 기본 큐브는 한 변 100cm, 피벗은 중심. X = 두께, Y = 폭, Z = 높이.
	Q->SetRelativeScale3D(FVector(PlateThick, FMath::Max(W, 0.01f), FMath::Max(H, 0.01f)) / 100.0f);
}

void UVRResultBoard::PlaceFill(UStaticMeshComponent* Q, int32 Seg, float LeftU, float Z, float FullW, float H,
	float Depth, float Value01) const
{
	if (!Q) { return; }
	const float W = FullW * FMath::Clamp(Value01, 0.0f, 1.0f);
	Q->SetVisibility(bShowing && W > 0.2f);
	PlaceQuad(Q, Seg, LeftU + W * 0.5f, Z, W, H, Depth);
}

void UVRResultBoard::SetLine(UTextRenderComponent* T, const FString& Text, const FColor& Color, float BaseSize, float MaxW) const
{
	if (!T) { return; }

	const bool bHasText = !Text.IsEmpty();
	T->SetVisibility(bShowing && bHasText);
	if (!bHasText) { return; }

	if (T->TextRenderColor != Color)
	{
		T->SetTextRenderColor(Color);
	}

	// 같은 글이면 다시 올리지 않는다 — 폰이 매 틱 Show() 를 부르므로 여기서 걸러야 렌더 상태가 매 프레임 더티되지 않는다.
	// (요소마다 BaseSize 는 고정이라 글이 같으면 크기도 같다.)
	if (T->Text.ToString().Equals(Text, ESearchCase::CaseSensitive))
	{
		return;
	}

	T->SetText(FText::FromString(Text));
	T->SetWorldSize(BaseSize);

	if (MaxW > 0.0f)
	{
		// 로컬 바운드: 글자는 YZ 평면에 놓여 Y 가 폭이다.
		const float W = T->GetTextLocalSize().Y;
		if (W > MaxW)
		{
			T->SetWorldSize(BaseSize * (MaxW / W));
		}
	}
}

void UVRResultBoard::CopyAnchorFrom(const USceneComponent* Anchor)
{
	if (!Anchor) { return; }
	SetRelativeLocationAndRotation(Anchor->GetRelativeLocation(), Anchor->GetRelativeRotation());
}

void UVRResultBoard::SetAllVisible(bool bVisible)
{
	for (USceneComponent* Part : AllParts)
	{
		if (Part) { Part->SetVisibility(bVisible); }
	}
}

void UVRResultBoard::Hide()
{
	if (!bShowing) { return; }
	bShowing = false;
	HoverButton = INDEX_NONE;
	HoverProgress = 0.0f;
	SetAllVisible(false);
}

void UVRResultBoard::Show(const FVRResultBoardData& Data)
{
	if (!bBuilt) { return; }

	if (!bShowing)
	{
		bShowing = true;
		AnimTime = 0.0f;
		WrappedCoachSource.Reset();
		// 판·트랙은 내용과 무관하게 항상 보인다. 글자·채움은 아래 갱신이 개별로 켠다.
		for (UStaticMeshComponent* Q : Plates)       { if (Q) { Q->SetVisibility(true); } }
		for (UStaticMeshComponent* Q : PlateStrips)  { if (Q) { Q->SetVisibility(true); } }
		for (UStaticMeshComponent* Q : ButtonPlates) { if (Q) { Q->SetVisibility(true); } }
	}

	Current = Data;

	// ── 가운데 ──
	SetLine(HeadingText, Current.Heading, TextBlue, 7.5f, CenterW - 12.0f);
	SetLine(RecordText, Current.RecordLine, Current.bNewRecord ? TextAmber : TextMuted, 8.0f, CenterW - 12.0f);
	SetLine(ResultText, Current.ResultLine, TextBody, 8.0f, CenterW - 12.0f);
	SetLine(UncalText, Current.bUncalibrated ? TEXT("* score baseline uncalibrated - for reference") : FString(),
		TextNotice, 5.2f, CenterW - 12.0f);

	// ── 왼쪽 날개 ──
	SetLine(LeftHeader, TEXT("BREAKDOWN"), TextAmber, 6.5f, WingTextW);
	for (int32 i = 0; i < MaxMeters; ++i)
	{
		const bool bHas = Current.Meters.IsValidIndex(i);
		SetLine(MeterLabels[i], bHas ? Current.Meters[i].Label : FString(), TextBody, 6.5f, WingTextW * 0.62f);
		SetLine(MeterValues[i], bHas ? Current.Meters[i].ValueText : FString(), TextTitle, 6.5f, WingTextW * 0.36f);
		if (MeterTracks[i]) { MeterTracks[i]->SetVisibility(bShowing && bHas); }
		if (bHas && MeterFillMats[i])
		{
			MeterFillMats[i]->SetVectorParameterValue(TEXT("Color"), Current.Meters[i].Color);
		}
	}

	const TArray<FString> StatLines = WrapByWidth(Current.StatLine, WingTextW / 5.5f, MaxStatLines);
	for (int32 i = 0; i < MaxStatLines; ++i)
	{
		SetLine(StatTexts[i], StatLines.IsValidIndex(i) ? StatLines[i] : FString(), TextMuted, 5.5f, WingTextW);
	}

	// ── 오른쪽 날개 ──
	SetLine(RightHeader, Current.bAwaitingCoaching ? TEXT("AI COACH  -  thinking...") : TEXT("AI COACH"),
		TextNotice, 6.5f, WingTextW);

	constexpr float CoachSize = 6.5f;
	if (!WrappedCoachSource.Equals(Current.CoachingText, ESearchCase::CaseSensitive))
	{
		WrappedCoachSource = Current.CoachingText;
		WrappedCoachLines = WrapByWidth(Current.CoachingText, WingTextW / CoachSize, MaxCoachLines);
	}
	for (int32 i = 0; i < MaxCoachLines; ++i)
	{
		SetLine(CoachTexts[i], WrappedCoachLines.IsValidIndex(i) ? WrappedCoachLines[i] : FString(),
			TextBody, CoachSize, WingTextW);
	}

	SetLine(DrillHeader, Current.Drills.Num() > 0 ? TEXT("RECOMMENDED DRILLS") : FString(), TextAmber, 6.0f, WingTextW);
	for (int32 i = 0; i < MaxDrillLines; ++i)
	{
		SetLine(DrillTexts[i], Current.Drills.IsValidIndex(i) ? Current.Drills[i] : FString(),
			FColor(255, 214, 150), 6.0f, WingTextW);
	}

	ApplyAnimated();
}

void UVRResultBoard::ApplyAnimated()
{
	if (!bShowing) { return; }

	const float A = EaseOutCubic(AnimTime / IntroSec);

	// ── 점수 카운트업 ──
	const FString ScoreStr = Current.bScoreValid
		? FString::Printf(TEXT("%.0f"), Current.Score * A)
		: FString(TEXT("--"));
	SetLine(ScoreText, ScoreStr, Current.bNewRecord ? TextAmber : TextTitle, ScoreSize, 0.0f);

	if (ScoreText && ScoreMaxText)
	{
		// 점수 폭이 자릿수·카운트업에 따라 바뀌므로 "/ 100" 을 매번 오른쪽 끝에 붙인다.
		const float HalfW = ScoreText->GetTextLocalSize().Y * 0.5f;
		Place(ScoreMaxText, SegCenter, ScoreU + HalfW + 3.0f, ScoreZ - 10.0f, DepthText, true);
		SetLine(ScoreMaxText, Current.bScoreValid ? TEXT("/ 100") : FString(), TextMuted, 11.0f, 0.0f);
	}

	// ── 막대 채움 ──
	for (int32 i = 0; i < MaxMeters; ++i)
	{
		const bool bHas = Current.Meters.IsValidIndex(i);
		const float Z = MeterTopZ - i * MeterStepZ + MeterBarDz;
		PlaceFill(MeterFills[i], SegLeft, -WingTextW * 0.5f, Z, WingTextW, MeterBarH, DepthFill,
			bHas ? Current.Meters[i].Value01 * A : 0.0f);
	}

	// ── 버튼 ──
	static const TCHAR* const Labels[NumButtons] = { TEXT("PLAY AGAIN"), TEXT("BACK TO MENU") };
	for (int32 i = 0; i < NumButtons; ++i)
	{
		const bool bHover = (i == HoverButton);
		const float P = bHover ? HoverProgress : 0.0f;

		if (ButtonPlateMats.IsValidIndex(i) && ButtonPlateMats[i])
		{
			const FLinearColor Col = bHover
				? FMath::Lerp(ButtonHover, ButtonDone, P)
				: ButtonIdle;
			ButtonPlateMats[i]->SetVectorParameterValue(TEXT("Color"), Col);
		}

		const float U = (i == 0) ? -ButtonU : ButtonU;
		const float FillW = ButtonW - 6.0f;
		PlaceFill(ButtonFills[i], SegCenter, U - FillW * 0.5f, ButtonZ - ButtonH * 0.5f + 3.0f,
			FillW, 2.4f, DepthFill, P);

		SetLine(ButtonTexts[i], Labels[i], bHover ? TextTitle : TextBody, 8.0f, ButtonW - 6.0f);
	}

	SetLine(HintText, TEXT("aim at a button and hold"), TextMuted, 5.0f, CenterW);
}

int32 UVRResultBoard::UpdateButtons(FVREndCardMenu& Menu, const FVector& AimOrigin, const FVector& AimDir,
	bool bTracked, float DeltaSeconds)
{
	if (!bShowing)
	{
		Menu.Reset();
		return INDEX_NONE;
	}

	AnimTime += DeltaSeconds;

	// 버튼 판 중심을 겨눔 대상으로 넘긴다.
	TArray<FVector, TInlineAllocator<NumButtons>> Targets;
	for (const UStaticMeshComponent* Btn : ButtonPlates)
	{
		Targets.Add(Btn ? Btn->GetComponentLocation() : GetComponentLocation());
	}

	// 등장 직후엔 겨눔을 받지 않는다 — 마지막 동작의 손이 우연히 버튼 쪽에 있어도 곧장 차오르지 않게.
	const bool bAimAllowed = bTracked && AnimTime >= InputDelaySec;
	const int32 Chosen = Menu.UpdateTargets(Targets, AimOrigin, AimDir, bAimAllowed, DeltaSeconds);

	HoverButton = Menu.HoverCard;
	HoverProgress = Menu.Progress();
	ApplyAnimated();

	// 조준 광선 + 보드 면 위 조준점 — 어디를 겨누는지 손으로 알 수 있게 (UVRInfoPanel::DrawPointerRay 와 같은 표현).
	if (bTracked)
	{
		if (const UWorld* World = GetWorld())
		{
			const FVector Dir = AimDir.GetSafeNormal();
			const FVector Normal = GetForwardVector();
			const float Denom = FVector::DotProduct(Dir, Normal);
			float T = 300.0f;
			bool bHit = false;
			if (FMath::Abs(Denom) > KINDA_SMALL_NUMBER)
			{
				const float Hit = FVector::DotProduct(GetComponentLocation() - AimOrigin, Normal) / Denom;
				if (Hit > 0.0f && Hit < 2000.0f) { T = Hit; bHit = true; }
			}

			const bool bHovering = (HoverButton != INDEX_NONE);
			const FColor RayColor = bHovering ? FColor(255, 190, 90) : FColor(80, 200, 255);
			const FVector End = AimOrigin + Dir * T;
			DrawDebugLine(World, AimOrigin, End, RayColor, false, -1.0f, 0, bHovering ? 0.6f : 0.35f);
			if (bHit)
			{
				DrawDebugPoint(World, End, bHovering ? 9.0f : 6.0f, RayColor, false, -1.0f, 0);
			}
		}
	}

	return Chosen;
}
