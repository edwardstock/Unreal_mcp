// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Tests/McpMaterialPureUnitTests.cpp
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionScalarParameter.h"

#include "../McpFunctionInputTypeName.h"
#include "../McpHandlerUtils.h"

// Pure-function tests only. No mock socket, no subsystem invocation.
// Anything handler-level is verified via live MCP probes documented in the plan.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpFunctionInputTypeNameTest,
    "LHGame.Mcp.Material.FunctionInputTypeName",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpFunctionInputTypeNameTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("FunctionInput_Scalar"),             McpFunctionInputTypeName(FunctionInput_Scalar),             FString(TEXT("Float1")));
    TestEqual(TEXT("FunctionInput_Vector2"),            McpFunctionInputTypeName(FunctionInput_Vector2),            FString(TEXT("Float2")));
    TestEqual(TEXT("FunctionInput_Vector3"),            McpFunctionInputTypeName(FunctionInput_Vector3),            FString(TEXT("Float3")));
    TestEqual(TEXT("FunctionInput_Vector4"),            McpFunctionInputTypeName(FunctionInput_Vector4),            FString(TEXT("Float4")));
    TestEqual(TEXT("FunctionInput_Texture2D"),          McpFunctionInputTypeName(FunctionInput_Texture2D),          FString(TEXT("Texture2D")));
    TestEqual(TEXT("FunctionInput_TextureCube"),        McpFunctionInputTypeName(FunctionInput_TextureCube),        FString(TEXT("TextureCube")));
    TestEqual(TEXT("FunctionInput_Texture2DArray"),     McpFunctionInputTypeName(FunctionInput_Texture2DArray),     FString(TEXT("Texture2DArray")));
    TestEqual(TEXT("FunctionInput_VolumeTexture"),      McpFunctionInputTypeName(FunctionInput_VolumeTexture),      FString(TEXT("VolumeTexture")));
    TestEqual(TEXT("FunctionInput_StaticBool"),         McpFunctionInputTypeName(FunctionInput_StaticBool),         FString(TEXT("StaticBool")));
    TestEqual(TEXT("FunctionInput_MaterialAttributes"), McpFunctionInputTypeName(FunctionInput_MaterialAttributes), FString(TEXT("MaterialAttributes")));
    TestEqual(TEXT("FunctionInput_TextureExternal"),    McpFunctionInputTypeName(FunctionInput_TextureExternal),    FString(TEXT("TextureExternal")));
    TestEqual(TEXT("FunctionInput_Bool"),               McpFunctionInputTypeName(FunctionInput_Bool),               FString(TEXT("Bool")));
    TestEqual(TEXT("FunctionInput_Substrate"),          McpFunctionInputTypeName(FunctionInput_Substrate),          FString(TEXT("Substrate")));
    TestEqual(TEXT("default returns Unknown"),
              McpFunctionInputTypeName(static_cast<EFunctionInputType>(0xFF)),
              FString(TEXT("Unknown")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpSubobjectPathResolveTest,
    "LHGame.Mcp.Material.SubobjectPathResolve",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpSubobjectPathResolveTest::RunTest(const FString& Parameters)
{
    UMaterialFunction* MF = NewObject<UMaterialFunction>(GetTransientPackage(),
                                                          TEXT("Fixture_Subobj"), RF_Transient);
    UMaterialExpressionScalarParameter* P = NewObject<UMaterialExpressionScalarParameter>(MF);
    P->ParameterName = TEXT("X");
    MF->GetExpressionCollection().AddExpression(P);

    const FString SubPath = MF->GetPathName() + TEXT(":") + P->GetName();
    FString Resolved;
    UObject* Found = McpHandlerUtils::ResolveObjectFromPath(SubPath, &Resolved);

    TestNotNull(TEXT("resolved subobject"), Found);
    TestEqual  (TEXT("resolved is the parameter"), Found, (UObject*)P);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
