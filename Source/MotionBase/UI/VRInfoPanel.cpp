#include "UI/VRInfoPanel.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Font.h"
#include "UObject/ConstructorHelpers.h"

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

	SetRelativeLocation(FVector(PendingDistanceCm, 0.0f, PendingHeightCm));

	TitleText = CreateText(TEXT("VrTitle"), 14.0f);
	if (TitleText) { TitleText->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f)); }

	RowTexts.Reset();
	for (int32 i = 0; i < MaxRows; ++i)
	{
		UTextRenderComponent* Row = CreateText(*FString::Printf(TEXT("VrRow%d"), i), 11.0f);
		if (Row) { Row->SetRelativeLocation(FVector(0.0f, 0.0f, RowTopZ - i * RowStepZ)); }
		RowTexts.Add(Row);
	}

	BackText = CreateText(TEXT("VrBack"), 10.0f);

	// 푸터엔 결과·수치(타격 거리/속도, 포구·송구 판정, 백업 해설)가 들어간다 —
	// 값을 읽기 쉽게 행(11)에 근접한 크기로. (헤드셋 가독성 개선)
	FooterText = CreateText(TEXT("VrFooter"), 10.0f);
	if (FooterText) { FooterText->SetRelativeLocation(FVector(0.0f, 0.0f, -98.0f)); }

	HintText = CreateText(TEXT("VrHint"), 6.0f);
	if (HintText) { HintText->SetRelativeLocation(FVector(0.0f, 0.0f, -120.0f)); }
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

void UVRInfoPanel::SetBackBelowRows(int32 RowCount, const FString& Text, const FColor& Color, bool bVisible)
{
	if (!BackText) { return; }
	BackText->SetVisibility(bVisible);
	if (!bVisible) { return; }
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
