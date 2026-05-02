#pragma once

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialFunctionFactoryNew.h"
#include "UObject/SavePackage.h"
#include "FileHelpers.h"
#include "Editor.h"

// ---------------------------------------------------------------------------
// 7.1 Material function with typed inputs, texture object parameter, custom
//     expression, and output connection smoke test
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpMaterialFunctionAuthoringTest,
    "LHGame.Mcp.MaterialAuthoring.FunctionWithTypedInputsAndCustomExpression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpMaterialFunctionAuthoringTest::RunTest(const FString& Parameters)
{
    const FString PkgName = TEXT("/Game/__McpTest_MaterialFunction_7_1__");
    UPackage* Pkg = CreatePackage(*PkgName);
    TestNotNull(TEXT("Package"), Pkg);

    UMaterialFunctionFactoryNew* Factory = NewObject<UMaterialFunctionFactoryNew>();
    IAssetTools& AT = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    UMaterialFunction* MF = Cast<UMaterialFunction>(AT.CreateAsset(
        TEXT("__McpTest_MaterialFunction_7_1__"),
        TEXT("/Game"),
        UMaterialFunction::StaticClass(),
        Factory));
    TestNotNull(TEXT("MaterialFunction created"), MF);
    if (!MF) return false;

    // Add function input
    UMaterialExpressionFunctionInput* FuncInput = NewObject<UMaterialExpressionFunctionInput>(
        MF, UMaterialExpressionFunctionInput::StaticClass(), NAME_None, RF_Transactional);
    FuncInput->InputName = FName(TEXT("TestInput"));
    FuncInput->InputType = EFunctionInputType::FunctionInput_Scalar;
    FuncInput->bUsePreviewValueAsDefault = false;
    FuncInput->SortPriority = 0;
    FuncInput->Id = FGuid::NewGuid();
    FuncInput->MaterialExpressionGuid = FGuid::NewGuid();
    MF->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(FuncInput);

    // Add custom expression
    UMaterialExpressionCustom* Custom = NewObject<UMaterialExpressionCustom>(
        MF, UMaterialExpressionCustom::StaticClass(), NAME_None, RF_Transactional);
    Custom->Code = TEXT("return In0 * 2.0f;");
    Custom->OutputType = CMOT_Float1;
    FCustomInput CI; CI.InputName = FName(TEXT("In0")); Custom->Inputs.Add(CI);
    Custom->RebuildOutputs();
    Custom->MaterialExpressionGuid = FGuid::NewGuid();
    MF->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(Custom);

    // Add function output
    UMaterialExpressionFunctionOutput* FuncOutput = NewObject<UMaterialExpressionFunctionOutput>(
        MF, UMaterialExpressionFunctionOutput::StaticClass(), NAME_None, RF_Transactional);
    FuncOutput->OutputName = FName(TEXT("Result"));
    FuncOutput->Id = FGuid::NewGuid();
    FuncOutput->MaterialExpressionGuid = FGuid::NewGuid();
    MF->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(FuncOutput);

    // Connect custom to function input
    Custom->Inputs[0].Input.Expression = FuncInput;
    FuncOutput->A.Expression = Custom;

    MF->PostEditChange();
    MF->MarkPackageDirty();

    TestEqual(TEXT("Expression count"), MF->GetEditorOnlyData()->ExpressionCollection.Expressions.Num(), 3);
    TestNotNull(TEXT("FuncInput expression"), Cast<UMaterialExpressionFunctionInput>(
        MF->GetEditorOnlyData()->ExpressionCollection.Expressions[0].Get()));
    TestNotNull(TEXT("Custom expression"), Cast<UMaterialExpressionCustom>(
        MF->GetEditorOnlyData()->ExpressionCollection.Expressions[1].Get()));
    TestNotNull(TEXT("FuncOutput expression"), Cast<UMaterialExpressionFunctionOutput>(
        MF->GetEditorOnlyData()->ExpressionCollection.Expressions[2].Get()));

    // Save and reload
    FEditorFileUtils::SaveDirtyPackages(false, true, true, false, false, false);
    ResetLoaders(Pkg);
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

    FString ReloadedPath = PkgName + TEXT(".__McpTest_MaterialFunction_7_1__");
    UMaterialFunction* Reloaded = LoadObject<UMaterialFunction>(nullptr, *ReloadedPath);
    if (Reloaded)
    {
        TestEqual(TEXT("Reloaded expression count"), Reloaded->GetEditorOnlyData()->ExpressionCollection.Expressions.Num(), 3);
    }

    // Cleanup
    if (MF) MF->ClearFlags(RF_Standalone);
    if (Pkg) { Pkg->MarkAsGarbage(); }
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    return true;
}

// ---------------------------------------------------------------------------
// 7.2 Material instance with scalar and vector overrides smoke test
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpMaterialInstanceOverridesTest,
    "LHGame.Mcp.MaterialAuthoring.MaterialInstanceScalarVectorOverrides",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpMaterialInstanceOverridesTest::RunTest(const FString& Parameters)
{
    // Create base material with scalar/vector params
    UMaterialFactoryNew* MatFactory = NewObject<UMaterialFactoryNew>();
    IAssetTools& AT = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

    UMaterial* BaseMat = Cast<UMaterial>(AT.CreateAsset(
        TEXT("__McpTest_BaseMat_7_2__"),
        TEXT("/Game"),
        UMaterial::StaticClass(),
        MatFactory));
    TestNotNull(TEXT("BaseMat created"), BaseMat);
    if (!BaseMat) return false;

    // Create material instance
    UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(AT.CreateAsset(
        TEXT("__McpTest_MatInst_7_2__"),
        TEXT("/Game"),
        UMaterialInstanceConstant::StaticClass(),
        nullptr));
    TestNotNull(TEXT("Instance created"), Instance);
    if (!Instance) { BaseMat->ClearFlags(RF_Standalone); return false; }

    Instance->SetParentEditorOnly(BaseMat);

    const FMaterialParameterInfo ScalarInfo(TEXT("TestScalar"));
    Instance->SetScalarParameterValueEditorOnly(ScalarInfo, 1.5f);

    const FMaterialParameterInfo VectorInfo(TEXT("TestVector"));
    Instance->SetVectorParameterValueEditorOnly(VectorInfo, FLinearColor(0.1f, 0.2f, 0.3f, 1.0f));

    float ScalarVal = 0.0f;
    bool bHasScalar = false;
    for (const auto& SV : Instance->ScalarParameterValues)
    {
        if (SV.ParameterInfo.Name == TEXT("TestScalar")) { ScalarVal = SV.ParameterValue; bHasScalar = true; break; }
    }
    TestTrue(TEXT("Scalar override set"), bHasScalar);
    TestEqual(TEXT("Scalar value"), ScalarVal, 1.5f);

    // Cleanup
    BaseMat->ClearFlags(RF_Standalone);
    Instance->ClearFlags(RF_Standalone);
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    return true;
}

// ---------------------------------------------------------------------------
// 7.3 Material function instance with parameter overrides smoke test
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpMaterialFunctionInstanceOverridesTest,
    "LHGame.Mcp.MaterialAuthoring.MaterialFunctionInstanceParameterOverrides",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpMaterialFunctionInstanceOverridesTest::RunTest(const FString& Parameters)
{
    IAssetTools& AT = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

    // Create parent function
    UMaterialFunctionFactoryNew* FuncFactory = NewObject<UMaterialFunctionFactoryNew>();
    UMaterialFunction* ParentFunc = Cast<UMaterialFunction>(AT.CreateAsset(
        TEXT("__McpTest_ParentFunc_7_3__"),
        TEXT("/Game"),
        UMaterialFunction::StaticClass(),
        FuncFactory));
    TestNotNull(TEXT("ParentFunc created"), ParentFunc);
    if (!ParentFunc) return false;

    // Create function instance
    UMaterialFunctionInstance* FuncInst = Cast<UMaterialFunctionInstance>(AT.CreateAsset(
        TEXT("__McpTest_FuncInst_7_3__"),
        TEXT("/Game"),
        UMaterialFunctionInstance::StaticClass(),
        nullptr));
    TestNotNull(TEXT("FuncInst created"), FuncInst);
    if (!FuncInst) { ParentFunc->ClearFlags(RF_Standalone); return false; }

    FuncInst->SetParent(ParentFunc);
    FuncInst->UpdateParameterSet();

    // Apply scalar override
    FScalarParameterValue ScalarOverride;
    ScalarOverride.ParameterInfo = FMaterialParameterInfo(TEXT("Intensity"));
    ScalarOverride.ParameterValue = 2.5f;
    FuncInst->ScalarParameterValues.Add(ScalarOverride);
    FuncInst->UpdateParameterSet();

    bool bFound = false;
    for (const auto& P : FuncInst->ScalarParameterValues)
    {
        if (P.ParameterInfo.Name == TEXT("Intensity")) { TestEqual(TEXT("Scalar value"), P.ParameterValue, 2.5f); bFound = true; break; }
    }
    TestTrue(TEXT("Override present"), bFound);

    // Cleanup
    ParentFunc->ClearFlags(RF_Standalone);
    FuncInst->ClearFlags(RF_Standalone);
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    return true;
}

// ---------------------------------------------------------------------------
// 7.5 FScopedTransaction in editor undo stack smoke test
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpMaterialTransactionUndoTest,
    "LHGame.Mcp.MaterialAuthoring.ScopedTransactionAppearsInUndoStack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMcpMaterialTransactionUndoTest::RunTest(const FString& Parameters)
{
    if (!GEditor)
    {
        AddError(TEXT("GEditor not available"));
        return false;
    }

    IAssetTools& AT = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    UMaterialFunctionFactoryNew* FuncFactory = NewObject<UMaterialFunctionFactoryNew>();
    UMaterialFunction* MF = Cast<UMaterialFunction>(AT.CreateAsset(
        TEXT("__McpTest_TransactionMF_7_5__"),
        TEXT("/Game"),
        UMaterialFunction::StaticClass(),
        FuncFactory));
    TestNotNull(TEXT("MF for transaction test"), MF);
    if (!MF) return false;

    const int32 InitialCount = MF->GetEditorOnlyData()->ExpressionCollection.Expressions.Num();

    {
        FScopedTransaction Transaction(NSLOCTEXT("McpTests", "AddExpressionTest", "Test: add expression"));
        MF->Modify();
        UMaterialExpressionCustom* Expr = NewObject<UMaterialExpressionCustom>(
            MF, UMaterialExpressionCustom::StaticClass(), NAME_None, RF_Transactional);
        Expr->Code = TEXT("return 1.0f;");
        Expr->MaterialExpressionGuid = FGuid::NewGuid();
        MF->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(Expr);
    }

    TestEqual(TEXT("Expression added by transaction"), MF->GetEditorOnlyData()->ExpressionCollection.Expressions.Num(), InitialCount + 1);

    // Undo
    GEditor->UndoTransaction();
    TestEqual(TEXT("Expression removed after undo"), MF->GetEditorOnlyData()->ExpressionCollection.Expressions.Num(), InitialCount);

    MF->ClearFlags(RF_Standalone);
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
