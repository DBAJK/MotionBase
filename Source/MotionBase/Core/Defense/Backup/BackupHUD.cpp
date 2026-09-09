#include "Core/Defense/Backup/BackupHUD.h"
#include "Core/Defense/Backup/BackupPawn.h"
#include "Core/Defense/Backup/BackupPlaybook.h"
#include "Engine/Canvas.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Engine/Engine.h"

namespace
{
	const FLinearColor PanelBg   (0.05f, 0.06f, 0.08f, 0.82f);
	const FLinearColor PanelLine (1.00f, 0.62f, 0.20f, 0.90f);
	const FLinearColor TextMain  (0.92f, 0.94f, 0.97f, 1.00f);
	const FLinearColor TextDim   (0.55f, 0.60f, 0.66f, 1.00f);
	const FLinearColor Good      (0.40f, 0.85f, 0.45f, 1.00f);

	const FLinearColor MapBg      (0.08f, 0.10f, 0.09f, 0.55f);
	const FLinearColor MapGrass   (0.20f, 0.45f, 0.25f, 0.35f);
	const FLinearColor MapDirt    (0.55f, 0.42f, 0.28f, 0.45f);
	const FLinearColor MapBase    (0.85f, 0.85f, 0.80f, 1.00f);
	const FLinearColor MapPosIdle (0.55f, 0.60f, 0.66f, 0.90f);
	const FLinearColor MapPosSelf (1.00f, 0.85f, 0.30f, 1.00f);
	const FLinearColor MapPlayer  (0.30f, 0.75f, 1.00f, 1.00f);
	const FLinearColor MapCorrect (0.40f, 0.90f, 0.47f, 1.00f);
	const FLinearColor MapOther   (0.55f, 0.60f, 0.66f, 0.55f);
	// 판정 전에 후보 존을 전부 같은 색으로 그릴 때 쓴다 (정답을 미리 알려주지 않기 위해).
	const FLinearColor MapNeutral (0.72f, 0.76f, 0.84f, 0.85f);
}

void ABackupHUD::DrawPanel(float X, float Y, float W, float H, const FLinearColor& Fill, const FLinearColor& Border)
{
	DrawRect(Fill, X, Y, W, H);
	const float T = 2.0f;
	DrawRect(Border, X, Y, W, T);
	DrawRect(Border, X, Y + H - T, W, T);
	DrawRect(Border, X, Y, T, H);
	DrawRect(Border, X + W - T, Y, T, H);
}

void ABackupHUD::DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale)
{
	DrawText(Text, Color, X, Y, GEngine->GetMediumFont(), Scale);
}

void ABackupHUD::DrawCentered(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float Scale)
{
	float TW = 0.0f, TH = 0.0f;
	GetTextSize(Text, TW, TH, GEngine->GetMediumFont(), Scale);
	DrawText(Text, Color, CenterX - TW * 0.5f, Y, GEngine->GetMediumFont(), Scale);
}

FVector2D ABackupHUD::WorldToMap(const FVector2D& WorldXY, const FVector2D& MapCenter, float MapRadiusPx, float WorldRadiusCm) const
{
	// 월드 +X(2루/외야 쪽) → 화면 위쪽(-Y), 월드 +Y(1루 쪽) → 화면 오른쪽(+X).
	// 홈(원점)이 미니맵 아래쪽에 오도록 뒤집는다 — 타자가 보는 시야와 같은 방향.
	const float Scale = MapRadiusPx / FMath::Max(WorldRadiusCm, 1.0f);
	return FVector2D(MapCenter.X + WorldXY.Y * Scale, MapCenter.Y - WorldXY.X * Scale);
}

void ABackupHUD::DrawMinimap(ABackupPawn* Pawn, float CenterX, float CenterY, float RadiusPx)
{
	const FBaseballField& Field = Pawn->GetField();
	const FVector2D Center(CenterX, CenterY);
	// 중견수 깊이 + 여유를 미니맵 반지름에 대응시킨다 — 전 포지션이 항상 안에 들어온다.
	const float WorldRadius = Field.OutfieldDepthCm * 1.15f;

	auto ToMap = [&](const FVector& World) { return WorldToMap(FVector2D(World.X, World.Y), Center, RadiusPx, WorldRadius); };

	// 배경 원(구장) + 파울선 (홈에서 ±45° — 다이아몬드 정의 그대로).
	DrawRect(MapBg, CenterX - RadiusPx, CenterY - RadiusPx, RadiusPx * 2.0f, RadiusPx * 2.0f);
	const FVector2D Home = ToMap(Field.GetBaseLocation(EBaseType::Home));
	const FVector2D FoulLeft  = ToMap(FVector(WorldRadius, -WorldRadius, 0.0f));
	const FVector2D FoulRight = ToMap(FVector(WorldRadius, WorldRadius, 0.0f));
	DrawLine(Home.X, Home.Y, FoulLeft.X, FoulLeft.Y, MapGrass, 1.5f);
	DrawLine(Home.X, Home.Y, FoulRight.X, FoulRight.Y, MapGrass, 1.5f);

	// 베이스 4개.
	const EBaseType Bases[4] = { EBaseType::Home, EBaseType::First, EBaseType::Second, EBaseType::Third };
	for (EBaseType B : Bases)
	{
		const FVector2D P = ToMap(Field.GetBaseLocation(B));
		DrawRect(MapBase, P.X - 3.0f, P.Y - 3.0f, 6.0f, 6.0f);
	}

	// 7개 수비 위치 (내 포지션은 강조).
	for (EFieldPosition Pos : UBackupPlaybook::AllPositions())
	{
		const FVector2D P = ToMap(Field.GetFieldingSpot(Pos));
		const bool bSelf = (Pos == Pawn->GetPosition());
		const FLinearColor Col = bSelf ? MapPosSelf : MapPosIdle;
		const float S = bSelf ? 5.0f : 3.0f;
		DrawRect(Col, P.X - S, P.Y - S, S * 2.0f, S * 2.0f);
		// 동료 이름도 같이 띄운다 — 정답 강조를 걷어낸 지금은 "누가 어디 있나"가 판단 근거다.
		DrawCentered(UBackupPlaybook::PositionName(Pos), P.X, P.Y + S + 2.0f, Col, bSelf ? 0.6f : 0.5f);
	}

	// 방향 판단 후보 존. 판정 전에는 전부 중립색 — 정답을 미리 알려주면 판단 훈련이
	// 아니라 "초록 원 따라가기"가 된다. 판정이 끝난 뒤에만 정답=초록으로 공개해 복기시킨다.
	// (월드 존 DrawZones 와 같은 기준: ABackupPawn::IsAnswerRevealed)
	const bool bReveal = Pawn->IsAnswerRevealed();
	const FBackupTrial& Trial = Pawn->GetCurrentTrial();
	for (int32 i = 0; i < Trial.CandidateZones.Num(); ++i)
	{
		const FBackupZone& Z = Trial.CandidateZones[i];
		const bool bCorrect = (i == Trial.CorrectCandidateIndex);
		const FLinearColor Col = bReveal ? (bCorrect ? MapCorrect : MapOther) : MapNeutral;
		const float Thick = bReveal ? (bCorrect ? 2.5f : 1.0f) : 1.4f;

		if (Z.Role == EBackupRole::CutoffRelay)
		{
			const FVector2D A = ToMap(Z.SegmentA);
			const FVector2D Bp = ToMap(Z.SegmentB);
			DrawLine(A.X, A.Y, Bp.X, Bp.Y, Col, Thick);
		}
		else
		{
			const FVector2D P = ToMap(Z.Center);
			const float R = FMath::Max(Z.RadiusCm * (RadiusPx / WorldRadius), 3.0f);
			constexpr int32 Segs = 16;
			FVector2D Prev = P + FVector2D(R, 0);
			for (int32 s = 1; s <= Segs; ++s)
			{
				const float Ang = 2.0f * PI * s / Segs;
				const FVector2D Cur = P + FVector2D(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R);
				DrawLine(Prev.X, Prev.Y, Cur.X, Cur.Y, Col, Thick);
				Prev = Cur;
			}
		}
	}

	// 내 현재 위치.
	const FVector2D MyPos = ToMap(Pawn->GetPlayerXY());
	DrawRect(MapPlayer, MyPos.X - 4.0f, MyPos.Y - 4.0f, 8.0f, 8.0f);
}

void ABackupHUD::DrawHUD()
{
	Super::DrawHUD();

	// ⚠️ VR(HMD)에서는 이 평면 Canvas HUD 를 그리지 않는다 — ModeSelectHUD 와 같은 이유.
	// Canvas 는 스테레오에서 눈마다 다른 위치로 찍혀 좌/우가 어긋나고, 월드 패널(UVRInfoPanel)과
	// 겹쳐 어지럽다. 헤드셋 안 UI 는 폰의 VrPanel 이 전담한다 (이 HUD 는 PC 시연/검증 전용).
	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		return;
	}

	ABackupPawn* Pawn = Cast<ABackupPawn>(GetOwningPawn());
	if (!Pawn || !Canvas)
	{
		return;
	}

	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	const float S = FMath::Clamp(W / 1920.0f, 0.7f, 1.4f);

	// ── 상단 패널: 진행·포지션·상황 ──
	const float PanelW = 940.0f * S;
	const float PanelH = 134.0f * S;
	const float PanelX = (W - PanelW) * 0.5f;
	const float PanelY = 28.0f * S;
	DrawPanel(PanelX, PanelY, PanelW, PanelH, PanelBg, PanelLine);

	const FString Progress = FString::Printf(TEXT("%s   Q %d / %d      Correct %d"),
		*UBackupPlaybook::PositionName(Pawn->GetPosition()), Pawn->GetTrialNumber(), Pawn->GetTotalTrials(), Pawn->GetSuccessCount());
	DrawLabel(Progress, PanelX + 20.0f * S, PanelY + 12.0f * S, TextDim, 0.85f * S);

	DrawLabel(FString::Printf(TEXT("ball speed x%.1f  ([ / ])"), Pawn->GetBallSpeedScale()),
		PanelX + 20.0f * S, PanelY + 66.0f * S, TextDim, 0.75f * S);

	{
		const float Live = Pawn->GetLiveDecisionSec();
		const float Shown = (Live >= 0.0f) ? Live : Pawn->GetLastDecisionSec();
		if (Shown >= 0.0f)
		{
			DrawLabel(FString::Printf(TEXT("decide  %.1fs"), Shown),
				PanelX + PanelW - 150.0f * S, PanelY + 12.0f * S,
				(Shown > 1.5f) ? FLinearColor(0.95f, 0.55f, 0.30f, 1.0f) : TextDim, 0.85f * S);
		}
	}

	DrawCentered(Pawn->GetSituationText(), W * 0.5f, PanelY + 38.0f * S, TextMain, 1.1f * S);
	DrawCentered(FString::Printf(TEXT("Runners: %s"), *Pawn->GetRunnersText()),
		W * 0.5f, PanelY + 66.0f * S, TextDim, 0.9f * S);
	DrawCentered(Pawn->IsHoldTrial() ? TEXT("Judgment: is there a backup job here at all?") : TEXT("Judgment: where do you back up?"),
		W * 0.5f, PanelY + 92.0f * S, Good, 0.85f * S);

	// ── 탑다운 미니맵 — PC 검증의 핵심 도구 ──
	const float MapRadius = FMath::Min(W, H) * 0.28f;
	const float MapCenterX = W * 0.5f;
	const float MapCenterY = PanelY + PanelH + MapRadius + 30.0f * S;
	DrawMinimap(Pawn, MapCenterX, MapCenterY, MapRadius);

	// ── 결과 + 해설 ──
	FString Outcome; FLinearColor OColor;
	if (Pawn->GetLastOutcomeText(Outcome, OColor))
	{
		const float ResultY = MapCenterY + MapRadius + 24.0f * S;
		DrawCentered(Outcome, W * 0.5f, ResultY, OColor, 1.4f * S);

		// 해설은 규칙 테이블의 한 줄일 수도, AI 가 확장한 1~2문장일 수도 있다 —
		// 한 줄로 그리면 후자가 화면 밖으로 잘린다. 글자수로 하드 랩한다.
		{
			const FString Explain = Pawn->GetLastExplainText();
			constexpr int32 ExplainChars = 88;
			float EY = ResultY + 36.0f * S;
			for (int32 i = 0; i < Explain.Len(); i += ExplainChars)
			{
				DrawCentered(Explain.Mid(i, ExplainChars), W * 0.5f, EY, TextDim, 0.8f * S);
				EY += 22.0f * S;
			}
		}
	}

	// ── 세션 종료 시 AI 판단 코칭 ──
	const FString& Coaching = Pawn->GetCoachingText();
	if (!Coaching.IsEmpty())
	{
		float PY = MapCenterY + MapRadius + 90.0f * S;
		DrawCentered(TEXT("AI judgment tips"), W * 0.5f, PY, FLinearColor(0.6f, 0.82f, 1.0f, 1.0f), 1.0f * S);
		PY += 30.0f * S;

		// 처방 목록이 들어갈 자리를 먼저 떼어두고, 코칭 문장은 남는 만큼만 그린다.
		// 코칭이 길어졌을 때(운동 효과 + 수행량을 문장에 넣으면서) 목록이 화면 밖으로
		// 밀려나면 정작 "몇 세트 몇 회"가 안 보인다 — 잘려야 할 쪽은 문장이다.
		const int32 DrillCount = Pawn->GetRecommendedDrills().Num();
		const float CoachMaxY = H - (DrillCount * 22.0f * S) - 56.0f * S;

		constexpr int32 MaxChars = 70;
		int32 i = 0;
		while (i < Coaching.Len() && PY <= CoachMaxY)
		{
			DrawCentered(Coaching.Mid(i, MaxChars), W * 0.5f, PY, TextMain, 0.8f * S);
			PY += 24.0f * S;
			i += MaxChars;
		}
		// 이름 옆 수행량은 판단 드릴이라 "케이스 수"다 (2 sets x 10 cases) — 근력 세트가 아니다.
		for (const FTrainingDrill& D : Pawn->GetRecommendedDrills())
		{
			const FString Head = D.Prescription.IsEmpty()
				? FString::Printf(TEXT("- %s"), *D.Name)
				: FString::Printf(TEXT("- %s (%s)"), *D.Name, *D.Prescription);
			DrawCentered(FString::Printf(TEXT("%s : %s"), *Head, *D.FocusCue),
				W * 0.5f, PY, FLinearColor(1.0f, 0.78f, 0.47f, 1.0f), 0.75f * S);
			PY += 22.0f * S;
		}
	}

	// ── 조작 안내 ──
	DrawCentered(TEXT("Hold WASD to move to your backup zone    -    Q / E (or arrows) to look around    -    M to exit"),
		W * 0.5f, H - 30.0f * S, TextDim, 0.75f * S);
}
