#include "AI/DrillCatalog.h"

namespace
{
	// Desc = 수행 방법, Benefit = 무엇이 좋아지는지, Prescription = 수행량.
	// ⚠️ 수행량은 Desc 에 섞어 쓰지 않는다 — 두 곳에 적히면 어긋나고, LLM 이 어느 쪽을
	//    인용할지 흔들린다. 횟수·세트는 Prescription 한 곳에만 존재한다.
	FTrainingDrill MakeDrill(EWeaknessAxis Axis, const TCHAR* Name, const TCHAR* Desc, const TCHAR* Cue,
		const TCHAR* Benefit, const TCHAR* Prescription, const TCHAR* PrescriptionShort)
	{
		FTrainingDrill D;
		D.TargetAxis = Axis;
		D.Name = Name;
		D.Description = Desc;
		D.FocusCue = Cue;
		D.Benefit = Benefit;
		D.Prescription = Prescription;
		D.PrescriptionShort = PrescriptionShort;
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
				TEXT("투구가 릴리스되는 순간부터 임팩트까지 스윙 없이 눈으로만 공을 따라간다."),
				TEXT("공을 더 오래 본다"),
				TEXT("눈으로 추적하는 능력과 구질 판별력을 길러, 실제로 본 공에 배트 중심이 맞게 한다"),
				TEXT("3세트 x 10구"), TEXT("3x10")),
			MakeDrill(EWeaknessAxis::ContactRate, TEXT("소프트토스 컨택"),
				TEXT("느리고 가까운 토스에 컨택하는 것에만 집중한다."),
				TEXT("컨택 먼저"),
				TEXT("손과 눈의 협응력과 반복 가능한 컨택 포인트를 만든다"),
				TEXT("3세트 x 20구"), TEXT("3x20")),
		};

	case EWeaknessAxis::Timing:
		return {
			MakeDrill(EWeaknessAxis::Timing, TEXT("리듬 스텝 드릴"),
				TEXT("투수의 릴리스 타이밍에 맞춰 앞발 스텝을 일정하게 반복한다."),
				TEXT("릴리스 = 스텝"),
				TEXT("체중 이동과 스트라이드를 릴리스에 맞춰, 스윙이 제때 시작되게 한다"),
				TEXT("3세트 x 15스윙"), TEXT("3x15")),
			MakeDrill(EWeaknessAxis::Timing, TEXT("가변속 소프트토스"),
				TEXT("느린 토스와 빠른 토스를 섞어, 공에 맞춰 타이밍을 조절한다."),
				TEXT("공을 기다린다"),
				TEXT("구속 변화에 대한 타이밍 조절 능력을 향상시킨다"),
				TEXT("3세트 x 15토스"), TEXT("3x15")),
		};

	case EWeaknessAxis::ContactAccuracy:
		return {
			MakeDrill(EWeaknessAxis::ContactAccuracy, TEXT("티 정밀 컨택"),
				TEXT("고정된 티볼의 중심을 배트 스위트스팟으로 맞춘다."),
				TEXT("배트 중심"),
				TEXT("배트 컨트롤과 스위트스팟 정확도를 예리하게 다듬는다"),
				TEXT("3세트 x 20스윙"), TEXT("3x20")),
			MakeDrill(EWeaknessAxis::ContactAccuracy, TEXT("존별 소프트토스"),
				TEXT("높낮이·인아웃 존을 나누어 토스하고, 각 존에서 깨끗하게 컨택한다."),
				TEXT("존마다 조준"),
				TEXT("편한 존뿐 아니라 모든 존에서 배트 정확도를 넓힌다"),
				TEXT("4개 존 x 10스윙"), TEXT("4x10")),
		};

	case EWeaknessAxis::BatSpeed:
		return {
			MakeDrill(EWeaknessAxis::BatSpeed, TEXT("데드리프트"),
				TEXT("바벨이나 케틀벨로 힙 힌지 동작을 하며 등을 평평하게 유지하고 바닥을 밀어낸다 - "
					"무게를 늘리기 전에 가벼운 무게로 자세부터 잡는다."),
				TEXT("허리가 아닌 엉덩이로 접는다"),
				TEXT("모든 회전 스윙이 딛고 서는 기반인 고관절 신전력과 코어 안정성을 기른다"),
				TEXT("3세트 x 15회"), TEXT("3x15")),
			MakeDrill(EWeaknessAxis::BatSpeed, TEXT("회전 파워 스로우"),
				TEXT("하체와 코어를 사용해 타격 방향으로 메디신볼을 강하게 던진다."),
				TEXT("다리부터 회전"),
				TEXT("배트 스피드로 전환되는 하체·코어 회전력을 발달시킨다"),
				TEXT("3세트 x 10회"), TEXT("3x10")),
			MakeDrill(EWeaknessAxis::BatSpeed, TEXT("저항밴드 스윙"),
				TEXT("밴드 저항에 맞서 스윙 궤도를 유지하며 끝까지 가속한다."),
				TEXT("임팩트에서 가속"),
				TEXT("부하 상태에서 컨택 구간을 통과하는 가속력을 훈련한다"),
				TEXT("3세트 x 12스윙"), TEXT("3x12")),
		};

	case EWeaknessAxis::Consistency:
		return {
			MakeDrill(EWeaknessAxis::Consistency, TEXT("고정 루틴 반복"),
				TEXT("같은 셋업-스텝-스윙 루틴을 반복하며 매번 느낌을 확인한다."),
				TEXT("같은 동작 반복"),
				TEXT("셋업-스텝-스윙 동작을 루틴화해 스윙마다의 편차를 줄인다"),
				TEXT("3세트 x 10스윙"), TEXT("3x10")),
			MakeDrill(EWeaknessAxis::Consistency, TEXT("체크포인트 스윙"),
				TEXT("그립·팔꿈치·회전 등 자세 체크포인트를 매번 확인하며 천천히 스윙한다."),
				TEXT("자세 확인"),
				TEXT("자세 체크포인트를 고정시켜 압박 상황에서도 같은 스윙이 반복되게 한다"),
				TEXT("3세트 x 10 슬로우 스윙"), TEXT("3x10")),
		};

	case EWeaknessAxis::HipShoulderSeparation:
		return {
			MakeDrill(EWeaknessAxis::HipShoulderSeparation, TEXT("힙 리드 분리 드릴"),
				TEXT("상체는 뒤에 남기고 엉덩이를 먼저 열어, 천천히 스윙하며 코일을 저장한다."),
				TEXT("엉덩이 먼저, 어깨는 뒤에"),
				TEXT("힙-숄더 분리를 늘려 몸통이 더 많은 탄성 에너지를 저장하고 방출하게 한다"),
				TEXT("3세트 x 10 슬로우 스윙"), TEXT("3x10")),
			MakeDrill(EWeaknessAxis::HipShoulderSeparation, TEXT("밴드 코일 홀드"),
				TEXT("밴드로 상체를 고정하고 하체만 회전시켜 분리 감각을 익힌다."),
				TEXT("상체와 하체를 분리"),
				TEXT("몸이 한 덩어리로 도는 대신 분리를 유지할 몸통 힘을 기른다"),
				TEXT("3세트 x 8회"), TEXT("3x8")),
		};

	case EWeaknessAxis::HeadStability:
		return {
			MakeDrill(EWeaknessAxis::HeadStability, TEXT("시선 고정 티배팅"),
				TEXT("임팩트까지 티볼의 한 점을 응시하며 머리를 고정한다."),
				TEXT("공에서 시선을 떼지 않는다"),
				TEXT("머리와 시선을 고정시켜 컨택까지 공에 초점이 유지되게 한다"),
				TEXT("3세트 x 15스윙"), TEXT("3x15")),
			MakeDrill(EWeaknessAxis::HeadStability, TEXT("헤드 고정 거울 드릴"),
				TEXT("거울이나 영상 앞에서 스윙하며 머리가 위아래·좌우로 흔들리지 않는지 확인한다."),
				TEXT("머리 높이 유지"),
				TEXT("스윙마다 컨택 포인트를 흔드는 머리 움직임을 없앤다"),
				TEXT("3세트 x 10스윙"), TEXT("3x10")),
		};

	case EWeaknessAxis::KineticChain:
		return {
			MakeDrill(EWeaknessAxis::KineticChain, TEXT("스텝-힙-핸드 순서 드릴"),
				TEXT("앞발 착지 -> 힙 회전 -> 손 순서를 과장해서 천천히 익힌다."),
				TEXT("아래에서 위로 전달"),
				TEXT("운동사슬(착지-힙-손) 순서를 정렬해 힘이 새지 않고 아래에서 위로 전달되게 한다"),
				TEXT("3세트 x 12스윙"), TEXT("3x12")),
			MakeDrill(EWeaknessAxis::KineticChain, TEXT("메디신볼 회전 스로우"),
				TEXT("다리부터 힘을 모아 던지며 힙 -> 몸통 -> 팔 순서를 몸에 익힌다."),
				TEXT("다리부터 시작"),
				TEXT("부하 상태에서 힙-몸통-팔 발동 순서를 각인시킨다"),
				TEXT("3세트 x 10회"), TEXT("3x10")),
		};

	case EWeaknessAxis::WeightShift:
		return {
			MakeDrill(EWeaknessAxis::WeightShift, TEXT("백투프론트 체중이동 드릴"),
				TEXT("뒷발에 체중을 실었다가 스윙과 함께 앞발로 옮기는 동작을 분리해서 반복한다."),
				TEXT("뒤에서 앞으로"),
				TEXT("완전한 뒤-앞 체중 이동을 훈련해, 팔 힘을 더 쓰지 않고도 추진력을 더한다"),
				TEXT("3세트 x 12스윙"), TEXT("3x12")),
			MakeDrill(EWeaknessAxis::WeightShift, TEXT("스텝스루 배팅"),
				TEXT("타격하며 가벼운 스텝으로 체중을 앞으로 밀고, 이동 후에도 균형을 유지한다."),
				TEXT("앞발을 딛는다"),
				TEXT("체중이 앞으로 이동하는 중에도 균형을 유지하는 법을 익힌다"),
				TEXT("3세트 x 10스윙"), TEXT("3x10")),
		};

	// ── 수비(포구) 체력 드릴 ──
	case EWeaknessAxis::CatchReaction:
		return {
			MakeDrill(EWeaknessAxis::CatchReaction, TEXT("반응 포구 드릴"),
				TEXT("예고 없이 던진 공이나 벽에 튕겨 나온 공을 즉시 포구한다."),
				TEXT("생각보다 손이 먼저"),
				TEXT("공을 본 순간부터 글러브가 도착하기까지의 반응 시간을 줄인다"),
				TEXT("3세트 x 20회"), TEXT("3x20")),
			MakeDrill(EWeaknessAxis::CatchReaction, TEXT("라이트 반응 터치"),
				TEXT("무작위로 켜지는 신호(불빛/파트너 손)를 손을 뻗어 터치한다."),
				TEXT("신호에 반응"),
				TEXT("포구 기술과 분리해 신호-첫 동작 사이의 지연 자체를 훈련한다"),
				TEXT("3세트 x 30초"), TEXT("3x30초")),
		};

	case EWeaknessAxis::UpperBodyFlex:
		return {
			MakeDrill(EWeaknessAxis::UpperBodyFlex, TEXT("흉추-어깨 회전 스트레칭"),
				TEXT("상체를 좌우로 크게 비틀며 끝 지점에서 멈춘다."),
				TEXT("닿는 범위를 넓힌다"),
				TEXT("흉추와 어깨 회전 가동범위를 열어, 포구 가능한 범위를 넓힌다"),
				TEXT("2세트 x 좌우 5회 (10초 유지)"), TEXT("2x5/방향")),
			MakeDrill(EWeaknessAxis::UpperBodyFlex, TEXT("밴드 오버헤드 리치"),
				TEXT("밴드를 잡고 팔로 머리 위로 큰 원을 그린다."),
				TEXT("범위를 넓힌다"),
				TEXT("머리 위 공을 처리하기 위한 오버헤드 어깨 가동범위를 회복시킨다"),
				TEXT("2세트 x 12회"), TEXT("2x12")),
		};

	case EWeaknessAxis::FootSpeed:
		return {
			MakeDrill(EWeaknessAxis::FootSpeed, TEXT("라더 퀵스텝"),
				TEXT("라더나 라인을 밟으며 발 회전 속도를 높인다."),
				TEXT("짧고 빠른 스텝"),
				TEXT("발 회전 속도를 높여 공을 향한 첫 걸음이 더 빨라지게 한다"),
				TEXT("3세트 x 30초"), TEXT("3x30초")),
			MakeDrill(EWeaknessAxis::FootSpeed, TEXT("사이드 셔플"),
				TEXT("낮은 자세로 좌우로 빠르게 셔플한다."),
				TEXT("낮고 빠르게"),
				TEXT("공이 도착하기 전에 수비 위치를 잡는 좌우 민첩성을 기른다"),
				TEXT("4세트 x 10m"), TEXT("4x10m")),
		};

	// ── 수비(송구) 드릴 ──
	case EWeaknessAxis::ThrowAccuracy:
		return {
			MakeDrill(EWeaknessAxis::ThrowAccuracy, TEXT("타겟 라인 스로우"),
				TEXT("20m 거리에서 가슴 높이 타겟으로 던지며 매번 타겟을 향해 똑바로 스텝한다."),
				TEXT("앞발이 베이스를 향한다"),
				TEXT("스트라이드와 릴리스 라인을 타겟에 맞춰 정렬해 송구 정확도를 높인다"),
				TEXT("3세트 x 20회"), TEXT("3x20")),
			MakeDrill(EWeaknessAxis::ThrowAccuracy, TEXT("베이스 원바운드 송구"),
				TEXT("먼 거리에서 의도적으로 원바운드를 만들어 받는 사람 글러브로 보낸다."),
				TEXT("낮게 빗나가야지, 높으면 안 된다"),
				TEXT("낮게 빗나가는 습관을 훈련한다 - 원바운드는 잡을 수 있지만 높은 송구는 진루를 내준다"),
				TEXT("3세트 x 15회"), TEXT("3x15")),
		};

	case EWeaknessAxis::ArmStrength:
		return {
			MakeDrill(EWeaknessAxis::ArmStrength, TEXT("롱토스 래더"),
				TEXT("15m -> 25m -> 35m로 거리를 늘렸다가 다시 줄인다."),
				TEXT("조준 말고 실어 보낸다"),
				TEXT("점진적인 거리 증가를 통해 송구 거리와 어깨 지구력을 기른다"),
				TEXT("6단계 x 5회"), TEXT("6x5")),
			MakeDrill(EWeaknessAxis::ArmStrength, TEXT("메디신볼 크로우홉 스로우"),
				TEXT("크로우홉 스텝으로 2kg 메디신볼을 온몸으로 던진다."),
				TEXT("팔이 아닌 다리와 몸통으로"),
				TEXT("팔에 무리를 주지 않고 다리와 몸통에서 나오는 전신 송구 파워를 더한다"),
				TEXT("3세트 x 8회"), TEXT("3x8")),
		};

	case EWeaknessAxis::TransferQuick:
		return {
			MakeDrill(EWeaknessAxis::TransferQuick, TEXT("글러브-핸드 트랜스퍼 반복"),
				TEXT("포구 후 가슴 앞에서 공을 던지는 손으로 옮긴다 - 던지지 않고 트랜스퍼만."),
				TEXT("귀가 아니라 가슴으로 가져온다"),
				TEXT("송구가 시작되기도 전인 글러브-손 트랜스퍼 시간을 줄인다"),
				TEXT("3세트 x 30회"), TEXT("3x30")),
			MakeDrill(EWeaknessAxis::TransferQuick, TEXT("퀵릴리스 풋워크"),
				TEXT("짧은 거리에서 포구-좌우 스텝-릴리스를 하나의 동작으로 이어간다."),
				TEXT("포구와 동시에 발이 움직인다"),
				TEXT("포구·스텝·릴리스를 하나로 합쳐 더 빠른 릴리스를 만든다"),
				TEXT("3세트 x 20회"), TEXT("3x20")),
		};

	// ── 수비(백업 판단) 드릴 ──
	// ⚠️ 판단 훈련이다. 처방의 단위는 세트·횟수가 아니라 **상황 케이스 수**다 —
	//    여기에 근력·컨디셔닝 처방이 섞이면 Backup 도메인 프롬프트의 금지 규칙과 충돌한다.
	case EWeaknessAxis::BackupJudgment:
		return {
			MakeDrill(EWeaknessAxis::BackupJudgment, TEXT("포지션 백업 워크스루"),
				TEXT("자신의 포지션에서 타구 방향별 백업 경로를 걸어가며 익힌다."),
				TEXT("타구 방향이 베이스를 결정한다"),
				TEXT("모든 타구 방향과 백업 베이스를 매칭해 자동으로 나오게 만든다"),
				TEXT("2세트 x 10케이스"), TEXT("2x10")),
			MakeDrill(EWeaknessAxis::BackupJudgment, TEXT("주자 상황 카드 리뷰"),
				TEXT("주자 상황(없음/1루/2루)을 정해두고 각각 자신의 백업 베이스를 소리내어 말한다."),
				TEXT("주자가 송구를 결정한다"),
				TEXT("주자 상황과 자신의 임무를 결정하는 송구 목적지를 연결시킨다"),
				TEXT("3세트 x 15케이스"), TEXT("3x15")),
		};

	case EWeaknessAxis::DecisionSpeed:
		return {
			MakeDrill(EWeaknessAxis::DecisionSpeed, TEXT("콜아웃 리액션"),
				TEXT("파트너가 상황을 부르면 2초 안에 백업 베이스를 말한다."),
				TEXT("움직이기 전에 결정한다"),
				TEXT("발이 움직이기 전, 상황 인지와 판단 사이의 간격을 줄인다"),
				TEXT("2세트 x 20회"), TEXT("2x20")),
			MakeDrill(EWeaknessAxis::DecisionSpeed, TEXT("투구 전 루틴"),
				TEXT("매 투구 전에 타구 방향별 자신의 임무를 말한다."),
				TEXT("투구 전에 미리 결정한다"),
				TEXT("판단 시점을 투구 전으로 옮겨, 생각하지 않고 반응하게 한다"),
				TEXT("1이닝 전체, 매 투구"), TEXT("1이닝")),
		};

	// ── 수비(백업 동선 효율) 드릴 — 판단 훈련. 컨디셔닝 처방 아님. ──
	case EWeaknessAxis::RouteEfficiency:
		return {
			MakeDrill(EWeaknessAxis::RouteEfficiency, TEXT("직선 경로 워크스루"),
				TEXT("절반 속도로 정확한 백업 경로를 걸으며 경로를 벗어나는 지점을 확인한다."),
				TEXT("지점을 정하고 직선으로 간다"),
				TEXT("경로 이탈을 없애 목표 지점까지 최단 경로로 가게 한다"),
				TEXT("2세트 x 8경로"), TEXT("2x8")),
			MakeDrill(EWeaknessAxis::RouteEfficiency, TEXT("콜앤커밋"),
				TEXT("첫 걸음을 떼기 전에 백업 베이스를 소리내어 말하고, 경로 중간에 마음을 바꾸지 않는다."),
				TEXT("한번 정하면 끝까지"),
				TEXT("첫 걸음 전에 콜을 강제해 경로 중간의 방향 전환을 막는다"),
				TEXT("3세트 x 15회"), TEXT("3x15")),
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
	// ⚠️ 모드에 맞는 것을 줘야 한다 — 수비 세션 끝에 "라이브 배팅을 계속하세요"가 나오면
	//    추천 전체의 신뢰가 무너진다 (약점 0개는 잘한 세션이라 오히려 자주 나온다).
	if (Report.Weaknesses.Num() == 0)
	{
		if (Report.Mode == EGameModeId::Defense)
		{
			Out.Add(MakeDrill(EWeaknessAxis::CatchReaction, TEXT("수비 루틴 유지"),
				TEXT("특별히 부족한 부분이 없다 - 실전 속도의 반복 훈련을 계속하며 이 감각을 유지한다."),
				TEXT("지금 감각 유지"),
				TEXT("이미 갖춘 수비 타이밍을 다시 만들 필요 없이 그대로 유지한다"),
				TEXT("2세트 x 10회 (실전 속도)"), TEXT("2x10")));
		}
		else
		{
			Out.Add(MakeDrill(EWeaknessAxis::Consistency, TEXT("경기 감각 유지"),
				TEXT("현재의 밸런스를 유지하기 위해 실전 속도의 라이브 배팅을 계속한다."),
				TEXT("지금 감각 유지"),
				TEXT("이미 갖춘 스윙 밸런스와 타이밍을 유지한다"),
				TEXT("3세트 x 10 라이브 스윙"), TEXT("3x10")));
		}
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
