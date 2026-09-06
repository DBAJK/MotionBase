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

FString UAIFeedbackService::LoadApiKey() const
{
	// Config/Secrets.ini (gitignore) 를 임의 경로 ini 로 읽는다. GConfig 가 캐시한다.
	const FString SecretsPath = FPaths::ProjectConfigDir() / TEXT("Secrets.ini");
	FString Key;
	if (GConfig)
	{
		GConfig->GetString(TEXT("AI"), TEXT("ApiKey"), Key, SecretsPath);
	}
	return Key.TrimStartAndEnd();
}

bool UAIFeedbackService::IsConfigured() const
{
	return !LoadApiKey().IsEmpty();
}

FString UAIFeedbackService::BuildSystemPrompt(ECoachDomain Domain) const
{
	if (Domain == ECoachDomain::Throwing)
	{
		return TEXT(
			"You are a baseball throwing coach. Using ONLY the given 'throwing analysis' "
			"and 'recommended exercises', write 3-4 short, specific coaching sentences in English.\n"
			"Rules:\n"
			"- Cite only the numbers provided (accuracy to the target base, release velocity, transfer time); "
			"do not invent new figures or metrics.\n"
			"- Separate the causes: missing the zone is direction/step, low velocity is whole-body power, "
			"slow transfer is glove-to-hand footwork. Address the weakest one first.\n"
			"- For every exercise you name, say what it improves (from its 'benefit') and then give its "
			"'volume' EXACTLY as written. Example shape: \"Long toss builds arm endurance through a "
			"progressive range - 6 steps of 5 throws.\"\n"
			"- The volume is a prescription reviewed by a human. Copy the sets/reps/time/distance verbatim; "
			"never round, scale, or invent numbers, never add weights or loads, and never suggest an "
			"exercise that is not on the list.\n"
			"- Encouraging tone, but no exaggeration.\n"
			"- If an 'uncalibrated' note is present, hedge with words like 'roughly' instead of being absolute.\n"
			"- Output the coaching sentences only: no preamble, lists, or markdown.");
	}

	if (Domain == ECoachDomain::Backup)
	{
		return TEXT(
			"You are a baseball infield/outfield positioning coach. The player just took a BACKUP-POSITION "
			"JUDGMENT drill where they physically move (via a hand controller) to the correct backup spot "
			"after a situation is called, not a physical workout. Using ONLY the given 'judgment analysis' "
			"and 'recommended exercises', write 3-4 short, specific coaching sentences in English.\n"
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
			"- For every exercise you name, say what it improves (from its 'benefit') and then give its "
			"'volume' EXACTLY as written. Here the volume counts SITUATIONS, not workout sets - say "
			"\"2 sets of 10 cases\", never turn it into reps of a physical exercise.\n"
			"- Copy the volume verbatim; never round, scale, or invent numbers, and never suggest an "
			"exercise that is not on the list.\n"
			"- Encouraging tone, but no exaggeration.\n"
			"- If an 'uncalibrated' note is present, hedge with words like 'roughly' instead of being absolute.\n"
			"- Output the coaching sentences only: no preamble, lists, or markdown.");
	}

	if (Domain == ECoachDomain::Fielding)
	{
		return TEXT(
			"You are a baseball fielding (catching) coach. Using ONLY the given 'catch analysis' "
			"and 'recommended exercises', write 3-4 short, specific coaching sentences in English.\n"
			"Rules:\n"
			"- Cite only the numbers provided; do not invent new figures or metrics.\n"
			"- Point out fitness factors (reaction speed, upper-body flexibility, foot speed) that match the weaknesses.\n"
			"- For every exercise you name, say what it improves (from its 'benefit') and then give its "
			"'volume' EXACTLY as written. Example shape: \"Ladder quick steps raise your foot turnover so "
			"the first step comes quicker - 3 sets of 30 seconds.\"\n"
			"- The volume is a prescription reviewed by a human. Copy the sets/reps/time/distance verbatim; "
			"never round, scale, or invent numbers, never add weights or loads, and never suggest an "
			"exercise that is not on the list.\n"
			"- Encouraging tone, but no exaggeration.\n"
			"- If an 'uncalibrated' note is present, hedge with words like 'roughly' instead of being absolute.\n"
			"- Output the coaching sentences only: no preamble, lists, or markdown.");
	}

	return TEXT(
		"You are a baseball hitting coach. Using ONLY the given 'weakness analysis', 'training trend', "
		"and 'recommended drills', write 3-4 short, specific coaching sentences in English.\n"
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
		"- Output the coaching sentences only: no preamble, lists, or markdown.");
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

void UAIFeedbackService::RequestCatchCoaching(const FWeaknessReport& Report, const TArray<FTrainingDrill>& Drills)
{
	// 포구는 만성 추세 이력이 아직 없다 — 빈(무효) 추세를 넘겨 프롬프트에서 생략되게 한다.
	DispatchCoachingRequest(BuildRequestBody(ECoachDomain::Fielding, Report, Drills, FChronicWeaknessReport()));
}

void UAIFeedbackService::RequestThrowCoaching(const FWeaknessReport& Report, const TArray<FTrainingDrill>& Drills)
{
	DispatchCoachingRequest(BuildRequestBody(ECoachDomain::Throwing, Report, Drills, FChronicWeaknessReport()));
}

void UAIFeedbackService::RequestBackupCoaching(const FWeaknessReport& Report, const TArray<FTrainingDrill>& Drills)
{
	DispatchCoachingRequest(BuildRequestBody(ECoachDomain::Backup, Report, Drills, FChronicWeaknessReport()));
}

void UAIFeedbackService::DispatchCoachingRequest(const FString& Body)
{
	const FString ApiKey = LoadApiKey();
	if (ApiKey.IsEmpty())
	{
		// 키가 없으면 조용히 건너뛴다 — 약점 리포트·드릴은 이미 화면에 있다.
		UE_LOG(LogMotionBase, Log,
			TEXT("AIFeedback: API 키 미설정 (Config/Secrets.ini [AI] ApiKey). AI 코칭 생략."));
		OnFeedbackReady.Broadcast(false, TEXT("AI 코칭 미설정 (Config/Secrets.ini)"));
		return;
	}

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("https://api.anthropic.com/v1/messages"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("x-api-key"), ApiKey);
	Request->SetHeader(TEXT("anthropic-version"), TEXT("2023-06-01"));
	Request->SetContentAsString(Body);

	UE_LOG(LogMotionBase, Log, TEXT("AIFeedback: 코칭 요청 (model=%s, %d chars)"), *ModelId, Body.Len());

	// 완료 콜백 — this 가 async 도중 파괴될 수 있으므로 weak 가드.
	TWeakObjectPtr<UAIFeedbackService> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis](FHttpRequestPtr /*Req*/, FHttpResponsePtr Response, bool bConnected)
		{
			UAIFeedbackService* Self = WeakThis.Get();
			if (!Self)
			{
				return; // 서비스가 이미 사라짐
			}

			if (!bConnected || !Response.IsValid())
			{
				Self->OnFeedbackReady.Broadcast(false, TEXT("AI 코칭 요청 실패 (네트워크)"));
				return;
			}

			const int32 Code = Response->GetResponseCode();
			const FString Content = Response->GetContentAsString();

			if (Code != 200)
			{
				UE_LOG(LogMotionBase, Warning, TEXT("AIFeedback: HTTP %d — %s"), Code, *Content);
				Self->OnFeedbackReady.Broadcast(false, FString::Printf(TEXT("AI 코칭 오류 (HTTP %d)"), Code));
				return;
			}

			// 응답 파싱: { "content": [ { "type":"text", "text":"..." } ], ... }
			TSharedPtr<FJsonObject> Json;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
			if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
			{
				Self->OnFeedbackReady.Broadcast(false, TEXT("AI 코칭 응답 파싱 실패"));
				return;
			}

			const TArray<TSharedPtr<FJsonValue>>* ContentArr = nullptr;
			if (Json->TryGetArrayField(TEXT("content"), ContentArr) && ContentArr && ContentArr->Num() > 0)
			{
				const TSharedPtr<FJsonObject> First = (*ContentArr)[0]->AsObject();
				FString Text;
				if (First.IsValid() && First->TryGetStringField(TEXT("text"), Text))
				{
					Self->OnFeedbackReady.Broadcast(true, Text.TrimStartAndEnd());
					return;
				}
			}

			Self->OnFeedbackReady.Broadcast(false, TEXT("AI 코칭 응답에 텍스트가 없습니다"));
		});

	Request->ProcessRequest();
}
