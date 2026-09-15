#include "AI/AIFeedbackService.h"
#include "MotionBase.h"
#include "Analysis/WeaknessDetector.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/MonitoredProcess.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Async/Async.h"
#include "Containers/Ticker.h"

namespace
{
	/** Config/Secrets.ini (gitignore) 의 [AI] 값 하나를 읽는다. 임의 경로 ini — GConfig 가 캐시한다. */
	FString ReadSecret(const TCHAR* Key)
	{
		const FString SecretsPath = FPaths::ProjectConfigDir() / TEXT("Secrets.ini");
		FString Value;
		if (GConfig)
		{
			GConfig->GetString(TEXT("AI"), Key, Value, SecretsPath);
		}
		return Value.TrimStartAndEnd();
	}
}

FString UAIFeedbackService::LoadApiKey() const
{
	return ReadSecret(TEXT("ApiKey"));
}

bool UAIFeedbackService::IsCliBackendEnabled() const
{
#if PLATFORM_WINDOWS
	return ReadSecret(TEXT("Backend")).Equals(TEXT("ClaudeCodeCli"), ESearchCase::IgnoreCase);
#else
	return false;
#endif
}

bool UAIFeedbackService::IsConfigured() const
{
	// API 키가 우선. 키가 없어도 CLI 백엔드가 켜져 있고 실행 파일이 있으면 호출할 수 있다.
	return !LoadApiKey().IsEmpty() || (IsCliBackendEnabled() && !ResolveClaudeCliPath().IsEmpty());
}

FString UAIFeedbackService::BuildSystemPrompt(ECoachDomain Domain) const
{
	if (Domain == ECoachDomain::Throwing)
	{
		return FString::Printf(TEXT(
			"You are a baseball throwing coach. Using ONLY the given 'throwing analysis' "
			"and 'recommended exercises', write 3-4 short, specific coaching sentences in %s.\n"
			"Rules:\n"
			"- Cite only the numbers provided (accuracy to the target base, release velocity, transfer time); "
			"do not invent new figures or metrics.\n"
			"- Separate the causes: missing the zone is direction/step, low velocity is whole-body power, "
			"slow transfer is glove-to-hand footwork. Address the weakest one first.\n"
			"- If a 'training trend' is present, reflect it: encourage when 'improving', and when 'worsening' "
			"or 'chronic', say the weakness keeps coming back and to focus there. If no trend block is given, "
			"say nothing about progress over time - this may be their first session.\n"
			"- For every exercise you name, say what it improves (from its 'benefit') and then give its "
			"'volume' EXACTLY as written. Example shape: \"Long toss builds arm endurance through a "
			"progressive range - 6 steps of 5 throws.\"\n"
			"- The volume is a prescription reviewed by a human. Copy the sets/reps/time/distance verbatim; "
			"never round, scale, or invent numbers, never add weights or loads, and never suggest an "
			"exercise that is not on the list.\n"
			"- Encouraging tone, but no exaggeration.\n"
			"- If an 'uncalibrated' note is present, hedge with words like 'roughly' instead of being absolute.\n"
			"- Output the coaching sentences only: no preamble, lists, or markdown."), *OutputLanguage);
	}

	if (Domain == ECoachDomain::Backup)
	{
		return FString::Printf(TEXT(
			"You are a baseball infield/outfield positioning coach. The player just took a BACKUP-POSITION "
			"JUDGMENT drill where they physically move (via a hand controller) to the correct backup spot "
			"after a situation is called, not a physical workout. Using ONLY the given 'judgment analysis' "
			"and 'recommended exercises', write 3-4 short, specific coaching sentences in %s.\n"
			"Rules:\n"
			"- This is about DECISION MAKING and ROUTE-TAKING, not fitness. Never prescribe strength, speed, "
			"flexibility, or conditioning work here.\n"
			"- The player moves with a controller, not their legs - never comment on running speed, "
			"quickness, or physical stamina. If 'route efficiency' is low, frame it as hesitating or "
			"changing your mind mid-route, not as being slow.\n"
			"- The two decision variables are the batted-ball direction and the runner situation - "
			"frame the advice around those.\n"
			"- Cite only the numbers provided (correct rate, decision time, route efficiency, which cases "
			"were missed); do not invent new figures.\n"
			"- The breakdown lists each case: the situation, what the correct job was (back up a base, cover "
			"a base, cut off the throw, or hold your spot), and how it went. Name a specific missed case "
			"rather than only the totals - that is what makes the advice usable.\n"
			"- If a 'training trend' is present, reflect it: encourage when 'improving', and when 'worsening' "
			"or 'chronic', say the judgment error keeps coming back and to focus there. If no trend block is "
			"given, say nothing about progress over time - this may be their first session.\n"
			"- For every exercise you name, say what it improves (from its 'benefit') and then give its "
			"'volume' EXACTLY as written. Here the volume counts SITUATIONS, not workout sets - say "
			"\"2 sets of 10 cases\", never turn it into reps of a physical exercise.\n"
			"- Copy the volume verbatim; never round, scale, or invent numbers, and never suggest an "
			"exercise that is not on the list.\n"
			"- Encouraging tone, but no exaggeration.\n"
			"- If an 'uncalibrated' note is present, hedge with words like 'roughly' instead of being absolute.\n"
			"- Output the coaching sentences only: no preamble, lists, or markdown."), *OutputLanguage);
	}

	if (Domain == ECoachDomain::Fielding)
	{
		return FString::Printf(TEXT(
			"You are a baseball fielding (catching) coach. Using ONLY the given 'catch analysis' "
			"and 'recommended exercises', write 3-4 short, specific coaching sentences in %s.\n"
			"Rules:\n"
			"- Cite only the numbers provided; do not invent new figures or metrics.\n"
			"- Point out fitness factors (reaction speed, upper-body flexibility, foot speed) that match the weaknesses.\n"
			"- If a 'training trend' is present, reflect it: encourage when 'improving', and when 'worsening' "
			"or 'chronic', say the weakness keeps coming back and to focus there. If no trend block is given, "
			"say nothing about progress over time - this may be their first session.\n"
			"- For every exercise you name, say what it improves (from its 'benefit') and then give its "
			"'volume' EXACTLY as written. Example shape: \"Ladder quick steps raise your foot turnover so "
			"the first step comes quicker - 3 sets of 30 seconds.\"\n"
			"- The volume is a prescription reviewed by a human. Copy the sets/reps/time/distance verbatim; "
			"never round, scale, or invent numbers, never add weights or loads, and never suggest an "
			"exercise that is not on the list.\n"
			"- Encouraging tone, but no exaggeration.\n"
			"- If an 'uncalibrated' note is present, hedge with words like 'roughly' instead of being absolute.\n"
			"- Output the coaching sentences only: no preamble, lists, or markdown."), *OutputLanguage);
	}

	return FString::Printf(TEXT(
		"You are a baseball hitting coach. Using ONLY the given 'weakness analysis', 'training trend', "
		"and 'recommended drills', write 3-4 short, specific coaching sentences in %s.\n"
		"Rules:\n"
		"- Cite only the numbers provided; do not invent new figures or metrics.\n"
		"- If a 'training trend' is present, reflect it: encourage when 'improving', and when 'worsening' "
		"or 'chronic', note the recurring weakness and urge focus on that drill. If no trend, do not mention it.\n"
		"- For every drill you name, say what it improves (from its 'benefit') and then give its 'volume' "
		"EXACTLY as written. Example shape: \"Deadlifts build hip extension and core stability, the base of "
		"your rotation - 3 sets of 15 reps.\"\n"
		"- The volume is a prescription reviewed by a human. Copy the sets/reps/time/distance verbatim; "
		"never round, scale, or invent numbers, never add weights or loads, and never suggest a drill that "
		"is not on the list.\n"
		"- Encouraging tone, but no exaggeration.\n"
		"- If an 'uncalibrated' note is present, hedge with words like 'roughly' instead of being absolute.\n"
		"- Output the coaching sentences only: no preamble, lists, or markdown."), *OutputLanguage);
}

FString UAIFeedbackService::BuildUserPrompt(const FWeaknessReport& Report, const TArray<FTrainingDrill>& Drills,
	const FChronicWeaknessReport& Chronic) const
{
	FString P = TEXT("[Analysis]\n");
	P += UWeaknessDetector::SummarizeReport(Report);

	// 이력이 쌓였을 때만 추세 블록을 넣는다 — 없으면 LLM 이 지어내지 않도록 아예 생략.
	if (Chronic.bValid && Chronic.Trends.Num() > 0)
	{
		P += FString::Printf(TEXT("\n\n[Training trend] (last %d sessions)\n"), Chronic.SessionsAnalyzed);
		const int32 ShowN = FMath::Min(Chronic.Trends.Num(), 3);
		for (int32 i = 0; i < ShowN; ++i)
		{
			const FAxisTrend& T = Chronic.Trends[i];
			P += FString::Printf(TEXT("- %s: appeared in %d/%d sessions, %s%s\n"),
				*UWeaknessDetector::GetAxisDisplayName(T.Axis).ToString(),
				T.AppearanceCount, T.WindowSize,
				*UWeaknessDetector::GetTrendDisplayName(T.Trend).ToString(),
				T.bChronic ? TEXT(", chronic") : TEXT(""));
		}
	}

	P += TEXT("\n\n[Recommended exercises]\n");
	if (Drills.Num() == 0)
	{
		P += TEXT("- (none)\n");
	}
	else
	{
		for (const FTrainingDrill& D : Drills)
		{
			// benefit / volume 을 별도 줄로 분리해 내려보낸다 — 한 줄에 뭉치면 LLM 이
			// 수행량을 "대략 15회쯤" 식으로 고쳐 쓰는 경향이 있다. 라벨을 붙여 인용을 강제한다.
			P += FString::Printf(TEXT("- %s: %s (focus: %s)\n"), *D.Name, *D.Description, *D.FocusCue);
			if (!D.Benefit.IsEmpty())
			{
				P += FString::Printf(TEXT("    benefit: %s\n"), *D.Benefit);
			}
			if (!D.Prescription.IsEmpty())
			{
				P += FString::Printf(TEXT("    volume: %s\n"), *D.Prescription);
			}
		}
	}

	P += TEXT("\nUsing the analysis and exercises above, write the coaching.");
	return P;
}

FString UAIFeedbackService::BuildRequestBody(ECoachDomain Domain, const FWeaknessReport& Report, const TArray<FTrainingDrill>& Drills,
	const FChronicWeaknessReport& Chronic) const
{
	// FJsonObject 로 구성해 한글·따옴표 이스케이프를 안전하게 처리.
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("model"), ModelId);
	Root->SetNumberField(TEXT("max_tokens"), MaxTokens);
	Root->SetStringField(TEXT("system"), BuildSystemPrompt(Domain));

	const TSharedRef<FJsonObject> UserMsg = MakeShared<FJsonObject>();
	UserMsg->SetStringField(TEXT("role"), TEXT("user"));
	UserMsg->SetStringField(TEXT("content"), BuildUserPrompt(Report, Drills, Chronic));

	TArray<TSharedPtr<FJsonValue>> Messages;
	Messages.Add(MakeShared<FJsonValueObject>(UserMsg));
	Root->SetArrayField(TEXT("messages"), Messages);

	FString Body;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(Root, Writer);
	return Body;
}

void UAIFeedbackService::RequestSwingCoaching(const FWeaknessReport& Report, const TArray<FTrainingDrill>& Drills,
	const FChronicWeaknessReport& Chronic)
{
	DispatchCoachingRequest(BuildRequestBody(ECoachDomain::Batting, Report, Drills, Chronic));
}

// ⚠️ 수비 3종목도 만성 추세를 싣는다. 예전엔 빈 FChronicWeaknessReport() 를 넘겼는데,
//    각 수비 폰은 드릴 추천(RecommendWithHistory)을 위해 **이미 AnalyzeTrend 를 돌려 결과를
//    갖고 있었다.** 계산해 놓고 프롬프트에서만 버리던 값이라, 넘기는 데 드는 비용이 0 이다.
//    Chronic.bValid=false(이력 부족)면 BuildUserPrompt 가 추세 블록 자체를 생략하므로
//    첫 세션 사용자에게 LLM 이 없는 추세를 지어낼 여지도 없다.

void UAIFeedbackService::RequestCatchCoaching(const FWeaknessReport& Report, const TArray<FTrainingDrill>& Drills,
	const FChronicWeaknessReport& Chronic)
{
	DispatchCoachingRequest(BuildRequestBody(ECoachDomain::Fielding, Report, Drills, Chronic));
}

void UAIFeedbackService::RequestThrowCoaching(const FWeaknessReport& Report, const TArray<FTrainingDrill>& Drills,
	const FChronicWeaknessReport& Chronic)
{
	DispatchCoachingRequest(BuildRequestBody(ECoachDomain::Throwing, Report, Drills, Chronic));
}

void UAIFeedbackService::RequestBackupCoaching(const FWeaknessReport& Report, const TArray<FTrainingDrill>& Drills,
	const FChronicWeaknessReport& Chronic)
{
	DispatchCoachingRequest(BuildRequestBody(ECoachDomain::Backup, Report, Drills, Chronic));
}

void UAIFeedbackService::DispatchCoachingRequest(const FString& Body)
{
	// 세션 코칭 — 모델은 ModelId(기본 opus). 세션당 1회라 품질을 택한다.
	DispatchRequest(Body, ModelId, [](UAIFeedbackService* Self, bool bSuccess, const FString& Text)
		{
			Self->OnFeedbackReady.Broadcast(bSuccess, Text);
		});
}

void UAIFeedbackService::DispatchRequest(const FString& Body, const FString& Model,
	TFunction<void(UAIFeedbackService*, bool, const FString&)> OnComplete, bool bIsRetry)
{
	const FString ApiKey = LoadApiKey();
	if (ApiKey.IsEmpty())
	{
		// 키가 없고 개발용 CLI 백엔드가 켜져 있으면 그쪽으로 보낸다 (Secrets.ini [AI] Backend=ClaudeCodeCli).
		if (IsCliBackendEnabled())
		{
			DispatchCliRequest(Body, OnComplete);
			return;
		}

		// 키가 없으면 조용히 건너뛴다 — 약점 리포트·드릴·저작 해설은 이미 화면에 있다.
		UE_LOG(LogMotionBase, Log,
			TEXT("AIFeedback: API 키 미설정 (Config/Secrets.ini [AI] ApiKey). AI 호출 생략."));
		OnComplete(this, false, TEXT("AI 코칭 미설정 (Config/Secrets.ini)"));
		return;
	}

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("https://api.anthropic.com/v1/messages"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("x-api-key"), ApiKey);
	Request->SetHeader(TEXT("anthropic-version"), TEXT("2023-06-01"));
	Request->SetContentAsString(Body);
	// 부스 와이파이가 불안정해도 요청이 무한정 매달리지 않게 명시적 타임아웃을 둔다.
	Request->SetTimeout(15.0f);

	UE_LOG(LogMotionBase, Log, TEXT("AIFeedback: 요청 (model=%s, %d chars%s)"), *Model, Body.Len(),
		bIsRetry ? TEXT(", 재시도") : TEXT(""));

	// HTTP 가 **어떤 이유로든** 최종 실패하면(네트워크·4xx·재시도 후 429/5xx·파싱 실패) CLI 백엔드가 켜져 있을 때
	// 같은 본문을 CLI 로 다시 보낸다 — 키는 넣어 두되 크레딧이 없는 개발 환경(HTTP 400)을 위한 대체 경로.
	// ⚠️ 429/5xx 1회 재시도는 원래 OnComplete 로 DispatchRequest 를 다시 부르므로, 대체는 최종 실패에서 한 번만 일어난다.
	TFunction<void(UAIFeedbackService*, bool, const FString&)> HttpComplete = OnComplete;
	if (IsCliBackendEnabled())
	{
		HttpComplete = [Body, OnComplete](UAIFeedbackService* Self, bool bSuccess, const FString& Text)
		{
			if (bSuccess)
			{
				OnComplete(Self, true, Text);
				return;
			}
			UE_LOG(LogMotionBase, Warning, TEXT("AIFeedback: API 호출 실패 (%s) — Claude Code CLI 로 대체 시도"), *Text);
			Self->DispatchCliRequest(Body, OnComplete);
		};
	}

	// 완료 콜백 — this 가 async 도중 파괴될 수 있으므로 weak 가드.
	TWeakObjectPtr<UAIFeedbackService> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, OnComplete, HttpComplete, Body, Model, bIsRetry](FHttpRequestPtr /*Req*/, FHttpResponsePtr Response, bool bConnected)
		{
			UAIFeedbackService* Self = WeakThis.Get();
			if (!Self)
			{
				return; // 서비스가 이미 사라짐
			}

			if (!bConnected || !Response.IsValid())
			{
				HttpComplete(Self, false, TEXT("AI 코칭 요청 실패 (네트워크)"));
				return;
			}

			const int32 Code = Response->GetResponseCode();
			const FString Content = Response->GetContentAsString();

			if (Code != 200)
			{
				// 429(과부하)·5xx(서버 오류)는 일시적일 때가 많다 — 짧은 대기 후 한 번만
				// 자동 재시도한다. 4xx(요청 자체가 잘못됨)는 재시도해도 결과가 같으므로
				// 바로 실패 사유를 보여준다.
				const bool bTransient = (Code == 429) || (Code >= 500);
				if (bTransient && !bIsRetry)
				{
					UE_LOG(LogMotionBase, Warning,
						TEXT("AIFeedback: HTTP %d — 일시적 오류로 보고 1.5초 뒤 1회 재시도"), Code);
					TWeakObjectPtr<UAIFeedbackService> RetryWeak = WeakThis;
					FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
						[RetryWeak, Body, Model, OnComplete](float) -> bool
						{
							if (UAIFeedbackService* RetrySelf = RetryWeak.Get())
							{
								RetrySelf->DispatchRequest(Body, Model, OnComplete, /*bIsRetry=*/true);
							}
							return false; // 한 번만 실행하고 티커에서 스스로 제거.
						}), 1.5f);
					return;
				}

				UE_LOG(LogMotionBase, Warning, TEXT("AIFeedback: HTTP %d — %s"), Code, *Content);
				HttpComplete(Self, false, FString::Printf(TEXT("AI 코칭 오류 (HTTP %d)"), Code));
				return;
			}

			// 응답 파싱: { "content": [ { "type":"text", "text":"..." }, ... ], ... }
			// content[0] 만 보면 앞에 다른 블록(향후 tool_use 등)이 오는 응답에서 실패한다 —
			// 배열을 순회해 type=="text" 인 첫 블록을 쓴다.
			TSharedPtr<FJsonObject> Json;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
			if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
			{
				HttpComplete(Self, false, TEXT("AI 코칭 응답 파싱 실패"));
				return;
			}

			const TArray<TSharedPtr<FJsonValue>>* ContentArr = nullptr;
			if (Json->TryGetArrayField(TEXT("content"), ContentArr) && ContentArr)
			{
				for (const TSharedPtr<FJsonValue>& Item : *ContentArr)
				{
					const TSharedPtr<FJsonObject> Obj = Item.IsValid() ? Item->AsObject() : nullptr;
					if (!Obj.IsValid())
					{
						continue;
					}
					FString Type;
					FString Text;
					if (Obj->TryGetStringField(TEXT("type"), Type) && Type == TEXT("text")
						&& Obj->TryGetStringField(TEXT("text"), Text))
					{
						HttpComplete(Self, true, Text.TrimStartAndEnd());
						return;
					}
				}
			}

			HttpComplete(Self, false, TEXT("AI 코칭 응답에 텍스트가 없습니다"));
		});

	Request->ProcessRequest();
}

FString UAIFeedbackService::BuildExplanationBody(const FBackupExplainRequest& Req) const
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("model"), ExplanationModelId);
	Root->SetNumberField(TEXT("max_tokens"), ExplanationMaxTokens);
	Root->SetStringField(TEXT("system"), FString::Printf(TEXT(
		"You are a baseball coach explaining ONE backup assignment that has ALREADY been decided by the "
		"game's rulebook. Write 1-2 short sentences in %s explaining WHY that is this fielder's job.\n"
		"- **Hard limit: 25 words total.** This is read on a small panel inside a VR headset while the "
		"player is standing on the field; anything longer gets cut off mid-sentence.\n"
		"Rules:\n"
		"- **The given assignment is correct and final. Never contradict it, never suggest a different base "
		"or a different job, never hedge about whether it is right.** You are explaining it, not reviewing it.\n"
		"- Ground the reason in the two things that decide it: where the ball went, and where the throw is "
		"going because of the runners. Say the causal chain, e.g. \"the throw is going to third, so someone "
		"has to be behind it in case it gets away.\"\n"
		"- Speak to the player as \"you\". Present tense.\n"
		"- Do not restate the situation text verbatim - the player just saw it. Add the reasoning it implies.\n"
		"- No preamble, no lists, no markdown. Just the sentences."), *OutputLanguage));

	FString User;
	User += FString::Printf(TEXT("Fielder: %s\n"), *Req.PositionName);
	User += FString::Printf(TEXT("Situation: %s\n"), *Req.Situation);
	User += FString::Printf(TEXT("Runners: %s\n"), *Req.RunnerText);
	User += FString::Printf(TEXT("Your job on this play: %s\n"), *Req.JobText);
	User += FString::Printf(TEXT("Rulebook's one-line reason (expand on this, do not contradict it): %s\n"),
		*Req.AuthoredExplain);
	User += TEXT("\nExplain why this is your job.");

	const TSharedRef<FJsonObject> UserMsg = MakeShared<FJsonObject>();
	UserMsg->SetStringField(TEXT("role"), TEXT("user"));
	UserMsg->SetStringField(TEXT("content"), User);

	TArray<TSharedPtr<FJsonValue>> Messages;
	Messages.Add(MakeShared<FJsonValueObject>(UserMsg));
	Root->SetArrayField(TEXT("messages"), Messages);

	FString Body;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(Root, Writer);
	return Body;
}

void UAIFeedbackService::RequestPlayExplanation(const FBackupExplainRequest& Req)
{
	const FString Key = Req.CacheKey;
	DispatchRequest(BuildExplanationBody(Req), ExplanationModelId,
		[Key](UAIFeedbackService* Self, bool bSuccess, const FString& Text)
		{
			// 실패해도 조용히 넘어간다 — 호출부가 저작 해설로 되돌아간다.
			Self->OnPlayExplanationReady.Broadcast(bSuccess, Key, bSuccess ? Text : FString());
		});
}

// ── 개발용 백엔드: Claude Code CLI ──────────────────────────────────────────
// ⚠️ 로그인된 개인 Claude 구독으로 호출한다 — 본인 개발·테스트 전용.
//    시연·부스처럼 다른 사람이 쓰는 환경에서는 API 키(Secrets.ini [AI] ApiKey)를 쓸 것.

FString UAIFeedbackService::ResolveClaudeCliPath() const
{
#if PLATFORM_WINDOWS
	IFileManager& FM = IFileManager::Get();

	// 1) Secrets.ini 에 명시한 경로가 있으면 그것만 본다 (틀렸으면 자동 탐색으로 넘어가지 않는다).
	const FString Configured = ReadSecret(TEXT("ClaudeCliPath"));
	if (!Configured.IsEmpty())
	{
		return FM.FileExists(*Configured) ? Configured : FString();
	}

	const FString UserProfile = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
	const FString AppData = FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"));

	// 2) 단독 설치 (네이티브 설치 / npm -g)
	for (const FString& Candidate : { UserProfile / TEXT(".local/bin/claude.exe"), AppData / TEXT("npm/claude.cmd") })
	{
		if (FM.FileExists(*Candidate))
		{
			return Candidate;
		}
	}

	// 3) Claude 데스크톱 앱 번들 — 앱이 업데이트될 때마다 버전 폴더가 바뀌므로 가장 최근 것을 고른다.
	//    ⚠️ 스토어(MSIX) 설치본은 %APPDATA% 가 앱 안에서만 가상화돼 보인다. 앱 밖(에디터·게임)에서는
	//    실제 파일이 %LOCALAPPDATA%\Packages\Claude_<id>\LocalCache\Roaming 아래에 있으므로 두 곳 모두 본다.
	TArray<FString> BundleRoots = { AppData / TEXT("Claude/claude-code") };
	const FString PackagesDir = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA")) / TEXT("Packages");
	TArray<FString> PackageDirs;
	FM.FindFiles(PackageDirs, *(PackagesDir / TEXT("Claude_*")), /*Files=*/false, /*Directories=*/true);
	for (const FString& Package : PackageDirs)
	{
		BundleRoots.Add(PackagesDir / Package / TEXT("LocalCache/Roaming/Claude/claude-code"));
	}

	FString Best;
	FDateTime BestTime = FDateTime::MinValue();
	for (const FString& BundleRoot : BundleRoots)
	{
		TArray<FString> VersionDirs;
		FM.FindFiles(VersionDirs, *(BundleRoot / TEXT("*")), /*Files=*/false, /*Directories=*/true);
		for (const FString& Dir : VersionDirs)
		{
			const FString Exe = BundleRoot / Dir / TEXT("claude.exe");
			const FDateTime Stamp = FM.GetTimeStamp(*Exe); // 없으면 MinValue
			if (Stamp > BestTime)
			{
				BestTime = Stamp;
				Best = Exe;
			}
		}
	}
	return Best;
#else
	return FString();
#endif
}

void UAIFeedbackService::DispatchCliRequest(const FString& Body,
	TFunction<void(UAIFeedbackService*, bool, const FString&)> OnComplete)
{
	// HTTP 경로와 같은 요청 본문에서 model / system / 사용자 메시지를 꺼낸다 —
	// 프롬프트 빌더를 백엔드마다 따로 두지 않기 위함.
	FString Model;
	FString System;
	FString User;
	{
		TSharedPtr<FJsonObject> Json;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
		{
			Json->TryGetStringField(TEXT("model"), Model);
			Json->TryGetStringField(TEXT("system"), System);
			const TArray<TSharedPtr<FJsonValue>>* Messages = nullptr;
			if (Json->TryGetArrayField(TEXT("messages"), Messages) && Messages && Messages->Num() > 0)
			{
				const TSharedPtr<FJsonObject> First = (*Messages)[0].IsValid() ? (*Messages)[0]->AsObject() : nullptr;
				if (First.IsValid())
				{
					First->TryGetStringField(TEXT("content"), User);
				}
			}
		}
	}
	if (User.IsEmpty())
	{
		OnComplete(this, false, TEXT("AI 코칭 요청 실패 (본문 파싱)"));
		return;
	}
	if (Model.IsEmpty())
	{
		Model = ModelId;
	}

	FString CliPath = ResolveClaudeCliPath();
	if (CliPath.IsEmpty())
	{
		UE_LOG(LogMotionBase, Warning,
			TEXT("AIFeedback: Backend=ClaudeCodeCli 인데 claude 실행 파일을 찾지 못함 (Secrets.ini [AI] ClaudeCliPath 로 지정 가능)."));
		OnComplete(this, false, TEXT("AI 코칭 미설정 (Claude Code CLI 없음)"));
		return;
	}

	// 프롬프트는 임시 파일로 넘긴다 — 여러 줄·따옴표를 명령줄 인자로 이스케이프하는 것보다 안전하다.
	// 작업 폴더도 여기로 둔다: 프로젝트 루트에서 실행하면 CLI 가 CLAUDE.md 를 프롬프트에 섞는다.
	const FString WorkDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("AI/ClaudeCli"));
	IFileManager::Get().MakeDirectory(*WorkDir, /*Tree=*/true);
	const FString RequestId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	FString SystemFile = WorkDir / (RequestId + TEXT("_system.txt"));
	FString UserFile = WorkDir / (RequestId + TEXT("_user.txt"));
	FFileHelper::SaveStringToFile(System, *SystemFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	FFileHelper::SaveStringToFile(User, *UserFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	FPaths::MakePlatformFilename(SystemFile);
	FPaths::MakePlatformFilename(UserFile);
	FPaths::MakePlatformFilename(CliPath);

	// cmd.exe 를 거치는 이유: 사용자 프롬프트를 표준 입력(<)으로 넣고, npm 설치본(claude.cmd)도 같은 경로로 띄우기 위해.
	//   --tools ""           : 도구 전부 끔 (문장 생성만)
	//   --setting-sources "" : 사용자 settings(훅·권한 등)를 읽지 않음
	const FString Params = FString::Printf(
		TEXT("/d /s /c \"\"%s\" -p --output-format json --model %s --system-prompt-file \"%s\" --tools \"\" --setting-sources \"\" --no-session-persistence < \"%s\"\""),
		*CliPath, *Model, *SystemFile, *UserFile);

	const TSharedPtr<FMonitoredProcess> Proc =
		MakeShared<FMonitoredProcess>(TEXT("cmd.exe"), Params, WorkDir, /*InHidden=*/true, /*InCreatePipes=*/true);

	// 완료·취소 콜백은 **모니터 스레드**에서 불린다 → 게임 스레드로 넘겨 처리한다.
	// ⚠️ 콜백 안에서 Proc 을 TSharedPtr 로 붙잡지 말 것: 마지막 참조가 모니터 스레드에서 풀리면
	//    소멸자가 자기 스레드의 종료를 기다려 멈춘다. 그래서 raw 포인터로 식별만 한다.
	const FMonitoredProcess* RawProc = Proc.Get();
	TWeakObjectPtr<UAIFeedbackService> WeakThis(this);
	auto Finish = [WeakThis, RawProc, OnComplete, SystemFile, UserFile](bool bCanceled, int32 ReturnCode, const FString& Output)
	{
		AsyncTask(ENamedThreads::GameThread,
			[WeakThis, RawProc, OnComplete, SystemFile, UserFile, bCanceled, ReturnCode, Output]()
			{
				IFileManager::Get().Delete(*SystemFile, /*RequireExists=*/false, /*EvenReadOnly=*/false, /*Quiet=*/true);
				IFileManager::Get().Delete(*UserFile, /*RequireExists=*/false, /*EvenReadOnly=*/false, /*Quiet=*/true);

				UAIFeedbackService* Self = WeakThis.Get();
				if (!Self)
				{
					return; // 서비스가 이미 사라짐 (BeginDestroy 가 프로세스를 정리했다)
				}
				Self->RunningCliProcesses.RemoveAll(
					[RawProc](const TSharedPtr<FMonitoredProcess>& P) { return P.Get() == RawProc; });

				if (bCanceled)
				{
					UE_LOG(LogMotionBase, Warning, TEXT("AIFeedback: Claude Code CLI 시간 초과 (%.0f초) — 취소"), Self->CliTimeoutSec);
					OnComplete(Self, false, TEXT("AI 코칭 요청 실패 (시간 초과)"));
					return;
				}

				// 출력은 JSON 한 덩어리. 앞뒤에 다른 줄이 섞여도 되게 첫 '{' ~ 마지막 '}' 만 파싱한다.
				TSharedPtr<FJsonObject> Json;
				int32 Start = INDEX_NONE;
				int32 End = INDEX_NONE;
				if (Output.FindChar(TEXT('{'), Start) && Output.FindLastChar(TEXT('}'), End) && End > Start)
				{
					const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Output.Mid(Start, End - Start + 1));
					FJsonSerializer::Deserialize(Reader, Json);
				}
				if (!Json.IsValid())
				{
					UE_LOG(LogMotionBase, Warning, TEXT("AIFeedback: Claude Code CLI 출력 파싱 실패 (exit %d) — %s"),
						ReturnCode, *Output.Left(500));
					OnComplete(Self, false, TEXT("AI 코칭 응답 파싱 실패"));
					return;
				}

				FString Result;
				bool bJsonError = false;
				Json->TryGetStringField(TEXT("result"), Result);
				Json->TryGetBoolField(TEXT("is_error"), bJsonError);
				Result.TrimStartAndEndInline();

				if (bJsonError || ReturnCode != 0 || Result.IsEmpty())
				{
					UE_LOG(LogMotionBase, Warning, TEXT("AIFeedback: Claude Code CLI 오류 (exit %d) — %s"), ReturnCode, *Result);
					// 가장 흔한 원인은 CLI 미로그인·로그인 만료 — 화면에서 바로 알 수 있게 따로 표시한다.
					// (만료 시 문구 예: "Failed to authenticate: OAuth session expired and could not be refreshed")
					const bool bNotLoggedIn = Result.Contains(TEXT("Not logged in"))
						|| Result.Contains(TEXT("/login"))
						|| Result.Contains(TEXT("authenticate"))
						|| Result.Contains(TEXT("OAuth"));
					OnComplete(Self, false, bNotLoggedIn
						? TEXT("AI 코칭 오류 (Claude Code 로그인 필요)")
						: TEXT("AI 코칭 오류 (Claude Code CLI)"));
					return;
				}

				OnComplete(Self, true, Result);
			});
	};

	Proc->OnCompleted().BindLambda([RawProc, Finish](int32 ReturnCode)
		{
			// bIsRunning 이 내려간 뒤 호출되므로 전체 출력 버퍼를 읽어도 안전하다.
			Finish(/*bCanceled=*/false, ReturnCode, RawProc->GetFullOutputWithoutDelegate());
		});
	Proc->OnCanceled().BindLambda([Finish]()
		{
			Finish(/*bCanceled=*/true, -1, FString());
		});

	if (!Proc->Launch())
	{
		IFileManager::Get().Delete(*SystemFile, false, false, true);
		IFileManager::Get().Delete(*UserFile, false, false, true);
		OnComplete(this, false, TEXT("AI 코칭 요청 실패 (Claude Code CLI 실행 불가)"));
		return;
	}
	// 완료 처리는 게임 스레드 태스크로 오므로, 지금(게임 스레드) 추가하는 게 항상 먼저다.
	RunningCliProcesses.Add(Proc);

	UE_LOG(LogMotionBase, Log, TEXT("AIFeedback: Claude Code CLI 요청 (model=%s, %d chars)"), *Model, User.Len());

	// CLI 는 요청마다 프로세스를 새로 띄워 HTTP 보다 느리다 — 넉넉히 두되 무한정 매달리지 않게 끊는다.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[WeakThis, RawProc](float) -> bool
		{
			UAIFeedbackService* Self = WeakThis.Get();
			if (!Self)
			{
				return false;
			}
			const TSharedPtr<FMonitoredProcess>* Found = Self->RunningCliProcesses.FindByPredicate(
				[RawProc](const TSharedPtr<FMonitoredProcess>& P) { return P.Get() == RawProc; });
			if (!Found)
			{
				return false; // 이미 끝남
			}
			if ((*Found)->GetDuration().GetTotalSeconds() > Self->CliTimeoutSec)
			{
				(*Found)->Cancel(/*InKillTree=*/true); // cmd.exe 아래 claude 까지 같이 끊는다
				return false;
			}
			return true;
		}), 1.0f);
}

void UAIFeedbackService::BeginDestroy()
{
	// 진행 중인 CLI 프로세스를 끊는다. 소멸자가 모니터 스레드 종료를 기다리므로 여기서 비워도 안전하다.
	for (const TSharedPtr<FMonitoredProcess>& P : RunningCliProcesses)
	{
		if (P.IsValid())
		{
			P->Cancel(/*InKillTree=*/true);
		}
	}
	RunningCliProcesses.Empty();

	Super::BeginDestroy();
}
