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

FString UAIFeedbackService::BuildSystemPrompt() const
{
	return TEXT(
		"당신은 야구 타격 코치입니다. 주어진 '약점 분석'과 '추천 드릴'만 근거로 "
		"한국어로 2~3문장의 짧고 구체적인 코칭을 작성하세요.\n"
		"규칙:\n"
		"- 제공된 숫자만 인용하고, 새로운 수치나 지표를 지어내지 마세요.\n"
		"- 추천 드릴을 이름으로 자연스럽게 언급하세요.\n"
		"- 격려하는 톤이되 과장하지 마세요.\n"
		"- '미보정' 표시가 있으면 단정하지 말고 '대략' 같은 표현을 쓰세요.\n"
		"- 코칭 문장만 출력하고 머리말·목록·마크다운은 쓰지 마세요.");
}

FString UAIFeedbackService::BuildUserPrompt(const FWeaknessReport& Report, const TArray<FTrainingDrill>& Drills) const
{
	FString P = TEXT("[약점 분석]\n");
	P += UWeaknessDetector::SummarizeReport(Report);

	P += TEXT("\n\n[추천 드릴]\n");
	if (Drills.Num() == 0)
	{
		P += TEXT("- (없음)\n");
	}
	else
	{
		for (const FTrainingDrill& D : Drills)
		{
			P += FString::Printf(TEXT("- %s: %s (핵심: %s)\n"), *D.Name, *D.Description, *D.FocusCue);
		}
	}

	P += TEXT("\n위 분석과 드릴을 근거로 코칭을 작성해 주세요.");
	return P;
}

FString UAIFeedbackService::BuildRequestBody(const FWeaknessReport& Report, const TArray<FTrainingDrill>& Drills) const
{
	// FJsonObject 로 구성해 한글·따옴표 이스케이프를 안전하게 처리.
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("model"), ModelId);
	Root->SetNumberField(TEXT("max_tokens"), MaxTokens);
	Root->SetStringField(TEXT("system"), BuildSystemPrompt());

	const TSharedRef<FJsonObject> UserMsg = MakeShared<FJsonObject>();
	UserMsg->SetStringField(TEXT("role"), TEXT("user"));
	UserMsg->SetStringField(TEXT("content"), BuildUserPrompt(Report, Drills));

	TArray<TSharedPtr<FJsonValue>> Messages;
	Messages.Add(MakeShared<FJsonValueObject>(UserMsg));
	Root->SetArrayField(TEXT("messages"), Messages);

	FString Body;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(Root, Writer);
	return Body;
}

void UAIFeedbackService::RequestSwingCoaching(const FWeaknessReport& Report, const TArray<FTrainingDrill>& Drills)
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

	const FString Body = BuildRequestBody(Report, Drills);

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
