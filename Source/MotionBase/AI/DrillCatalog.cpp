#include "AI/DrillCatalog.h"

namespace
{
	FTrainingDrill MakeDrill(EWeaknessAxis Axis, const TCHAR* Name, const TCHAR* Desc, const TCHAR* Cue)
	{
		FTrainingDrill D;
		D.TargetAxis = Axis;
		D.Name = Name;
		D.Description = Desc;
		D.FocusCue = Cue;
		return D;
	}
}

TArray<FTrainingDrill> UDrillCatalog::DrillsForAxis(EWeaknessAxis Axis)
{
	switch (Axis)
	{
	case EWeaknessAxis::ContactRate:
		return {
			MakeDrill(EWeaknessAxis::ContactRate, TEXT("트래킹 드릴"),
				TEXT("공을 릴리스부터 임팩트까지 눈으로 끝까지 좇으며 10구 지켜보기(스윙 없이)."),
				TEXT("공을 오래 본다")),
			MakeDrill(EWeaknessAxis::ContactRate, TEXT("소프트 토스 컨택"),
				TEXT("가까운 거리에서 느리게 토스된 공을 맞히는 데만 집중, 20구."),
				TEXT("맞히기 우선")),
		};

	case EWeaknessAxis::Timing:
		return {
			MakeDrill(EWeaknessAxis::Timing, TEXT("리듬 스텝 드릴"),
				TEXT("투수 릴리스에 맞춰 앞발 스텝 타이밍을 일정하게 반복, 15스윙."),
				TEXT("릴리스=스텝")),
			MakeDrill(EWeaknessAxis::Timing, TEXT("구속 변화 소프트토스"),
				TEXT("느린 공과 빠른 공을 섞어 토스, 타이밍을 공에 맞춰 조절."),
				TEXT("공에 맞춰 기다린다")),
		};

	case EWeaknessAxis::ContactAccuracy:
		return {
			MakeDrill(EWeaknessAxis::ContactAccuracy, TEXT("티 배팅 정밀 컨택"),
				TEXT("고정 티의 공 중심을 배트 스위트스팟으로 정확히 맞히기, 20스윙."),
				TEXT("배트 중심")),
			MakeDrill(EWeaknessAxis::ContactAccuracy, TEXT("존 구분 소프트토스"),
				TEXT("높낮이·좌우 코스를 나눠 토스, 각 코스에서 정확히 컨택."),
				TEXT("코스별 조준")),
		};

	case EWeaknessAxis::BatSpeed:
		return {
			MakeDrill(EWeaknessAxis::BatSpeed, TEXT("회전 파워 스로우"),
				TEXT("메디신볼을 타격 방향으로 힘껏 던져 하체·코어 회전력 강화, 10회 3세트."),
				TEXT("하체부터 회전")),
			MakeDrill(EWeaknessAxis::BatSpeed, TEXT("저항밴드 스윙"),
				TEXT("밴드 저항을 걸고 스윙 궤도를 유지하며 가속, 12스윙 3세트."),
				TEXT("임팩트에서 가속")),
		};

	case EWeaknessAxis::Consistency:
		return {
			MakeDrill(EWeaknessAxis::Consistency, TEXT("루틴 고정 반복"),
				TEXT("셋업→스텝→스윙 루틴을 동일하게 10스윙 반복, 매 스윙 감각 체크."),
				TEXT("같은 동작 반복")),
			MakeDrill(EWeaknessAxis::Consistency, TEXT("체크포인트 스윙"),
				TEXT("느린 스윙으로 자세 체크포인트(그립·팔꿈치·회전)를 매번 확인."),
				TEXT("자세 점검")),
		};

	case EWeaknessAxis::HipShoulderSeparation:
		return {
			MakeDrill(EWeaknessAxis::HipShoulderSeparation, TEXT("힙 리드 분리 드릴"),
				TEXT("상체는 뒤에 남긴 채 골반을 먼저 여는 느낌으로 천천히 10스윙, 비틀림 축적."),
				TEXT("골반 먼저, 어깨는 뒤에")),
			MakeDrill(EWeaknessAxis::HipShoulderSeparation, TEXT("밴드 코일 홀드"),
				TEXT("밴드로 상체를 고정하고 하체만 회전해 상하체 분리 감각을 익힌다, 8회 3세트."),
				TEXT("상하체 따로")),
		};

	case EWeaknessAxis::HeadStability:
		return {
			MakeDrill(EWeaknessAxis::HeadStability, TEXT("시선 고정 티 배팅"),
				TEXT("티 위 공의 한 점을 임팩트까지 응시하며 머리를 고정, 15스윙."),
				TEXT("공에서 눈 떼지 않기")),
			MakeDrill(EWeaknessAxis::HeadStability, TEXT("헤드 스틸 미러 드릴"),
				TEXT("거울/영상 앞에서 스윙하며 머리 높이·좌우가 흔들리지 않는지 확인, 10스윙."),
				TEXT("머리 높이 유지")),
		};

	case EWeaknessAxis::KineticChain:
		return {
			MakeDrill(EWeaknessAxis::KineticChain, TEXT("스텝-히프-핸드 순서 드릴"),
				TEXT("앞발 착지→골반 회전→손 순서를 과장해 느리게 반복, 순서 체득 12스윙."),
				TEXT("아래에서 위로 전달")),
			MakeDrill(EWeaknessAxis::KineticChain, TEXT("메디신볼 로테이션 스로우"),
				TEXT("하체부터 감아 던지며 힙→몸통→팔로 힘이 전달되는 순서를 몸에 새긴다, 10회 3세트."),
				TEXT("하체부터 시작")),
		};

	case EWeaknessAxis::WeightShift:
		return {
			MakeDrill(EWeaknessAxis::WeightShift, TEXT("뒷발→앞발 로드 드릴"),
				TEXT("체중을 뒷발에 실었다가 스윙과 함께 앞발로 옮기는 동작을 분리 반복, 12스윙."),
				TEXT("뒤에서 앞으로")),
			MakeDrill(EWeaknessAxis::WeightShift, TEXT("스텝 스루 배팅"),
				TEXT("가벼운 스텝으로 체중을 앞으로 밀며 타격, 이동 후 균형 유지 확인, 10스윙."),
				TEXT("앞발로 눌러 딛기")),
		};

	default:
		return {};
	}
}

TArray<FTrainingDrill> UDrillCatalog::Recommend(const FWeaknessReport& Report, int32 MaxDrills)
{
	TArray<FTrainingDrill> Out;
	MaxDrills = FMath::Max(MaxDrills, 1);

	if (!Report.bValid)
	{
		return Out;
	}

	// 약점이 없으면 유지용 기본 드릴.
	if (Report.Weaknesses.Num() == 0)
	{
		Out.Add(MakeDrill(EWeaknessAxis::Consistency, TEXT("경기 감각 유지"),
			TEXT("실전 페이스로 라이브 배팅을 이어가며 현재 밸런스를 유지."),
			TEXT("현재 감각 유지")));
		return Out;
	}

	// 시급한 약점부터(리포트가 이미 정렬됨) 드릴을 뽑되, 이름 중복은 건너뛴다.
	TSet<FString> Seen;
	for (const FWeakness& W : Report.Weaknesses)
	{
		for (const FTrainingDrill& D : DrillsForAxis(W.Axis))
		{
			if (Out.Num() >= MaxDrills)
			{
				return Out;
			}
			if (!Seen.Contains(D.Name))
			{
				Seen.Add(D.Name);
				Out.Add(D);
			}
		}
	}
	return Out;
}

TArray<FTrainingDrill> UDrillCatalog::RecommendWithHistory(
	const FWeaknessReport& Report, const FChronicWeaknessReport& Chronic, int32 MaxDrills)
{
	// 이력이 없으면 단발 추천과 동일하게 — 신규 사용자/첫 세션 경로.
	if (!Chronic.bValid)
	{
		return Recommend(Report, MaxDrills);
	}

	TArray<FTrainingDrill> Out;
	MaxDrills = FMath::Max(MaxDrills, 1);

	if (!Report.bValid)
	{
		return Out;
	}

	// 축별 만성 추세를 빠르게 찾기 위한 색인.
	TMap<EWeaknessAxis, const FAxisTrend*> TrendByAxis;
	for (const FAxisTrend& T : Chronic.Trends)
	{
		TrendByAxis.Add(T.Axis, &T);
	}

	// 이번 세션 약점을 (현재 심각도 + 만성 가중)으로 재정렬한다.
	// 만성 가중: 반복 등장 비율 * 0.5, 악화면 +0.25 더. 단발 심각도(0~1)와 같은 스케일.
	struct FRanked { EWeaknessAxis Axis; float Priority; float Severity; int32 Appearances; };
	TArray<FRanked> Ranked;
	for (const FWeakness& W : Report.Weaknesses)
	{
		FRanked R;
		R.Axis = W.Axis;
		R.Severity = W.Severity;
		R.Appearances = 0;
		float Boost = 0.0f;
		if (const FAxisTrend* const* Found = TrendByAxis.Find(W.Axis))
		{
			const FAxisTrend* T = *Found;
			R.Appearances = T->AppearanceCount;
			if (T->WindowSize > 0)
			{
				Boost += 0.5f * (static_cast<float>(T->AppearanceCount) / T->WindowSize);
			}
			if (T->Trend == EWeaknessTrend::Worsening) { Boost += 0.25f; }
		}
		R.Priority = W.Severity + Boost;
		Ranked.Add(R);
	}

	// 우선순위 내림차순 — 만성·악화 약점이 앞으로.
	Ranked.Sort([](const FRanked& A, const FRanked& B) { return A.Priority > B.Priority; });

	// 축마다 드릴을 뽑되, 반복 처방된 축은 등장 횟수만큼 로테이션해 다른 드릴을 낸다.
	// (같은 약점에 매번 같은 운동만 나오면 질린다 — 카탈로그를 돌려 쓴다.)
	TSet<FString> Seen;
	for (const FRanked& R : Ranked)
	{
		if (Out.Num() >= MaxDrills)
		{
			break;
		}
		const TArray<FTrainingDrill> Drills = DrillsForAxis(R.Axis);
		if (Drills.Num() == 0)
		{
			continue;
		}
		const int32 Start = R.Appearances % Drills.Num();
		// 로테이션 시작점부터 한 바퀴 돌며 아직 안 뽑힌 첫 드릴을 고른다.
		for (int32 k = 0; k < Drills.Num(); ++k)
		{
			const FTrainingDrill& D = Drills[(Start + k) % Drills.Num()];
			if (!Seen.Contains(D.Name))
			{
				Seen.Add(D.Name);
				Out.Add(D);
				break;
			}
		}
	}

	return Out;
}
