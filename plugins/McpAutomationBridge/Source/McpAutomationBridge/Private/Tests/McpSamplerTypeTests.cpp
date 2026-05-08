// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Tests/McpSamplerTypeTests.cpp
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "../McpAutomationBridgeHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpParseSamplerTypeStringTest,
    "LHGame.Mcp.Material.SamplerType.Parse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpParseSamplerTypeStringTest::RunTest(const FString& Parameters)
{
    bool bRecognized = false;
    EMaterialSamplerType T;

    // All 17 canonical strings must parse and report recognized=true
    T = McpParseSamplerTypeString(TEXT("Color"),                  &bRecognized);
    TestTrue (TEXT("Color recognized"),                    bRecognized);
    TestEqual(TEXT("Color"),                               (int32)T, (int32)SAMPLERTYPE_Color);

    T = McpParseSamplerTypeString(TEXT("LinearColor"),            &bRecognized);
    TestTrue (TEXT("LinearColor recognized"),              bRecognized);
    TestEqual(TEXT("LinearColor"),                         (int32)T, (int32)SAMPLERTYPE_LinearColor);

    T = McpParseSamplerTypeString(TEXT("Normal"),                 &bRecognized);
    TestTrue (TEXT("Normal recognized"),                   bRecognized);
    TestEqual(TEXT("Normal"),                              (int32)T, (int32)SAMPLERTYPE_Normal);

    T = McpParseSamplerTypeString(TEXT("Masks"),                  &bRecognized);
    TestTrue (TEXT("Masks recognized"),                    bRecognized);
    TestEqual(TEXT("Masks"),                               (int32)T, (int32)SAMPLERTYPE_Masks);

    T = McpParseSamplerTypeString(TEXT("Alpha"),                  &bRecognized);
    TestTrue (TEXT("Alpha recognized"),                    bRecognized);
    TestEqual(TEXT("Alpha"),                               (int32)T, (int32)SAMPLERTYPE_Alpha);

    T = McpParseSamplerTypeString(TEXT("Grayscale"),              &bRecognized);
    TestTrue (TEXT("Grayscale recognized"),                bRecognized);
    TestEqual(TEXT("Grayscale"),                           (int32)T, (int32)SAMPLERTYPE_Grayscale);

    T = McpParseSamplerTypeString(TEXT("LinearGrayscale"),        &bRecognized);
    TestTrue (TEXT("LinearGrayscale recognized"),          bRecognized);
    TestEqual(TEXT("LinearGrayscale"),                     (int32)T, (int32)SAMPLERTYPE_LinearGrayscale);

    T = McpParseSamplerTypeString(TEXT("DistanceFieldFont"),      &bRecognized);
    TestTrue (TEXT("DistanceFieldFont recognized"),        bRecognized);
    TestEqual(TEXT("DistanceFieldFont"),                   (int32)T, (int32)SAMPLERTYPE_DistanceFieldFont);

    T = McpParseSamplerTypeString(TEXT("External"),               &bRecognized);
    TestTrue (TEXT("External recognized"),                 bRecognized);
    TestEqual(TEXT("External"),                            (int32)T, (int32)SAMPLERTYPE_External);

    T = McpParseSamplerTypeString(TEXT("Data"),                   &bRecognized);
    TestTrue (TEXT("Data recognized"),                     bRecognized);
    TestEqual(TEXT("Data"),                                (int32)T, (int32)SAMPLERTYPE_Data);

    T = McpParseSamplerTypeString(TEXT("VirtualColor"),           &bRecognized);
    TestTrue (TEXT("VirtualColor recognized"),             bRecognized);
    TestEqual(TEXT("VirtualColor"),                        (int32)T, (int32)SAMPLERTYPE_VirtualColor);

    T = McpParseSamplerTypeString(TEXT("VirtualLinearColor"),     &bRecognized);
    TestTrue (TEXT("VirtualLinearColor recognized"),       bRecognized);
    TestEqual(TEXT("VirtualLinearColor"),                  (int32)T, (int32)SAMPLERTYPE_VirtualLinearColor);

    T = McpParseSamplerTypeString(TEXT("VirtualGrayscale"),       &bRecognized);
    TestTrue (TEXT("VirtualGrayscale recognized"),         bRecognized);
    TestEqual(TEXT("VirtualGrayscale"),                    (int32)T, (int32)SAMPLERTYPE_VirtualGrayscale);

    T = McpParseSamplerTypeString(TEXT("VirtualLinearGrayscale"), &bRecognized);
    TestTrue (TEXT("VirtualLinearGrayscale recognized"),   bRecognized);
    TestEqual(TEXT("VirtualLinearGrayscale"),              (int32)T, (int32)SAMPLERTYPE_VirtualLinearGrayscale);

    T = McpParseSamplerTypeString(TEXT("VirtualNormal"),          &bRecognized);
    TestTrue (TEXT("VirtualNormal recognized"),            bRecognized);
    TestEqual(TEXT("VirtualNormal"),                       (int32)T, (int32)SAMPLERTYPE_VirtualNormal);

    T = McpParseSamplerTypeString(TEXT("VirtualMasks"),           &bRecognized);
    TestTrue (TEXT("VirtualMasks recognized"),             bRecognized);
    TestEqual(TEXT("VirtualMasks"),                        (int32)T, (int32)SAMPLERTYPE_VirtualMasks);

    T = McpParseSamplerTypeString(TEXT("VirtualAlpha"),           &bRecognized);
    TestTrue (TEXT("VirtualAlpha recognized"),             bRecognized);
    TestEqual(TEXT("VirtualAlpha"),                        (int32)T, (int32)SAMPLERTYPE_VirtualAlpha);

    // Empty input must be unrecognized and fall back to Color
    bRecognized = true;
    T = McpParseSamplerTypeString(TEXT(""), &bRecognized);
    TestFalse(TEXT("empty unrecognized"),                  bRecognized);
    TestEqual(TEXT("empty falls back to Color"),           (int32)T, (int32)SAMPLERTYPE_Color);

    // Garbage input must be unrecognized and fall back to Color
    bRecognized = true;
    T = McpParseSamplerTypeString(TEXT("VirtualMasksXYZ"), &bRecognized);
    TestFalse(TEXT("garbage unrecognized"),                bRecognized);
    TestEqual(TEXT("garbage falls back to Color"),         (int32)T, (int32)SAMPLERTYPE_Color);

    // 1-arg form (existing call sites) must still compile and work
    T = McpParseSamplerTypeString(TEXT("Color"));
    TestEqual(TEXT("1-arg form returns Color"),            (int32)T, (int32)SAMPLERTYPE_Color);

    T = McpParseSamplerTypeString(TEXT("VirtualNormal"));
    TestEqual(TEXT("1-arg form returns VirtualNormal"),    (int32)T, (int32)SAMPLERTYPE_VirtualNormal);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
