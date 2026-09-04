#include "Core/ModeManager.h"
#include "MotionBase.h"
#include "Save/MotionBaseSaveGame.h"
#include "Kismet/GameplayStatics.h"

void UModeManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 슬롯이 있으면 로드, 없으면 새 저장 오브젝트를 만든다.
	const FString Slot = UMotionBaseSaveGame::DefaultSlotName;
	const uint32 User = UMotionBaseSaveGame::DefaultUserIndex;

	if (UGameplayStatics::DoesSaveGameExist(Slot, User))
	{
		SaveData = Cast<UMotionBaseSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, User));
	}

	if (!SaveData)
	{
		SaveData = Cast<UMotionBaseSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UMotionBaseSaveGame::StaticClass()));
	}

	UE_LOG(LogMotionBase, Log, TEXT("ModeManager: 저장 로드 — 누적 세션 %d건"),
		SaveData ? SaveData->History.Num() : 0);
}

void UModeManager::Deinitialize()
{
	// 앱 종료·레벨 정리 시점에 폰이 flush 하지 못한 세션이 남아 있을 수 있다.
	// FinalizeSession 이 빈 세션은 스스로 걸러내므로 무조건 한 번 호출해도 안전하다.
	// (여기선 집계 평균·리포트를 다시 구하지 않고, 시도들만 확정 저장한다.)
	FinalizeSession(FScoreResult(), FWeaknessReport());

	Super::Deinitialize();
}

void UModeManager::SetActiveMode(EGameModeId NewMode, EDifficultyLevel NewDifficulty, EBattingStance NewStance)
{
	const bool bChanged = (NewMode != ActiveMode);
	ActiveMode = NewMode;
	ActiveDifficulty = NewDifficulty;
	ActiveStance = NewStance;

	// 모드 진입 = 새 세션. 같은 모드를 다시 고른 경우에도 반드시 비운다.
	SessionResults.Reset();
	SessionStartedAt = FDateTime::Now();
	ActiveDrill = NAME_None; // 세부 종목은 진입 직후 SetActiveDrill 로 다시 지정된다.
	ActiveFieldPosition = EFieldPosition::First; // 포지션도 마찬가지 — SetActiveFieldPosition 이 다시 지정.

	UE_LOG(LogMotionBase, Log, TEXT("ModeManager: 모드 진입 → %s / 난이도 %s / 타석 %s (세션 초기화)"),
		*GetModeDisplayName(NewMode).ToString(), *GetDifficultyDisplayName(NewDifficulty).ToString(),
		*GetStanceDisplayName(NewStance).ToString());

	if (bChanged)
	{
		OnModeChanged.Broadcast(NewMode);
	}
}

void UModeManager::ClearSessionResults()
{
	SessionResults.Reset();
}

void UModeManager::SetActiveDrill(FName DrillId)
{
	ActiveDrill = DrillId;
	UE_LOG(LogMotionBase, Log, TEXT("ModeManager: 세부 종목 → %s"), *DrillId.ToString());
}

void UModeManager::SetActiveFieldPosition(EFieldPosition InPosition)
{
	ActiveFieldPosition = InPosition;
	UE_LOG(LogMotionBase, Log, TEXT("ModeManager: 수비 포지션 → %d"), static_cast<int32>(InPosition));
}

FName UModeManager::GetDefenseDrillIdName(int32 DrillIndex)
{
	// ⚠️ 저장에 남는 값이라 바꾸면 과거 기록과 매칭이 끊긴다 (표시 이름과 분리한 이유).
	// index 2 는 "Backup" 이 아니라 "BackupMove" 다 — 4지선다 퀴즈 시절의 "Backup" 기록과
	// 실제 이동 훈련의 기록이 섞이면 판단 시간 만성 추세가 가짜로 뒤집힌다
	// (퀴즈=텍스트 읽는 3초, 이동=개시 0.4~0.9초 — 완전히 다른 기준선).
	switch (DrillIndex)
	{
	case 0:  return TEXT("Catch");
	case 1:  return TEXT("Throw");
	case 2:  return TEXT("BackupMove");
	default: return NAME_None;
	}
}

bool UModeManager::FinalizeSession(const FScoreResult& SessionAverage, const FWeaknessReport& Report)
{
	// 빈 세션(시도 0)은 저장하지 않는다 — 모드에 들어갔다 바로 나온 경우까지
	// 기록으로 남으면 통계·최고점이 오염된다.
	if (SessionResults.Num() == 0)
	{
		return false;
	}

	if (!SaveData)
	{
		UE_LOG(LogMotionBase, Warning, TEXT("ModeManager: SaveData 없음 — 세션을 저장하지 못했습니다."));
		SessionResults.Reset();
		return false;
	}

	FSessionResult Session;
	Session.Mode = ActiveMode;
	Session.DrillId = ActiveDrill;   // 수비 세부 종목 — 추세 분석이 종목별로 갈라지는 근거.
	Session.StartedAt = SessionStartedAt;
	Session.AttemptCount = SessionResults.Num();
	Session.Attempts = SessionResults;
	Session.Average = SessionAverage;
	Session.DifficultyLevel = static_cast<int32>(ActiveDifficulty);
	Session.Report = Report; // 만성 약점·추세 계산의 원천 — 세션마다 함께 저장.

	SaveData->History.Add(MoveTemp(Session));
	PersistSaveData();

	UE_LOG(LogMotionBase, Log, TEXT("ModeManager: 세션 저장 (mode=%s%s 시도 %d 평균 %.1f) — 누적 %d건"),
		*GetModeIdName(ActiveMode).ToString(),
		ActiveDrill.IsNone() ? TEXT("") : *FString::Printf(TEXT("/%s"), *ActiveDrill.ToString()),
		SaveData->History.Last().AttemptCount,
		SessionAverage.TotalScore, SaveData->History.Num());

	// 저장했으면 반드시 비운다 — 다음 flush 에서 같은 세션이 두 번 기록되지 않게.
	SessionResults.Reset();
	return true;
}

const TArray<FSessionResult>& UModeManager::GetHistory() const
{
	static const TArray<FSessionResult> Empty;
	return SaveData ? SaveData->History : Empty;
}

float UModeManager::GetBestTotalScore(EGameModeId Mode) const
{
	if (!SaveData)
	{
		return -1.0f;
	}

	float Best = -1.0f;
	for (const FSessionResult& S : SaveData->History)
	{
		if (S.Mode == Mode)
		{
			Best = FMath::Max(Best, S.Average.TotalScore);
		}
	}
	return Best;
}

FModeStats UModeManager::GetModeStats(EGameModeId Mode, int32 RecentCount) const
{
	FModeStats Stats;
	if (!SaveData)
	{
		return Stats;
	}

	double Sum = 0.0;
	for (const FSessionResult& S : SaveData->History)
	{
		if (S.Mode != Mode)
		{
			continue;
		}
		const float T = S.Average.TotalScore;
		Stats.BestTotal = FMath::Max(Stats.BestTotal, T);
		Stats.LastTotal = T; // append 순서 = 시간순 → 마지막 매치가 가장 최근
		Sum += T;
		++Stats.SessionCount;
	}

	if (Stats.SessionCount > 0)
	{
		Stats.AverageTotal = static_cast<float>(Sum / Stats.SessionCount);
	}

	// 최근 RecentCount 개를 뒤에서부터 모아 오래된→최신 순으로 담는다.
	RecentCount = FMath::Max(RecentCount, 0);
	for (int32 i = SaveData->History.Num() - 1; i >= 0 && Stats.RecentTotals.Num() < RecentCount; --i)
	{
		if (SaveData->History[i].Mode == Mode)
		{
			Stats.RecentTotals.Insert(SaveData->History[i].Average.TotalScore, 0);
		}
	}

	return Stats;
}

void UModeManager::PersistSaveData() const
{
	if (!SaveData)
	{
		return;
	}

	const bool bOk = UGameplayStatics::SaveGameToSlot(
		SaveData, UMotionBaseSaveGame::DefaultSlotName, UMotionBaseSaveGame::DefaultUserIndex);
	if (!bOk)
	{
		UE_LOG(LogMotionBase, Warning, TEXT("ModeManager: 저장 슬롯 기록 실패."));
	}
}

void UModeManager::RecordResult(const FScoreResult& Result)
{
	FScoreResult Stored = Result;

	// 점수 계층은 모드를 모르므로 ModeId 가 비어 온다. 여기서 채워야
	// 저장·AI 피드백 단계에서 "어느 모드 기록인지"를 잃지 않는다.
	if (Stored.ModeId.IsNone())
	{
		Stored.ModeId = GetModeIdName(ActiveMode);
	}

	// 배치의 첫 시도에서 세션 시작 시각을 찍는다. 리셋([R])으로 시작된 새 세션은
	// SetActiveMode 를 다시 거치지 않으므로, 여기서 갱신해야 저장 시각이 정확하다.
	if (SessionResults.Num() == 0)
	{
		SessionStartedAt = FDateTime::Now();
	}

	SessionResults.Add(Stored);

	UE_LOG(LogMotionBase, Log, TEXT("ModeManager: 결과 기록 (mode=%s total=%.1f) 누적 %d건"),
		*Stored.ModeId.ToString(), Stored.TotalScore, SessionResults.Num());

	// 여기서는 세션 내 메모리 누적만 한다. 슬롯 영속화는 세션 종료 시
	// FinalizeSession 에서 한 번에 일어난다 (스윙마다 디스크에 쓰지 않는다).
}


TArray<EGameModeId> UModeManager::GetMenuModes()
{
	// 현재 구현된 두 모드만 메뉴에 노출한다. 나머지 모드(신체인식·반응속도·베이스러닝·
	// 기능성피트니스)는 구현되면 여기에 다시 추가한다 (열거형·표시명·설명은 그대로 유지).
	return {
		EGameModeId::Defense,
		EGameModeId::Batting,
		EGameModeId::AICoaching   // 기록 기반 AI 운동 추천 (읽기 전용 리뷰 화면)
	};
}

FText UModeManager::GetModeDisplayName(EGameModeId Mode)
{
	switch (Mode)
	{
	case EGameModeId::BodyScan:          return FText::FromString(TEXT("신체 인식 (셋업)"));
	case EGameModeId::ReactionSpeed:     return FText::FromString(TEXT("반응속도 훈련"));
	case EGameModeId::Defense:           return FText::FromString(TEXT("수비 훈련"));
	case EGameModeId::BaseRunning:       return FText::FromString(TEXT("베이스 러닝"));
	case EGameModeId::Batting:           return FText::FromString(TEXT("타격 훈련"));
	case EGameModeId::FunctionalFitness: return FText::FromString(TEXT("기능성 피트니스"));
	case EGameModeId::AICoaching:        return FText::FromString(TEXT("AI 코칭 (운동 추천)"));
	default:                             return FText::FromString(TEXT("알 수 없는 모드"));
	}
}

FName UModeManager::GetModeIdName(EGameModeId Mode)
{
	switch (Mode)
	{
	case EGameModeId::BodyScan:          return TEXT("BodyScan");
	case EGameModeId::ReactionSpeed:     return TEXT("ReactionSpeed");
	case EGameModeId::Defense:           return TEXT("Defense");
	case EGameModeId::BaseRunning:       return TEXT("BaseRunning");
	case EGameModeId::Batting:           return TEXT("Batting");
	case EGameModeId::FunctionalFitness: return TEXT("FunctionalFitness");
	case EGameModeId::AICoaching:        return TEXT("AICoaching");
	default:                             return NAME_None;
	}
}

FText UModeManager::GetModeDescription(EGameModeId Mode)
{
	switch (Mode)
	{
	case EGameModeId::BodyScan:
		return FText::FromString(TEXT("키·팔 길이·활동 범위를 측정해 개인 난이도 기준선을 만듭니다. (LiDAR)"));
	case EGameModeId::ReactionSpeed:
		return FText::FromString(TEXT("바닥에 점등되는 타깃을 밟아 반응속도와 콤보를 겨룹니다. (LiDAR)"));
	case EGameModeId::Defense:
		return FText::FromString(TEXT("좌우 이동·점프·숙이기로 타구를 처리합니다. (LiDAR 전신)"));
	case EGameModeId::BaseRunning:
		return FText::FromString(TEXT("주루 타이밍과 이동 속도로 세이프/아웃을 판정합니다. (LiDAR)"));
	case EGameModeId::Batting:
		return FText::FromString(TEXT("날아오는 공에 타이밍을 맞춰 스윙합니다. 정확도·효율·일관성 3축 채점."));
	case EGameModeId::FunctionalFitness:
		return FText::FromString(TEXT("관절각과 반복 횟수를 측정하는 야구 특화 피트니스. (LiDAR)"));
	case EGameModeId::AICoaching:
		return FText::FromString(TEXT("그동안의 기록을 분석해 약점과 추천 운동, AI 코칭을 보여줍니다. (읽기 전용)"));
	default:
		return FText::GetEmpty();
	}
}

bool UModeManager::IsModeImplemented(EGameModeId Mode)
{
	// ROADMAP Phase 1 기준: 타격만 플레이 가능. 나머지는 Phase 3~4 에서 열린다.
	// 모드를 구현하면 여기에 추가하고 AMotionBaseGameMode::GetPawnClassForMode 에도 폰을 등록할 것.
	return Mode == EGameModeId::Batting
		|| Mode == EGameModeId::Defense
		|| Mode == EGameModeId::AICoaching;   // 읽기 전용 리뷰 화면 (StartAICoaching 로 진입)
}

TArray<EDifficultyLevel> UModeManager::GetMenuDifficulties()
{
	return { EDifficultyLevel::Beginner, EDifficultyLevel::Amateur, EDifficultyLevel::Pro };
}

FText UModeManager::GetDifficultyDisplayName(EDifficultyLevel Level)
{
	switch (Level)
	{
	case EDifficultyLevel::Beginner: return FText::FromString(TEXT("초보"));
	case EDifficultyLevel::Amateur:  return FText::FromString(TEXT("아마추어"));
	case EDifficultyLevel::Pro:      return FText::FromString(TEXT("프로"));
	default:                         return FText::FromString(TEXT("알 수 없음"));
	}
}

FText UModeManager::GetDifficultyDescription(EDifficultyLevel Level)
{
	switch (Level)
	{
	case EDifficultyLevel::Beginner:
		return FText::FromString(TEXT("느린 직구 위주, 변화구 없음, 여유로운 간격. 타이밍 감을 익히는 단계."));
	case EDifficultyLevel::Amateur:
		return FText::FromString(TEXT("보통 구속에 변화구가 섞입니다. 코스도 넓어집니다."));
	case EDifficultyLevel::Pro:
		return FText::FromString(TEXT("빠른 구속과 잦은 변화구, 짧은 간격. 실전에 가까운 난이도."));
	default:
		return FText::GetEmpty();
	}
}

FName UModeManager::GetDifficultyIdName(EDifficultyLevel Level)
{
	switch (Level)
	{
	case EDifficultyLevel::Beginner: return TEXT("Beginner");
	case EDifficultyLevel::Amateur:  return TEXT("Amateur");
	case EDifficultyLevel::Pro:      return TEXT("Pro");
	default:                         return NAME_None;
	}
}

TArray<EBattingStance> UModeManager::GetMenuStances()
{
	return { EBattingStance::Right, EBattingStance::Left };
}

FText UModeManager::GetStanceDisplayName(EBattingStance Stance)
{
	switch (Stance)
	{
	case EBattingStance::Right: return FText::FromString(TEXT("우타"));
	case EBattingStance::Left:  return FText::FromString(TEXT("좌타"));
	default:                    return FText::FromString(TEXT("알 수 없음"));
	}
}

FText UModeManager::GetStanceDescription(EBattingStance Stance)
{
	switch (Stance)
	{
	case EBattingStance::Right:
		return FText::FromString(TEXT("오른손잡이 타석 — 홈플레이트 3루 쪽에 서서 왼쪽으로 당겨칩니다."));
	case EBattingStance::Left:
		return FText::FromString(TEXT("왼손잡이 타석 — 홈플레이트 1루 쪽에 서서 오른쪽으로 당겨칩니다."));
	default:
		return FText::GetEmpty();
	}
}

FName UModeManager::GetStanceIdName(EBattingStance Stance)
{
	switch (Stance)
	{
	case EBattingStance::Right: return TEXT("Right");
	case EBattingStance::Left:  return TEXT("Left");
	default:                    return NAME_None;
	}
}
