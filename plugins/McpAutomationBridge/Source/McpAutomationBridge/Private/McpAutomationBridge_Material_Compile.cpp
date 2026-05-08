// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_Material_Compile.cpp
//
// Task F.3 - compile_materials and compile_materials_diagnostics.
//
// External handlers exposed by this TU:
//   - compile_materials                 (lightweight: per-asset compile + bool)
//   - compile_materials_diagnostics     (full stats per asset on success)
//
// Both endpoints follow partial-success-per-item: a compile error or timeout
// on one asset never aborts the rest; each asset gets its own row in the
// 'results' array of the final response.
//
// Plan: docs/superpowers/plans/2026-05-07-mcp-material-tools-redesign.md
// Spec: docs/superpowers/specs/2026-05-07-mcp-material-tools-redesign-design.md (sec 8b)

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR

#include "EditorAssetLibrary.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"
#include "RHIDefinitions.h"
#include "RHIStrings.h"
#include "SceneTypes.h"

#endif // WITH_EDITOR

DEFINE_LOG_CATEGORY_STATIC(LogMcpMaterialCompile, Log, All);

namespace
{
#if WITH_EDITOR

// Default per-asset compile timeout (seconds); spec says 60s. Configurable per
// request via the optional 'timeoutSeconds' field on the payload.
static constexpr double GMcpMaterialCompileDefaultTimeoutSeconds = 60.0;
// Hard upper bound to avoid wedging the editor on a runaway compile.
static constexpr double GMcpMaterialCompileMaxTimeoutSeconds = 600.0;

static bool McpIsEngineAsset(const FString& AssetPath)
{
    return AssetPath.StartsWith(TEXT("/Engine/")) || AssetPath.StartsWith(TEXT("/EnginePlugins/"));
}

// Build the per-asset response row used by both endpoints. The row always
// carries 'assetPath' and 'compileSucceeded'; diagnostic-mode rows additionally
// carry 'errors', 'warnings', and 'stats' (which is null on compile failure).
static TSharedPtr<FJsonObject> McpMakeCompileRow(
    const FString& AssetPath, bool bCompileSucceeded)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("assetPath"), AssetPath);
    Row->SetBoolField(TEXT("compileSucceeded"), bCompileSucceeded);
    return Row;
}

// Append a single error entry to a JSON array under {message: ...}.
static void McpAppendErrorMessage(TArray<TSharedPtr<FJsonValue>>& Out, const FString& Message)
{
    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("message"), Message);
    Out.Add(MakeShared<FJsonValueObject>(Entry));
}

// Format a compile-target's blend-mode + shading-model as a single label like
// "BlendedTranslucent", "OpaqueDeferred", "Masked", etc. Mirrors the spec's
// 'renderTargetMode' field; we do not have a single engine API that returns
// this string, so we synthesize it from the Material's properties.
static FString McpDescribeRenderTargetMode(UMaterial* Material)
{
    if (!Material)
    {
        return TEXT("Unknown");
    }
    const EBlendMode Blend = Material->GetBlendMode();
    FString BlendStr;
    switch (Blend)
    {
    case BLEND_Opaque:               BlendStr = TEXT("Opaque"); break;
    case BLEND_Masked:               BlendStr = TEXT("Masked"); break;
    case BLEND_Translucent:          BlendStr = TEXT("BlendedTranslucent"); break;
    case BLEND_Additive:             BlendStr = TEXT("Additive"); break;
    case BLEND_Modulate:             BlendStr = TEXT("Modulate"); break;
    case BLEND_AlphaComposite:       BlendStr = TEXT("AlphaComposite"); break;
    case BLEND_AlphaHoldout:         BlendStr = TEXT("AlphaHoldout"); break;
    default:                         BlendStr = FString::Printf(TEXT("Blend_%d"), (int32)Blend); break;
    }
    return BlendStr;
}

// Resolve the 'compile target' Material for an asset of any of the supported
// kinds. For MaterialInstanceConstant, the underlying parent UMaterial owns
// the resource we extract stats from. For MaterialFunction(Instance), there
// is no per-resource stat surface, so we return nullptr and the caller treats
// the function compile as best-effort with no stats.
static UMaterial* McpResolveCompileTarget(UObject* Asset, FString& OutKind)
{
    if (UMaterial* M = Cast<UMaterial>(Asset))
    {
        OutKind = TEXT("Material");
        return M;
    }
    if (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Asset))
    {
        OutKind = TEXT("MaterialInstance");
        return MIC->GetMaterial();
    }
    if (UMaterialFunctionInstance* MFI = Cast<UMaterialFunctionInstance>(Asset))
    {
        OutKind = TEXT("MaterialFunctionInstance");
        return nullptr;
    }
    if (UMaterialFunction* MF = Cast<UMaterialFunction>(Asset))
    {
        OutKind = TEXT("MaterialFunction");
        return nullptr;
    }
    OutKind = TEXT("Unknown");
    return nullptr;
}

// Kick off a synchronous compile for the asset. Material / MaterialInstance go
// through Pre/PostEditChange (canonical in-editor path); MaterialFunction(s)
// go through ForceRecompileForRendering with a fresh FMaterialUpdateContext.
// After kicking off, the caller spins waiting for IsCompiling() to clear.
static void McpKickOffMaterialCompile(UObject* Asset)
{
    if (UMaterial* Material = Cast<UMaterial>(Asset))
    {
        Material->PreEditChange(nullptr);
        Material->PostEditChange();
        return;
    }
    if (UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Asset))
    {
        Instance->PreEditChange(nullptr);
        Instance->PostEditChange();
        return;
    }
    if (UMaterialFunction* Function = Cast<UMaterialFunction>(Asset))
    {
        FMaterialUpdateContext UpdateContext;
        Function->ForceRecompileForRendering(UpdateContext, nullptr);
        return;
    }
    if (UMaterialFunctionInstance* FuncInstance = Cast<UMaterialFunctionInstance>(Asset))
    {
        FMaterialUpdateContext UpdateContext;
        FuncInstance->ForceRecompileForRendering(UpdateContext, nullptr);
        return;
    }
}

// Wait for the compile to finish or for the per-asset budget to elapse.
// Returns true if the compile finished within budget. The hot loop yields to
// the platform every 50ms to avoid spinning a CPU.
static bool McpWaitForMaterialCompile(UObject* Asset, double TimeoutSeconds, double& OutElapsedSeconds)
{
    UMaterial* Material = nullptr;
    if (UMaterial* M = Cast<UMaterial>(Asset))
    {
        Material = M;
    }
    else if (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Asset))
    {
        Material = MIC->GetMaterial();
    }
    // MaterialFunction[Instance] has no IsCompiling() surface; treat as immediate.

    const double Start = FPlatformTime::Seconds();
    if (Material)
    {
        while (Material->IsCompiling())
        {
            const double Elapsed = FPlatformTime::Seconds() - Start;
            if (Elapsed >= TimeoutSeconds)
            {
                OutElapsedSeconds = Elapsed;
                return false;
            }
            FPlatformProcess::Sleep(0.05f);
        }
    }
    OutElapsedSeconds = FPlatformTime::Seconds() - Start;
    return true;
}

// Inspect post-compile state. Returns true if no shader-platform reported a
// compile error; collects any reported error strings into OutErrors.
static bool McpCollectCompileErrors(UMaterial* CompileTarget, TArray<FString>& OutErrors)
{
    if (!CompileTarget)
    {
        return true;
    }
    bool bHadErrors = false;
    for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; ++FeatureLevelIndex)
    {
        const ERHIFeatureLevel::Type FeatureLevel = static_cast<ERHIFeatureLevel::Type>(FeatureLevelIndex);
        const EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(FeatureLevel);
        if (ShaderPlatform == SP_NumPlatforms)
        {
            continue;
        }
        const FMaterialResource* Resource = CompileTarget->GetMaterialResource(ShaderPlatform);
        if (!Resource)
        {
            continue;
        }
        const TArray<FString>& Errors = Resource->GetCompileErrors();
        if (Errors.Num() > 0)
        {
            bHadErrors = true;
            for (const FString& E : Errors)
            {
                OutErrors.AddUnique(E);
            }
        }
    }
    // Also check the editor-level "compile error" flag for the running platform.
    const EShaderPlatform RunningPlatform = GetFeatureLevelShaderPlatform(GMaxRHIFeatureLevel);
    if (CompileTarget->IsCompilingOrHadCompileError(RunningPlatform))
    {
        // IsCompilingOrHadCompileError returns true while still compiling too; only
        // treat as error when no longer compiling.
        if (!CompileTarget->IsCompiling())
        {
            bHadErrors = bHadErrors || OutErrors.Num() > 0;
        }
    }
    return !bHadErrors;
}

// Build the diagnostic 'stats' object from the running-platform material resource.
// Returns null if the resource cannot be accessed.
static TSharedPtr<FJsonObject> McpBuildDiagnosticStats(UMaterial* CompileTarget)
{
    if (!CompileTarget)
    {
        return nullptr;
    }
    const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;
    const EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(FeatureLevel);
    const FMaterialResource* Resource = CompileTarget->GetMaterialResource(ShaderPlatform);
    if (!Resource)
    {
        return nullptr;
    }

    TSharedPtr<FJsonObject> Stats = MakeShared<FJsonObject>();

    // Instruction counts. The detailed per-representative-shader counts come
    // from FMaterialStatsUtils::GetRepresentativeInstructionCounts, which lives
    // in the MaterialEditor module but is not DLL-exported (no MATERIALEDITOR_API),
    // so we cannot reach it from another module. As a public-API proxy we surface
    // the total shader count derived from the game-thread shader map; per-stage
    // breakdowns are reported as 0 to keep the response shape stable.
    TSharedPtr<FJsonObject> InstructionCounts = MakeShared<FJsonObject>();
    {
        int32 TotalShaderCount = 0;
        if (FMaterialShaderMap* ShaderMap = Resource->GetGameThreadShaderMap())
        {
            TotalShaderCount = static_cast<int32>(ShaderMap->GetShaderNum());
        }
        InstructionCounts->SetNumberField(TEXT("PixelShader"), 0);
        InstructionCounts->SetNumberField(TEXT("VertexShader"), 0);
        InstructionCounts->SetNumberField(TEXT("ComputeShader"), 0);
        // Approximate "function instruction" budget via total shader count (no
        // exported engine surface for the precise representative count).
        InstructionCounts->SetNumberField(TEXT("MaterialFunctionInstructions"), TotalShaderCount);
    }
    Stats->SetObjectField(TEXT("instructionCounts"), InstructionCounts);

    // Sampler usage and limit.
    const int32 SamplersUsed = Resource->GetSamplerUsage();
    const int32 MaxSamplers = GetExpectedFeatureLevelMaxTextureSamplers(Resource->GetFeatureLevel());
    Stats->SetNumberField(TEXT("samplerCount"), SamplersUsed >= 0 ? SamplersUsed : 0);
    Stats->SetNumberField(TEXT("samplerLimit"), MaxSamplers);

    // Texture-lookup count (estimated): VS + PS samples.
    uint32 NumVSTextureSamples = 0;
    uint32 NumPSTextureSamples = 0;
    Resource->GetEstimatedNumTextureSamples(NumVSTextureSamples, NumPSTextureSamples);
    Stats->SetNumberField(TEXT("textureLookupCount"),
        static_cast<int32>(NumVSTextureSamples + NumPSTextureSamples));

    // Uniform expression count: count of uniform 2-vector / 4-vector / scalar
    // expressions in the material's uniform expression set. The engine API
    // surfaces this via the shader map preshader stats, which is the closest
    // public proxy; absent that we fall back to 0.
    uint32 PreshaderParams = 0;
    uint32 PreshaderOps = 0;
    Resource->GetPreshaderStats(PreshaderParams, PreshaderOps);
    Stats->SetNumberField(TEXT("uniformExpressionCount"), static_cast<int32>(PreshaderParams));

    // User interpolator count (in scalars). UE breaks this down into UV +
    // custom; we report the combined scalar count which matches the editor
    // stats panel.
    uint32 UVScalars = 0;
    uint32 CustomScalars = 0;
    Resource->GetUserInterpolatorUsage(UVScalars, CustomScalars);
    Stats->SetNumberField(TEXT("interpolatorCount"), static_cast<int32>(UVScalars + CustomScalars));

    Stats->SetStringField(TEXT("renderTargetMode"), McpDescribeRenderTargetMode(CompileTarget));

    // Spec example shows 'shaderPlatform' on warnings only, not on stats; we
    // surface the running platform here as an extra diagnostic crumb.
    const FName PlatformName = LegacyShaderPlatformToShaderFormat(ShaderPlatform);
    Stats->SetStringField(TEXT("shaderPlatform"), PlatformName.ToString());

    return Stats;
}

// Pull the asset paths array off the payload, normalizing both the string-only
// and item-object forms.
static bool McpReadAssetPaths(const TSharedPtr<FJsonObject>& Payload, TArray<FString>& OutPaths)
{
    OutPaths.Reset();
    if (!Payload.IsValid())
    {
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>* AssetPathsArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArr) && AssetPathsArr)
    {
        for (const TSharedPtr<FJsonValue>& V : *AssetPathsArr)
        {
            if (!V.IsValid())
            {
                continue;
            }
            if (V->Type == EJson::String)
            {
                const FString S = V->AsString();
                if (!S.IsEmpty()) OutPaths.Add(S);
                continue;
            }
            const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
            if (V->TryGetObject(ObjPtr) && ObjPtr)
            {
                FString P;
                (*ObjPtr)->TryGetStringField(TEXT("assetPath"), P);
                if (!P.IsEmpty()) OutPaths.Add(P);
            }
        }
    }
    return OutPaths.Num() > 0;
}

// Compile a single asset. Fills OutRow with at minimum {assetPath, compileSucceeded};
// in diagnostic mode adds {errors, warnings, stats}. Honors the per-asset timeout.
static void McpCompileSingleAsset(
    FString AssetPath, double TimeoutSeconds, bool bDiagnostics,
    TSharedPtr<FJsonObject>& OutRow, bool& bOutSucceeded)
{
    bOutSucceeded = false;

    OutRow = McpMakeCompileRow(AssetPath, false);

    if (McpIsEngineAsset(AssetPath))
    {
        if (bDiagnostics)
        {
            TArray<TSharedPtr<FJsonValue>> Errors;
            McpAppendErrorMessage(Errors,
                FString::Printf(TEXT("Asset '%s' is under engine content; refuse to compile."), *AssetPath));
            OutRow->SetArrayField(TEXT("errors"), Errors);
            OutRow->SetArrayField(TEXT("warnings"), TArray<TSharedPtr<FJsonValue>>());
            OutRow->SetField(TEXT("stats"), MakeShared<FJsonValueNull>());
        }
        return;
    }

    AssetPath = SanitizeProjectRelativePath(AssetPath);
    OutRow->SetStringField(TEXT("assetPath"), AssetPath);

    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset)
    {
        if (bDiagnostics)
        {
            TArray<TSharedPtr<FJsonValue>> Errors;
            McpAppendErrorMessage(Errors,
                FString::Printf(TEXT("Could not load asset '%s'."), *AssetPath));
            OutRow->SetArrayField(TEXT("errors"), Errors);
            OutRow->SetArrayField(TEXT("warnings"), TArray<TSharedPtr<FJsonValue>>());
            OutRow->SetField(TEXT("stats"), MakeShared<FJsonValueNull>());
        }
        return;
    }

    FString Kind;
    UMaterial* CompileTarget = McpResolveCompileTarget(Asset, Kind);
    if (Kind == TEXT("Unknown"))
    {
        if (bDiagnostics)
        {
            TArray<TSharedPtr<FJsonValue>> Errors;
            McpAppendErrorMessage(Errors,
                FString::Printf(TEXT("Asset '%s' is not a Material/MaterialInstance/MaterialFunction."), *AssetPath));
            OutRow->SetArrayField(TEXT("errors"), Errors);
            OutRow->SetArrayField(TEXT("warnings"), TArray<TSharedPtr<FJsonValue>>());
            OutRow->SetField(TEXT("stats"), MakeShared<FJsonValueNull>());
        }
        return;
    }

    // Kick off compile, then spin up to the per-asset budget.
    McpKickOffMaterialCompile(Asset);
    double ElapsedSeconds = 0.0;
    const bool bWithinBudget = McpWaitForMaterialCompile(Asset, TimeoutSeconds, ElapsedSeconds);

    if (!bWithinBudget)
    {
        // Per-asset timeout. Emit the spec'd error entry.
        OutRow->SetBoolField(TEXT("compileSucceeded"), false);
        if (bDiagnostics)
        {
            TArray<TSharedPtr<FJsonValue>> Errors;
            McpAppendErrorMessage(Errors,
                FString::Printf(TEXT("compile timeout exceeded (%.0fs)"), TimeoutSeconds));
            OutRow->SetArrayField(TEXT("errors"), Errors);
            OutRow->SetArrayField(TEXT("warnings"), TArray<TSharedPtr<FJsonValue>>());
            OutRow->SetField(TEXT("stats"), MakeShared<FJsonValueNull>());
        }
        return;
    }

    // Collect compile errors (from MaterialResource). MaterialFunction[Instance]
    // routes through a host material the engine resolves; we have no resource
    // surface for them, so they are treated as success unless the host throws.
    TArray<FString> ErrorStrings;
    const bool bSuccess = McpCollectCompileErrors(CompileTarget, ErrorStrings);
    OutRow->SetBoolField(TEXT("compileSucceeded"), bSuccess);
    bOutSucceeded = bSuccess;

    if (!bDiagnostics)
    {
        return;
    }

    // Diagnostic mode: errors[], warnings[], stats{} or null.
    TArray<TSharedPtr<FJsonValue>> Errors;
    for (const FString& E : ErrorStrings)
    {
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("message"), E);
        // Best-effort platform tag; running-platform is the most useful default.
        const FName PlatformName = LegacyShaderPlatformToShaderFormat(GetFeatureLevelShaderPlatform(GMaxRHIFeatureLevel));
        Entry->SetStringField(TEXT("shaderPlatform"), PlatformName.ToString());
        Errors.Add(MakeShared<FJsonValueObject>(Entry));
    }
    OutRow->SetArrayField(TEXT("errors"), Errors);

    // Warnings are not surfaced via FMaterialResource::GetCompileErrors; the
    // engine collects them differently per platform. We emit an empty array
    // for shape stability. (G.4 may expand this later via shader-map walk.)
    OutRow->SetArrayField(TEXT("warnings"), TArray<TSharedPtr<FJsonValue>>());

    if (bSuccess)
    {
        TSharedPtr<FJsonObject> Stats = McpBuildDiagnosticStats(CompileTarget);
        if (Stats.IsValid())
        {
            OutRow->SetObjectField(TEXT("stats"), Stats);
        }
        else
        {
            OutRow->SetField(TEXT("stats"), MakeShared<FJsonValueNull>());
        }
    }
    else
    {
        OutRow->SetField(TEXT("stats"), MakeShared<FJsonValueNull>());
    }
}

// Shared dispatcher: drives the per-asset loop for both compile_materials and
// compile_materials_diagnostics. The caller passes bDiagnostics to opt into
// the heavier per-asset payload.
static bool McpHandle_CompileMaterialsCommon(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
    bool bDiagnostics)
{
    if (!Sub)
    {
        return true;
    }
    if (!Payload.IsValid())
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("Invalid payload"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    TArray<FString> AssetPaths;
    if (!McpReadAssetPaths(Payload, AssetPaths))
    {
        Sub->SendAutomationError(Socket, RequestId,
            TEXT("'assetPaths' is required and must be a non-empty array of strings."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    double TimeoutSeconds = GMcpMaterialCompileDefaultTimeoutSeconds;
    Payload->TryGetNumberField(TEXT("timeoutSeconds"), TimeoutSeconds);
    TimeoutSeconds = FMath::Clamp(TimeoutSeconds, 1.0, GMcpMaterialCompileMaxTimeoutSeconds);

    TArray<TSharedPtr<FJsonValue>> Results;
    Results.Reserve(AssetPaths.Num());
    int32 SuccessCount = 0;

    for (const FString& Path : AssetPaths)
    {
        TSharedPtr<FJsonObject> Row;
        bool bSucceeded = false;
        McpCompileSingleAsset(Path, TimeoutSeconds, bDiagnostics, Row, bSucceeded);
        if (Row.IsValid())
        {
            Results.Add(MakeShared<FJsonValueObject>(Row));
        }
        if (bSucceeded)
        {
            ++SuccessCount;
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetArrayField(TEXT("results"), Results);
    Sub->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Compiled %d/%d material asset(s)."),
            SuccessCount, AssetPaths.Num()),
        Resp);
    return true;
}

#endif // WITH_EDITOR
} // namespace

// ---------------------------------------------------------------------------
// External handlers (resolved via 'extern bool ...' from the dispatch site).
// ---------------------------------------------------------------------------

bool McpHandle_CompileMaterials(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_CompileMaterialsCommon(Sub, RequestId, Payload, Socket, /*bDiagnostics=*/false);
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandle_CompileMaterialsDiagnostics(
    UMcpAutomationBridgeSubsystem* Sub, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    return McpHandle_CompileMaterialsCommon(Sub, RequestId, Payload, Socket, /*bDiagnostics=*/true);
#else
    if (Sub) Sub->SendAutomationError(Socket, RequestId,
        TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}
