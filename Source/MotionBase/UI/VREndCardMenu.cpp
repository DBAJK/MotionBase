#include "UI/VREndCardMenu.h"

int32 FVREndCardMenu::UpdateTargets(TConstArrayView<FVector> CardLocations,
	const FVector& AimOrigin, const FVector& AimDir, bool bTracked, float DeltaSeconds)
{
	if (CardLocations.Num() <= 0 || !bTracked)
	{
		HoverCard = INDEX_NONE;
		DwellTimer = 0.0f;
		return INDEX_NONE;
	}

	// 가장 가깝게 겨눈 카드 하나만 고른다 — 두 카드가 나란히 붙어 있어 각도 안에 둘 다
	// 들어올 수 있는데, 그때 임의로 잡으면 커서가 카드 사이에서 튄다.
	const float CosThresh = FMath::Cos(FMath::DegreesToRadians(AngleDeg));
	const FVector Aim = AimDir.GetSafeNormal();

	int32 Best = INDEX_NONE;
	float BestCos = CosThresh;
	for (int32 i = 0; i < CardLocations.Num(); ++i)
	{
		const FVector Dir = (CardLocations[i] - AimOrigin).GetSafeNormal();
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
