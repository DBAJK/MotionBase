using UnrealBuildTool;
using System.Collections.Generic;

public class MotionBaseEditorTarget : TargetRules
{
	public MotionBaseEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		// UE 5.8 기본값. 경고 4종(UndefinedIdentifier/ReturnType/Dangling/Unreachable)이
		// Error 로 승격된다 — 설치형 엔진과 동일 설정이라 빌드환경 충돌도 사라진다.
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("MotionBase");
	}
}
