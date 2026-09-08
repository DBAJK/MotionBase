#include "UI/VREndCardMenu.h"
#include "UI/VRInfoPanel.h"
#include "Components/TextRenderComponent.h"

int32 FVREndCardMenu::Update(const UVRInfoPanel* Panel, int32 InFirstRow, int32 InCardCount,
	const FVector& AimOrigin, const FVector& AimDir, bool bTracked, float DeltaSeconds)
{
	FirstRow = InFirstRow;

	if (!Panel || InCardCount <= 0 || !bTracked)
	{
		HoverCard = INDEX_NONE;
		DwellTimer = 0.0f;
		return INDEX_NONE;
	}

	// 가장 가깝게 겨눈 카드 하나만 고른다 — 두 카드가 세로로 붙어 있어 각도 안에 둘 다
	// 들어올 수 있는데, 그때 임의로 잡으면 커서가 카드 사이에서 튄다.
	const float CosThresh = FMath::Cos(FMath::DegreesToRadians(AngleDeg));
	const FVector Aim = AimDir.GetSafeNormal();

	int32 Best = INDEX_NONE;
	float BestCos = CosThresh;
	for (int32 i = 0; i < InCardCount; ++i)
	{
		const UTextRenderComponent* Card = Panel->GetRowText(FirstRow + i);
		if (!Card || !Card->IsVisible()) { continue; }

		const FVector Dir = (Card->GetComponentLocation() - AimOrigin).GetSafeNormal();
		const float C = FVector::DotProduct(Aim, Dir);
		if (C > BestCos) { BestCos = C; Best = i; }
	}

	// 다른 카드로 옮겨가면 진행을 처음부터 — 스치며 지나간 시간이 쌓여 오선택되면 안 된다.
	if (Best != HoverCard)
	{
		HoverCard = Best;
		DwellTimer = 0.0f;
	}

	if (HoverCard == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	DwellTimer += DeltaSeconds;
	if (DwellTimer >= DwellSec)
	{
		const int32 Chosen = HoverCard;
		Reset(); // 확정 직후 비워, 폰이 화면을 바꾸기 전 한 프레임 더 확정되지 않게.
		return Chosen;
	}
	return INDEX_NONE;
}

FString FVREndCardMenu::Label(int32 Card, const TCHAR* Text) const
{
	if (HoverCard != Card)
	{
		return FString::Printf(TEXT("  %s"), Text);
	}

	constexpr int32 Cells = 6;
	const int32 Filled = FMath::Clamp(FMath::RoundToInt(Progress() * Cells), 0, Cells);
	return FString::Printf(TEXT("> %s   [%s%s]"), Text,
		*FString::ChrN(Filled, TEXT('=')), *FString::ChrN(Cells - Filled, TEXT('.')));
}

FColor FVREndCardMenu::Color(int32 Card) const
{
	if (HoverCard != Card)
	{
		return FColor(185, 192, 208);
	}

	// 주황(겨눔 시작) → 초록(확정 직전). 색만 봐도 얼마나 남았는지 읽힌다.
	const float A = Progress();
	return FColor(
		static_cast<uint8>(FMath::Lerp(255.0f, 120.0f, A)),
		static_cast<uint8>(FMath::Lerp(190.0f, 240.0f, A)),
		static_cast<uint8>(FMath::Lerp(90.0f, 140.0f, A)));
}
