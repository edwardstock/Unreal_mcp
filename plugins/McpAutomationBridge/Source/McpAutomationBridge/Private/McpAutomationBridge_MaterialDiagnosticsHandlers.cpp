#include "McpAutomationBridgeSubsystem.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopedSlowTask.h"
#include "ScopedTransaction.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/UnrealType.h"
#endif

namespace
{
#if WITH_EDITOR

static FString McpDiagnosticsOwnerKindToString(EMcpMaterialGraphOwnerKind Kind)
{
    switch (Kind)
    {
    case EMcpMaterialGraphOwnerKind::Material:
        return TEXT("Material");
    case EMcpMaterialGraphOwnerKind::MaterialFunction:
        return TEXT("MaterialFunction");
    case EMcpMaterialGraphOwnerKind::MaterialFunctionInstance:
        return TEXT("MaterialFunctionInstance");
    default:
        return TEXT("Unknown");
    }
}

static TSharedPtr<FJsonObject> McpDiagnosticsExpressionIdentity(
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpression* Expression,
    int32 ExpressionIndex)
{
    TSharedPtr<FJsonObject> Identity = MakeShared<FJsonObject>();
    Identity->SetNumberField(TEXT("expressionIndex"), ExpressionIndex);
    if (Expression)
    {
        Identity->SetStringField(TEXT("expressionName"), Expression->GetName());
        Identity->SetStringField(TEXT("expressionPath"), Expression->GetPathName());
        Identity->SetStringField(TEXT("expressionGuid"), Expression->MaterialExpressionGuid.ToString());
        Identity->SetStringField(TEXT("class"), Expression->GetClass()->GetName());
    }
    if (Owner.Asset)
    {
        Identity->SetStringField(TEXT("ownerPath"), Owner.Asset->GetPathName());
    }
    return Identity;
}

static int32 McpDiagnosticsExpressionIndex(
    const FMcpMaterialGraphOwner& Owner,
    const UMaterialExpression* Expression)
{
    const TArray<TObjectPtr<UMaterialExpression>>* Expressions = McpGetGraphExpressions(Owner);
    if (!Expressions || !Expression)
    {
        return INDEX_NONE;
    }
    for (int32 Index = 0; Index < Expressions->Num(); ++Index)
    {
        if ((*Expressions)[Index] == Expression)
        {
            return Index;
        }
    }
    return INDEX_NONE;
}

static UMaterialExpression* McpDiagnosticsFindExpression(
    const FMcpMaterialGraphOwner& Owner,
    const TSharedPtr<FJsonObject>& Payload)
{
    const TSharedPtr<FJsonObject>* ExpressionObject = nullptr;
    if (Payload->TryGetObjectField(TEXT("expression"), ExpressionObject) && ExpressionObject && ExpressionObject->IsValid())
    {
        int32 ExpressionIndex = INDEX_NONE;
        if ((*ExpressionObject)->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex))
        {
            return McpFindGraphExpression(Owner, FString(), ExpressionIndex);
        }

        FString ExpressionPath;
        if ((*ExpressionObject)->TryGetStringField(TEXT("expressionPath"), ExpressionPath) && !ExpressionPath.IsEmpty())
        {
            UMaterialExpression* Found = McpFindGraphExpression(Owner, ExpressionPath);
            FString ExpressionGuid;
            if (Found && (*ExpressionObject)->TryGetStringField(TEXT("expressionGuid"), ExpressionGuid) && !ExpressionGuid.IsEmpty())
            {
                FGuid ParsedGuid;
                if (FGuid::Parse(ExpressionGuid, ParsedGuid) && Found->MaterialExpressionGuid != ParsedGuid)
                {
                    return nullptr;
                }
            }
            return Found;
        }

        FString ExpressionName;
        if ((*ExpressionObject)->TryGetStringField(TEXT("expressionName"), ExpressionName) && !ExpressionName.IsEmpty())
        {
            return McpFindGraphExpression(Owner, ExpressionName);
        }
    }

    FString ExpressionRef;
    if (Payload->TryGetStringField(TEXT("expression"), ExpressionRef) && !ExpressionRef.IsEmpty())
    {
        return McpFindGraphExpression(Owner, ExpressionRef);
    }

    int32 ExpressionIndex = INDEX_NONE;
    if (Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex))
    {
        return McpFindGraphExpression(Owner, FString(), ExpressionIndex);
    }

    FString NodeId;
    if (Payload->TryGetStringField(TEXT("nodeId"), NodeId) && !NodeId.IsEmpty())
    {
        return McpFindGraphExpression(Owner, NodeId);
    }

    return nullptr;
}

static TSharedPtr<FJsonObject> McpDiagnosticsObjectSummary(UObject* Object)
{
    TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
    Summary->SetBoolField(TEXT("isNull"), Object == nullptr);
    if (Object)
    {
        Summary->SetStringField(TEXT("name"), Object->GetName());
        Summary->SetStringField(TEXT("path"), Object->GetPathName());
        Summary->SetStringField(TEXT("class"), Object->GetClass()->GetName());
    }
    return Summary;
}

static bool McpDiagnosticsStringArrayContains(const TArray<FString>& Values, const FString& Candidate)
{
    return Values.Num() == 0 || Values.ContainsByPredicate(
        [&Candidate](const FString& Value) { return Value.Equals(Candidate, ESearchCase::IgnoreCase); });
}

static TArray<FString> McpDiagnosticsGetStringArray(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName)
{
    TArray<FString> Result;
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Payload->TryGetArrayField(FieldName, Values) || !Values)
    {
        return Result;
    }
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        FString StringValue;
        if (Value.IsValid() && Value->TryGetString(StringValue) && !StringValue.IsEmpty())
        {
            Result.Add(StringValue);
        }
    }
    return Result;
}

static TSharedPtr<FJsonObject> McpDiagnosticsDumpObjectProperties(
    UObject* Object,
    const TArray<FString>& PropertyAllowList,
    int32 MaxProperties,
    bool& bOutTruncated)
{
    TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
    bOutTruncated = false;
    if (!Object)
    {
        return Properties;
    }

    int32 AddedProperties = 0;
    for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property)
        {
            continue;
        }

        const FString PropertyName = Property->GetName();
        if (!McpDiagnosticsStringArrayContains(PropertyAllowList, PropertyName))
        {
            continue;
        }

        if (AddedProperties >= MaxProperties)
        {
            bOutTruncated = true;
            break;
        }

        if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
        {
            UObject* ReferencedObject = ObjectProperty->GetObjectPropertyValue_InContainer(Object);
            Properties->SetObjectField(PropertyName, McpDiagnosticsObjectSummary(ReferencedObject));
        }
        else
        {
            FString TextValue;
            const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
            MCP_PROPERTY_EXPORT_TEXT(Property, TextValue, ValuePtr, nullptr, Object, PPF_None);
            Properties->SetStringField(PropertyName, TextValue);
        }
        ++AddedProperties;
    }
    return Properties;
}

static void McpDiagnosticsAppendTypedSummary(UMaterialExpression* Expression, TSharedRef<FJsonObject> Out)
{
    if (!Expression)
    {
        return;
    }

    TSharedPtr<FJsonObject> Typed = MakeShared<FJsonObject>();
    if (UMaterialExpressionFunctionInput* Input = Cast<UMaterialExpressionFunctionInput>(Expression))
    {
        Typed->SetStringField(TEXT("kind"), TEXT("FunctionInput"));
        Typed->SetStringField(TEXT("inputName"), Input->InputName.ToString());
        Typed->SetStringField(TEXT("description"), Input->Description);
        Typed->SetStringField(TEXT("functionInputId"), Input->Id.ToString());
        Typed->SetStringField(TEXT("inputType"), StaticEnum<EFunctionInputType>()->GetNameStringByValue(static_cast<int64>(Input->InputType)));
        Typed->SetBoolField(TEXT("usePreviewValueAsDefault"), Input->bUsePreviewValueAsDefault);
        Typed->SetNumberField(TEXT("sortPriority"), Input->SortPriority);
        FString BlendRel;
        switch (Input->BlendInputRelevance) {
            case EBlendInputRelevance::Top: BlendRel = TEXT("Top"); break;
            case EBlendInputRelevance::Bottom: BlendRel = TEXT("Bottom"); break;
            default: BlendRel = TEXT("General"); break;
        }
        Typed->SetStringField(TEXT("blendInputRelevance"), BlendRel);
    }
    else if (UMaterialExpressionFunctionOutput* Output = Cast<UMaterialExpressionFunctionOutput>(Expression))
    {
        Typed->SetStringField(TEXT("kind"), TEXT("FunctionOutput"));
        Typed->SetStringField(TEXT("outputName"), Output->OutputName.ToString());
        Typed->SetStringField(TEXT("description"), Output->Description);
        Typed->SetStringField(TEXT("functionOutputId"), Output->Id.ToString());
    }
    else if (UMaterialExpressionMaterialFunctionCall* Call = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
    {
        Typed->SetStringField(TEXT("kind"), TEXT("MaterialFunctionCall"));
        Typed->SetStringField(TEXT("functionPath"), Call->MaterialFunction ? Call->MaterialFunction->GetPathName() : TEXT(""));
        Typed->SetNumberField(TEXT("functionInputCount"), (double)Call->FunctionInputs.Num());
        Typed->SetNumberField(TEXT("functionOutputCount"), (double)Call->FunctionOutputs.Num());
        TArray<TSharedPtr<FJsonValue>> FuncInputPins, FuncOutputPins;
        for (const FFunctionExpressionInput& FEI : Call->FunctionInputs) {
            TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
            P->SetStringField(TEXT("id"), FEI.ExpressionInputId.ToString());
            P->SetStringField(TEXT("name"), FEI.Input.InputName.ToString());
            FuncInputPins.Add(MakeShared<FJsonValueObject>(P));
        }
        for (const FFunctionExpressionOutput& FEO : Call->FunctionOutputs) {
            TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
            P->SetStringField(TEXT("id"), FEO.ExpressionOutputId.ToString());
            P->SetStringField(TEXT("name"), FEO.Output.OutputName.ToString());
            FuncOutputPins.Add(MakeShared<FJsonValueObject>(P));
        }
        Typed->SetArrayField(TEXT("functionInputPins"), FuncInputPins);
        Typed->SetArrayField(TEXT("functionOutputPins"), FuncOutputPins);
    }
    else if (UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expression))
    {
        Typed->SetStringField(TEXT("kind"), TEXT("Custom"));
        Typed->SetStringField(TEXT("code"), Custom->Code);
        Typed->SetStringField(TEXT("outputType"), StaticEnum<ECustomMaterialOutputType>()->GetNameStringByValue(static_cast<int64>(Custom->OutputType)));
        TArray<TSharedPtr<FJsonValue>> InputNames, OutputNames;
        for (const FCustomInput& CI : Custom->Inputs) InputNames.Add(MakeShared<FJsonValueString>(CI.InputName.ToString()));
        for (const FCustomOutput& CO : Custom->AdditionalOutputs) OutputNames.Add(MakeShared<FJsonValueString>(CO.OutputName.ToString()));
        Typed->SetArrayField(TEXT("inputNames"), InputNames);
        Typed->SetArrayField(TEXT("additionalOutputNames"), OutputNames);
        Typed->SetNumberField(TEXT("inputCount"), Custom->Inputs.Num());
        Typed->SetNumberField(TEXT("additionalOutputCount"), Custom->AdditionalOutputs.Num());
        Typed->SetNumberField(TEXT("additionalDefineCount"), Custom->AdditionalDefines.Num());
        Typed->SetNumberField(TEXT("includeFilePathCount"), Custom->IncludeFilePaths.Num());
    }
    else if (UMaterialExpressionTextureSampleParameter* TexSampleParam = Cast<UMaterialExpressionTextureSampleParameter>(Expression))
    {
        Typed->SetStringField(TEXT("kind"), TEXT("TextureSampleParameter"));
        Typed->SetStringField(TEXT("parameterName"), TexSampleParam->ParameterName.ToString());
        Typed->SetStringField(TEXT("group"), TexSampleParam->Group.ToString());
        Typed->SetNumberField(TEXT("sortPriority"), TexSampleParam->SortPriority);
        Typed->SetStringField(TEXT("texturePath"), TexSampleParam->Texture ? TexSampleParam->Texture->GetPathName() : TEXT(""));
        Typed->SetStringField(TEXT("samplerType"), StaticEnum<EMaterialSamplerType>() ?
            StaticEnum<EMaterialSamplerType>()->GetNameStringByValue(static_cast<int64>(TexSampleParam->SamplerType)) : TEXT(""));
    }
    else if (UMaterialExpressionTextureObjectParameter* TexObjParam = Cast<UMaterialExpressionTextureObjectParameter>(Expression))
    {
        Typed->SetStringField(TEXT("kind"), TEXT("TextureObjectParameter"));
        Typed->SetStringField(TEXT("parameterName"), TexObjParam->ParameterName.ToString());
        Typed->SetStringField(TEXT("group"), TexObjParam->Group.ToString());
        Typed->SetNumberField(TEXT("sortPriority"), TexObjParam->SortPriority);
        Typed->SetStringField(TEXT("texturePath"), TexObjParam->Texture ? TexObjParam->Texture->GetPathName() : TEXT(""));
    }
    else if (UMaterialExpressionTextureObject* TexObj = Cast<UMaterialExpressionTextureObject>(Expression))
    {
        Typed->SetStringField(TEXT("kind"), TEXT("TextureObject"));
        Typed->SetStringField(TEXT("texturePath"), TexObj->Texture ? TexObj->Texture->GetPathName() : TEXT(""));
    }
    else if (UMaterialExpressionTextureSample* TexSample = Cast<UMaterialExpressionTextureSample>(Expression))
    {
        Typed->SetStringField(TEXT("kind"), TEXT("TextureSample"));
        Typed->SetStringField(TEXT("texturePath"), TexSample->Texture ? TexSample->Texture->GetPathName() : TEXT(""));
        Typed->SetStringField(TEXT("samplerType"), StaticEnum<EMaterialSamplerType>() ?
            StaticEnum<EMaterialSamplerType>()->GetNameStringByValue(static_cast<int64>(TexSample->SamplerType)) : TEXT(""));
    }
    else if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expression))
    {
        Typed->SetStringField(TEXT("kind"), TEXT("ScalarParameter"));
        Typed->SetStringField(TEXT("parameterName"), ScalarParam->ParameterName.ToString());
        Typed->SetStringField(TEXT("group"), ScalarParam->Group.ToString());
        Typed->SetNumberField(TEXT("sortPriority"), ScalarParam->SortPriority);
        Typed->SetNumberField(TEXT("defaultValue"), ScalarParam->DefaultValue);
    }
    else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expression))
    {
        Typed->SetStringField(TEXT("kind"), TEXT("VectorParameter"));
        Typed->SetStringField(TEXT("parameterName"), VectorParam->ParameterName.ToString());
        Typed->SetStringField(TEXT("group"), VectorParam->Group.ToString());
        Typed->SetNumberField(TEXT("sortPriority"), VectorParam->SortPriority);
        TSharedPtr<FJsonObject> DefVal = MakeShared<FJsonObject>();
        DefVal->SetNumberField(TEXT("r"), VectorParam->DefaultValue.R);
        DefVal->SetNumberField(TEXT("g"), VectorParam->DefaultValue.G);
        DefVal->SetNumberField(TEXT("b"), VectorParam->DefaultValue.B);
        DefVal->SetNumberField(TEXT("a"), VectorParam->DefaultValue.A);
        Typed->SetObjectField(TEXT("defaultValue"), DefVal);
    }
    else if (UMaterialExpressionStaticSwitchParameter* SwitchParam = Cast<UMaterialExpressionStaticSwitchParameter>(Expression))
    {
        Typed->SetStringField(TEXT("kind"), TEXT("StaticSwitchParameter"));
        Typed->SetStringField(TEXT("parameterName"), SwitchParam->ParameterName.ToString());
        Typed->SetStringField(TEXT("group"), SwitchParam->Group.ToString());
        Typed->SetNumberField(TEXT("sortPriority"), SwitchParam->SortPriority);
        Typed->SetBoolField(TEXT("defaultValue"), SwitchParam->DefaultValue);
    }
    else if (UMaterialExpressionParameter* Parameter = Cast<UMaterialExpressionParameter>(Expression))
    {
        Typed->SetStringField(TEXT("kind"), TEXT("Parameter"));
        Typed->SetStringField(TEXT("parameterName"), Parameter->ParameterName.ToString());
        Typed->SetStringField(TEXT("group"), Parameter->Group.ToString());
        Typed->SetNumberField(TEXT("sortPriority"), Parameter->SortPriority);
    }
    else
    {
        Typed->SetStringField(TEXT("kind"), TEXT("Expression"));
    }

    Out->SetObjectField(TEXT("typedSummary"), Typed);
}

static TArray<TSharedPtr<FJsonValue>> McpDiagnosticsBuildPins(
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpression* Expression,
    bool bInputs)
{
    TArray<TSharedPtr<FJsonValue>> Pins;
    if (!Expression)
    {
        return Pins;
    }

    if (bInputs)
    {
        int32 Index = 0;
        for (FExpressionInputIterator It(Expression); It; ++It, ++Index)
        {
            FExpressionInput* Input = It.Input;
            TSharedPtr<FJsonObject> Pin = MakeShared<FJsonObject>();
            Pin->SetNumberField(TEXT("index"), Index);
            Pin->SetStringField(TEXT("name"), Expression->GetInputName(Index).ToString());
            Pin->SetBoolField(TEXT("connected"), Input && Input->Expression != nullptr);
            if (Input && Input->Expression)
            {
                Pin->SetObjectField(
                    TEXT("connectedExpression"),
                    McpDiagnosticsExpressionIdentity(
                        Owner,
                        Input->Expression,
                        McpDiagnosticsExpressionIndex(Owner, Input->Expression)));
                Pin->SetNumberField(TEXT("sourceOutputIndex"), Input->OutputIndex);
                Pin->SetBoolField(TEXT("maskR"), Input->MaskR != 0);
                Pin->SetBoolField(TEXT("maskG"), Input->MaskG != 0);
                Pin->SetBoolField(TEXT("maskB"), Input->MaskB != 0);
                Pin->SetBoolField(TEXT("maskA"), Input->MaskA != 0);
            }
            Pins.Add(MakeShared<FJsonValueObject>(Pin));
        }
    }
    else
    {
        TArray<FExpressionOutput>& Outputs = Expression->GetOutputs();
        for (int32 Index = 0; Index < Outputs.Num(); ++Index)
        {
            TSharedPtr<FJsonObject> Pin = MakeShared<FJsonObject>();
            Pin->SetNumberField(TEXT("index"), Index);
            Pin->SetStringField(TEXT("name"), Outputs[Index].OutputName.ToString());
            Pin->SetBoolField(TEXT("nameResolved"), !Outputs[Index].OutputName.IsNone());
            Pin->SetNumberField(TEXT("mask"), Outputs[Index].Mask);
            Pin->SetBoolField(TEXT("maskR"), Outputs[Index].MaskR != 0);
            Pin->SetBoolField(TEXT("maskG"), Outputs[Index].MaskG != 0);
            Pin->SetBoolField(TEXT("maskB"), Outputs[Index].MaskB != 0);
            Pin->SetBoolField(TEXT("maskA"), Outputs[Index].MaskA != 0);
            Pins.Add(MakeShared<FJsonValueObject>(Pin));
        }
    }
    return Pins;
}

static TSharedPtr<FJsonObject> McpDiagnosticsBuildGraphSummary(
    const FMcpMaterialGraphOwner& Owner,
    int32 MaxDepth,
    const FString& Verbosity,
    const TArray<FString>& ClassAllowList,
    const TArray<FString>& PropertyAllowList,
    bool& bOutTruncated)
{
    TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
    Summary->SetStringField(TEXT("ownerKind"), McpDiagnosticsOwnerKindToString(Owner.Kind));
    Summary->SetStringField(TEXT("assetClass"), Owner.Asset ? Owner.Asset->GetClass()->GetName() : TEXT(""));
    Summary->SetStringField(TEXT("assetPath"), Owner.Asset ? Owner.Asset->GetPathName() : TEXT(""));

    bOutTruncated = false;
    const TArray<TObjectPtr<UMaterialExpression>>* Expressions = McpGetGraphExpressions(Owner);
    TArray<TSharedPtr<FJsonValue>> ExpressionArray;
    if (Expressions)
    {
        const int32 MaxExpressions = Verbosity.Equals(TEXT("full"), ESearchCase::IgnoreCase) ? 250 : 100;
        for (int32 Index = 0; Index < Expressions->Num(); ++Index)
        {
            UMaterialExpression* Expression = (*Expressions)[Index];
            if (!Expression)
            {
                TSharedPtr<FJsonObject> NullExpression = MakeShared<FJsonObject>();
                NullExpression->SetNumberField(TEXT("expressionIndex"), Index);
                NullExpression->SetBoolField(TEXT("isNull"), true);
                ExpressionArray.Add(MakeShared<FJsonValueObject>(NullExpression));
                continue;
            }

            if (!McpDiagnosticsStringArrayContains(ClassAllowList, Expression->GetClass()->GetName()))
            {
                continue;
            }

            if (ExpressionArray.Num() >= MaxExpressions)
            {
                bOutTruncated = true;
                break;
            }

            TSharedPtr<FJsonObject> ExpressionObject = McpDiagnosticsExpressionIdentity(Owner, Expression, Index);
            ExpressionObject->SetStringField(TEXT("outerPath"), Expression->GetOuter() ? Expression->GetOuter()->GetPathName() : TEXT(""));
            ExpressionObject->SetStringField(TEXT("classPath"), Expression->GetClass()->GetPathName());
            ExpressionObject->SetNumberField(TEXT("editorX"), Expression->MaterialExpressionEditorX);
            ExpressionObject->SetNumberField(TEXT("editorY"), Expression->MaterialExpressionEditorY);
            ExpressionObject->SetStringField(TEXT("description"), Expression->Desc);
            ExpressionObject->SetStringField(TEXT("objectFlags"), LexToString(Expression->GetFlags()));
            if (!Verbosity.Equals(TEXT("summary"), ESearchCase::IgnoreCase))
            {
                bool bPropertyDumpTruncated = false;
                ExpressionObject->SetObjectField(
                    TEXT("properties"),
                    McpDiagnosticsDumpObjectProperties(
                        Expression,
                        PropertyAllowList,
                        MaxDepth <= 1 ? 24 : 96,
                        bPropertyDumpTruncated));
                bOutTruncated = bOutTruncated || bPropertyDumpTruncated;
                McpDiagnosticsAppendTypedSummary(Expression, ExpressionObject.ToSharedRef());
            }
            ExpressionArray.Add(MakeShared<FJsonValueObject>(ExpressionObject));
        }
    }
    Summary->SetArrayField(TEXT("expressions"), ExpressionArray);
    Summary->SetNumberField(TEXT("expressionCount"), Expressions ? Expressions->Num() : 0);
    Summary->SetBoolField(TEXT("truncated"), bOutTruncated);
    return Summary;
}

static TArray<TSharedPtr<FJsonValue>> McpDiagnosticsBuildParameterNamespace(const FMcpMaterialGraphOwner& Owner)
{
    TArray<TSharedPtr<FJsonValue>> Parameters;

    if (UMaterialFunctionInstance* FuncInst = Cast<UMaterialFunctionInstance>(Owner.Asset))
    {
        // Expose explicit overrides on function instances
        for (const auto& P : FuncInst->ScalarParameterValues) {
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("name"), P.ParameterInfo.Name.ToString());
            Obj->SetStringField(TEXT("type"), TEXT("Scalar"));
            Obj->SetNumberField(TEXT("explicitValue"), P.ParameterValue);
            Obj->SetBoolField(TEXT("hasExplicitOverride"), true);
            Parameters.Add(MakeShared<FJsonValueObject>(Obj));
        }
        for (const auto& P : FuncInst->VectorParameterValues) {
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("name"), P.ParameterInfo.Name.ToString());
            Obj->SetStringField(TEXT("type"), TEXT("Vector"));
            TSharedPtr<FJsonObject> Col = MakeShared<FJsonObject>();
            Col->SetNumberField(TEXT("r"), P.ParameterValue.R); Col->SetNumberField(TEXT("g"), P.ParameterValue.G);
            Col->SetNumberField(TEXT("b"), P.ParameterValue.B); Col->SetNumberField(TEXT("a"), P.ParameterValue.A);
            Obj->SetObjectField(TEXT("explicitValue"), Col);
            Obj->SetBoolField(TEXT("hasExplicitOverride"), true);
            Parameters.Add(MakeShared<FJsonValueObject>(Obj));
        }
        for (const auto& P : FuncInst->TextureParameterValues) {
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("name"), P.ParameterInfo.Name.ToString());
            Obj->SetStringField(TEXT("type"), TEXT("Texture"));
            Obj->SetStringField(TEXT("explicitValue"), P.ParameterValue.Get() ? P.ParameterValue.Get()->GetPathName() : TEXT(""));
            Obj->SetBoolField(TEXT("hasExplicitOverride"), true);
            Parameters.Add(MakeShared<FJsonValueObject>(Obj));
        }
        return Parameters;
    }

    if (UMaterialInstance* Instance = Cast<UMaterialInstance>(Owner.Asset))
    {
        TArray<FMaterialParameterInfo> ScalarInfos;
        TArray<FGuid> ScalarIds;
        Instance->GetAllScalarParameterInfo(ScalarInfos, ScalarIds);
        for (int32 Index = 0; Index < ScalarInfos.Num(); ++Index)
        {
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("name"), ScalarInfos[Index].Name.ToString());
            Obj->SetStringField(TEXT("type"), TEXT("Scalar"));
            Obj->SetStringField(TEXT("association"), StaticEnum<EMaterialParameterAssociation>()->GetNameStringByValue(static_cast<int64>(ScalarInfos[Index].Association)));
            Obj->SetNumberField(TEXT("index"), ScalarInfos[Index].Index);
            if (ScalarIds.IsValidIndex(Index))
            {
                Obj->SetStringField(TEXT("expressionGuid"), ScalarIds[Index].ToString());
            }
            float Value = 0.0f;
            if (Instance->GetScalarParameterValue(FHashedMaterialParameterInfo(ScalarInfos[Index]), Value))
            {
                Obj->SetNumberField(TEXT("effectiveValue"), Value);
            }
            Parameters.Add(MakeShared<FJsonValueObject>(Obj));
        }

        TArray<FMaterialParameterInfo> VectorInfos;
        TArray<FGuid> VectorIds;
        Instance->GetAllVectorParameterInfo(VectorInfos, VectorIds);
        for (int32 Index = 0; Index < VectorInfos.Num(); ++Index)
        {
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("name"), VectorInfos[Index].Name.ToString());
            Obj->SetStringField(TEXT("type"), TEXT("Vector"));
            Obj->SetStringField(TEXT("association"), StaticEnum<EMaterialParameterAssociation>()->GetNameStringByValue(static_cast<int64>(VectorInfos[Index].Association)));
            Obj->SetNumberField(TEXT("index"), VectorInfos[Index].Index);
            if (VectorIds.IsValidIndex(Index))
            {
                Obj->SetStringField(TEXT("expressionGuid"), VectorIds[Index].ToString());
            }
            Parameters.Add(MakeShared<FJsonValueObject>(Obj));
        }

        TArray<FMaterialParameterInfo> TextureInfos;
        TArray<FGuid> TextureIds;
        Instance->GetAllTextureParameterInfo(TextureInfos, TextureIds);
        for (int32 Index = 0; Index < TextureInfos.Num(); ++Index)
        {
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("name"), TextureInfos[Index].Name.ToString());
            Obj->SetStringField(TEXT("type"), TEXT("Texture"));
            Obj->SetStringField(TEXT("association"), StaticEnum<EMaterialParameterAssociation>()->GetNameStringByValue(static_cast<int64>(TextureInfos[Index].Association)));
            Obj->SetNumberField(TEXT("index"), TextureInfos[Index].Index);
            if (TextureIds.IsValidIndex(Index))
            {
                Obj->SetStringField(TEXT("expressionGuid"), TextureIds[Index].ToString());
            }
            Parameters.Add(MakeShared<FJsonValueObject>(Obj));
        }
        return Parameters;
    }

    const TArray<TObjectPtr<UMaterialExpression>>* Expressions = McpGetGraphExpressions(Owner);
    if (!Expressions)
    {
        return Parameters;
    }

    for (int32 Index = 0; Index < Expressions->Num(); ++Index)
    {
        UMaterialExpressionParameter* Parameter = Cast<UMaterialExpressionParameter>((*Expressions)[Index]);
        if (!Parameter)
        {
            continue;
        }

        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"), Parameter->ParameterName.ToString());
        Obj->SetStringField(TEXT("type"), Parameter->GetClass()->GetName());
        Obj->SetStringField(TEXT("group"), Parameter->Group.ToString());
        Obj->SetNumberField(TEXT("sortPriority"), Parameter->SortPriority);
        Obj->SetStringField(TEXT("expressionGuid"), Parameter->MaterialExpressionGuid.ToString());
        Obj->SetObjectField(TEXT("expressionIdentity"), McpDiagnosticsExpressionIdentity(Owner, Parameter, Index));
        Parameters.Add(MakeShared<FJsonValueObject>(Obj));
    }
    return Parameters;
}

static bool McpDiagnosticsResolveGraphOwner(
    const FString& AssetPath,
    FMcpMaterialGraphOwner& OutGraphOwner,
    FString& OutError,
    FString& OutErrorCode)
{
    if (McpResolveMaterialGraphOwner(AssetPath, OutGraphOwner, OutError))
    {
        return true;
    }

    if (UMaterialInstanceConstant* Instance = LoadObject<UMaterialInstanceConstant>(nullptr, *AssetPath))
    {
        OutGraphOwner.Asset = Instance;
        OutGraphOwner.GraphSource = nullptr;
        OutGraphOwner.Kind = EMcpMaterialGraphOwnerKind::Material;
        OutGraphOwner.bReadOnly = true;
        return true;
    }

    if (UMaterialFunctionInstance* FuncInst = LoadObject<UMaterialFunctionInstance>(nullptr, *AssetPath))
    {
        OutGraphOwner.Asset = FuncInst;
        OutGraphOwner.GraphSource = nullptr;
        OutGraphOwner.Kind = EMcpMaterialGraphOwnerKind::MaterialFunctionInstance;
        OutGraphOwner.bReadOnly = true;
        return true;
    }

    OutErrorCode = OutError.Contains(TEXT("not found")) ? TEXT("invalid-asset") : TEXT("unsupported-asset-type");
    return false;
}

static bool McpDiagnosticsCloseEditorForReload(UObject* Asset)
{
    if (!Asset || !GEditor)
    {
        return true;
    }

    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!AssetEditorSubsystem)
    {
        return true;
    }

    if (!AssetEditorSubsystem->FindEditorForAsset(Asset, false))
    {
        return true;
    }

    AssetEditorSubsystem->CloseAllEditorsForAsset(Asset);
    return AssetEditorSubsystem->FindEditorForAsset(Asset, false) == nullptr;
}

#endif
}

bool UMcpAutomationBridgeSubsystem::HandleManageMaterialDiagnosticsAction(
    const FString &RequestId,
    const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (Action != TEXT("manage_material_diagnostics"))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing payload."), TEXT("missing-field"));
        return true;
    }

    FString SubAction;
    if (!Payload->TryGetStringField(TEXT("subAction"), SubAction) || SubAction.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'subAction' for manage_material_diagnostics."), TEXT("missing-field"));
        return true;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("missing-field"));
        return true;
    }

    AssetPath = SanitizeProjectRelativePath(AssetPath);
    if (AssetPath.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Invalid assetPath."), TEXT("invalid-asset"));
        return true;
    }

    FMcpMaterialGraphOwner GraphOwner;
    FString GraphOwnerError;
    FString GraphOwnerErrorCode;
    if (!McpDiagnosticsResolveGraphOwner(AssetPath, GraphOwner, GraphOwnerError, GraphOwnerErrorCode))
    {
        SendAutomationError(Socket, RequestId, GraphOwnerError, GraphOwnerErrorCode);
        return true;
    }

    const FString LowerSubAction = SubAction.ToLower();

    if (LowerSubAction == TEXT("dump_raw_graph"))
    {
        if (!GraphOwner.GraphSource)
        {
            SendAutomationError(Socket, RequestId, TEXT("Target asset does not own a material graph."), TEXT("unsupported-asset-type"));
            return true;
        }

        int32 MaxDepth = 3;
        Payload->TryGetNumberField(TEXT("maxDepth"), MaxDepth);
        MaxDepth = FMath::Clamp(MaxDepth, 1, 8);

        FString Verbosity = TEXT("normal");
        Payload->TryGetStringField(TEXT("verbosity"), Verbosity);

        bool bTruncated = false;
        TSharedPtr<FJsonObject> Result = McpDiagnosticsBuildGraphSummary(
            GraphOwner,
            MaxDepth,
            Verbosity,
            McpDiagnosticsGetStringArray(Payload, TEXT("classAllowList")),
            McpDiagnosticsGetStringArray(Payload, TEXT("propertyAllowList")),
            bTruncated);
        Result->SetArrayField(TEXT("comments"), TArray<TSharedPtr<FJsonValue>>());
        SendAutomationResponse(Socket, RequestId, true, TEXT("Raw material graph diagnostics dumped."), Result);
        return true;
    }

    if (LowerSubAction == TEXT("dump_raw_expression"))
    {
        UMaterialExpression* Expression = McpDiagnosticsFindExpression(GraphOwner, Payload);
        if (!Expression)
        {
            SendAutomationError(Socket, RequestId, TEXT("Expression not found."), TEXT("invalid-pin"));
            return true;
        }

        int32 MaxDepth = 3;
        Payload->TryGetNumberField(TEXT("maxDepth"), MaxDepth);
        MaxDepth = FMath::Clamp(MaxDepth, 1, 8);

        bool bTruncated = false;
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), AssetPath);
        Result->SetObjectField(TEXT("expressionIdentity"), McpDiagnosticsExpressionIdentity(
            GraphOwner,
            Expression,
            McpDiagnosticsExpressionIndex(GraphOwner, Expression)));
        Result->SetObjectField(TEXT("properties"), McpDiagnosticsDumpObjectProperties(
            Expression,
            McpDiagnosticsGetStringArray(Payload, TEXT("propertyAllowList")),
            MaxDepth <= 1 ? 24 : 128,
            bTruncated));
        McpDiagnosticsAppendTypedSummary(Expression, Result.ToSharedRef());
        Result->SetBoolField(TEXT("truncated"), bTruncated);
        SendAutomationResponse(Socket, RequestId, true, TEXT("Raw material expression diagnostics dumped."), Result);
        return true;
    }

    if (LowerSubAction == TEXT("dump_pins"))
    {
        UMaterialExpression* Expression = McpDiagnosticsFindExpression(GraphOwner, Payload);
        if (!Expression)
        {
            SendAutomationError(Socket, RequestId, TEXT("Expression not found."), TEXT("invalid-pin"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), AssetPath);
        Result->SetObjectField(TEXT("expressionIdentity"), McpDiagnosticsExpressionIdentity(
            GraphOwner,
            Expression,
            McpDiagnosticsExpressionIndex(GraphOwner, Expression)));
        Result->SetArrayField(TEXT("inputs"), McpDiagnosticsBuildPins(GraphOwner, Expression, true));
        Result->SetArrayField(TEXT("outputs"), McpDiagnosticsBuildPins(GraphOwner, Expression, false));

        // Build mainMaterialPins for UMaterial graph owners
        TArray<TSharedPtr<FJsonValue>> MainPins;
        if (UMaterial* MatOwner = Cast<UMaterial>(GraphOwner.GraphSource))
        {
            Result->SetBoolField(TEXT("bUseMaterialAttributes"), MatOwner->bUseMaterialAttributes);

            auto AddMainPin = [&](const FString& PinName, FExpressionInput& Input) {
                TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
                PinObj->SetStringField(TEXT("name"), PinName);
                PinObj->SetBoolField(TEXT("connected"), Input.Expression != nullptr);
                if (Input.Expression) {
                    const int32 ConnIdx = McpDiagnosticsExpressionIndex(GraphOwner, Input.Expression);
                    PinObj->SetObjectField(TEXT("connectedExpression"), McpDiagnosticsExpressionIdentity(GraphOwner, Input.Expression, ConnIdx));
                    PinObj->SetNumberField(TEXT("outputIndex"), Input.OutputIndex);
                }
                MainPins.Add(MakeShared<FJsonValueObject>(PinObj));
            };

#if WITH_EDITORONLY_DATA
            AddMainPin(TEXT("BaseColor"), MCP_GET_MATERIAL_INPUT(MatOwner, BaseColor));
            AddMainPin(TEXT("EmissiveColor"), MCP_GET_MATERIAL_INPUT(MatOwner, EmissiveColor));
            AddMainPin(TEXT("Roughness"), MCP_GET_MATERIAL_INPUT(MatOwner, Roughness));
            AddMainPin(TEXT("Metallic"), MCP_GET_MATERIAL_INPUT(MatOwner, Metallic));
            AddMainPin(TEXT("Specular"), MCP_GET_MATERIAL_INPUT(MatOwner, Specular));
            AddMainPin(TEXT("Normal"), MCP_GET_MATERIAL_INPUT(MatOwner, Normal));
            AddMainPin(TEXT("Opacity"), MCP_GET_MATERIAL_INPUT(MatOwner, Opacity));
            AddMainPin(TEXT("OpacityMask"), MCP_GET_MATERIAL_INPUT(MatOwner, OpacityMask));
            AddMainPin(TEXT("AmbientOcclusion"), MCP_GET_MATERIAL_INPUT(MatOwner, AmbientOcclusion));
            AddMainPin(TEXT("SubsurfaceColor"), MCP_GET_MATERIAL_INPUT(MatOwner, SubsurfaceColor));
            AddMainPin(TEXT("WorldPositionOffset"), MCP_GET_MATERIAL_INPUT(MatOwner, WorldPositionOffset));
#endif
        }
        Result->SetArrayField(TEXT("mainMaterialPins"), MainPins);
        SendAutomationResponse(Socket, RequestId, true, TEXT("Material pin diagnostics dumped."), Result);
        return true;
    }

    if (LowerSubAction == TEXT("dump_parameter_namespace"))
    {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), AssetPath);
        Result->SetArrayField(TEXT("parameters"), McpDiagnosticsBuildParameterNamespace(GraphOwner));
        SendAutomationResponse(Socket, RequestId, true, TEXT("Material parameter namespace dumped."), Result);
        return true;
    }

    if (LowerSubAction == TEXT("validate_material_graph"))
    {
        TArray<TSharedPtr<FJsonValue>> Issues;
        if (GraphOwner.GraphSource)
        {
            if (const TArray<TObjectPtr<UMaterialExpression>>* Expressions = McpGetGraphExpressions(GraphOwner))
            {
                for (int32 Index = 0; Index < Expressions->Num(); ++Index)
                {
                    UMaterialExpression* Expression = (*Expressions)[Index];
                    if (!Expression)
                    {
                        TSharedPtr<FJsonObject> Issue = MakeShared<FJsonObject>();
                        Issue->SetStringField(TEXT("code"), TEXT("null-expression"));
                        Issue->SetNumberField(TEXT("expressionIndex"), Index);
                        Issue->SetStringField(TEXT("severity"), TEXT("warning"));
                        Issues.Add(MakeShared<FJsonValueObject>(Issue));
                        continue;
                    }

                    if (UMaterialExpressionMaterialFunctionCall* Call = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
                    {
                        if (!Call->MaterialFunction)
                        {
                            TSharedPtr<FJsonObject> Issue = MakeShared<FJsonObject>();
                            Issue->SetStringField(TEXT("code"), TEXT("missing-material-function"));
                            Issue->SetObjectField(TEXT("expressionIdentity"), McpDiagnosticsExpressionIdentity(GraphOwner, Expression, Index));
                            Issue->SetStringField(TEXT("severity"), TEXT("error"));
                            Issues.Add(MakeShared<FJsonValueObject>(Issue));
                        }
                        else
                        {
                            // Check for stale function call pins
                            TArray<FFunctionExpressionInput> ExpectedInputs;
                            TArray<FFunctionExpressionOutput> ExpectedOutputs;
                            Call->MaterialFunction->GetInputsAndOutputs(ExpectedInputs, ExpectedOutputs);

                            for (const FFunctionExpressionInput& Actual : Call->FunctionInputs)
                            {
                                bool bFound = false;
                                for (const FFunctionExpressionInput& Expected : ExpectedInputs)
                                {
                                    if (Expected.ExpressionInputId == Actual.ExpressionInputId) { bFound = true; break; }
                                }
                                if (!bFound)
                                {
                                    TSharedPtr<FJsonObject> Issue = MakeShared<FJsonObject>();
                                    Issue->SetStringField(TEXT("code"), TEXT("stale-function-input-pin"));
                                    Issue->SetObjectField(TEXT("expressionIdentity"), McpDiagnosticsExpressionIdentity(GraphOwner, Expression, Index));
                                    Issue->SetStringField(TEXT("stalePinName"), Actual.Input.InputName.ToString());
                                    Issue->SetStringField(TEXT("stalePinId"), Actual.ExpressionInputId.ToString());
                                    Issue->SetStringField(TEXT("severity"), TEXT("warning"));
                                    Issues.Add(MakeShared<FJsonValueObject>(Issue));
                                }
                            }

                            for (const FFunctionExpressionOutput& Actual : Call->FunctionOutputs)
                            {
                                bool bFound = false;
                                for (const FFunctionExpressionOutput& Expected : ExpectedOutputs)
                                {
                                    if (Expected.ExpressionOutputId == Actual.ExpressionOutputId) { bFound = true; break; }
                                }
                                if (!bFound)
                                {
                                    TSharedPtr<FJsonObject> Issue = MakeShared<FJsonObject>();
                                    Issue->SetStringField(TEXT("code"), TEXT("stale-function-output-pin"));
                                    Issue->SetObjectField(TEXT("expressionIdentity"), McpDiagnosticsExpressionIdentity(GraphOwner, Expression, Index));
                                    Issue->SetStringField(TEXT("stalePinName"), Actual.Output.OutputName.ToString());
                                    Issue->SetStringField(TEXT("stalePinId"), Actual.ExpressionOutputId.ToString());
                                    Issue->SetStringField(TEXT("severity"), TEXT("warning"));
                                    Issues.Add(MakeShared<FJsonValueObject>(Issue));
                                }
                            }
                        }
                    }
                    else if (UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expression))
                    {
                        // Check for duplicate custom expression input names
                        TSet<FName> SeenNames;
                        for (const FCustomInput& CI : Custom->Inputs)
                        {
                            if (SeenNames.Contains(CI.InputName))
                            {
                                TSharedPtr<FJsonObject> Issue = MakeShared<FJsonObject>();
                                Issue->SetStringField(TEXT("code"), TEXT("duplicate-custom-input-name"));
                                Issue->SetObjectField(TEXT("expressionIdentity"), McpDiagnosticsExpressionIdentity(GraphOwner, Expression, Index));
                                Issue->SetStringField(TEXT("duplicateName"), CI.InputName.ToString());
                                Issue->SetStringField(TEXT("severity"), TEXT("error"));
                                Issues.Add(MakeShared<FJsonValueObject>(Issue));
                                break;
                            }
                            SeenNames.Add(CI.InputName);
                        }
                    }

                    // Check for dangling connections (FExpressionInput pointing to a non-null but unresolvable expression)
                    {
                        int32 InputIdx = 0;
                        for (FExpressionInputIterator It(Expression); It; ++It, ++InputIdx)
                        {
                            FExpressionInput* Input = It.Input;
                            if (!Input || !Input->Expression) continue;
                            // Validate that the connected expression is in our graph
                            const TArray<TObjectPtr<UMaterialExpression>>* AllExprs = McpGetGraphExpressions(GraphOwner);
                            if (AllExprs)
                            {
                                bool bExprInGraph = false;
                                for (const auto& ExprPtr : *AllExprs)
                                {
                                    if (ExprPtr == Input->Expression) { bExprInGraph = true; break; }
                                }
                                if (!bExprInGraph)
                                {
                                    TSharedPtr<FJsonObject> Issue = MakeShared<FJsonObject>();
                                    Issue->SetStringField(TEXT("code"), TEXT("dangling-connection"));
                                    Issue->SetObjectField(TEXT("expressionIdentity"), McpDiagnosticsExpressionIdentity(GraphOwner, Expression, Index));
                                    Issue->SetStringField(TEXT("inputName"), Expression->GetInputName(InputIdx).ToString());
                                    Issue->SetNumberField(TEXT("inputIndex"), InputIdx);
                                    Issue->SetStringField(TEXT("severity"), TEXT("error"));
                                    Issues.Add(MakeShared<FJsonValueObject>(Issue));
                                }
                                else
                                {
                                    // Validate output index
                                    const TArray<FExpressionOutput>& Outputs = Input->Expression->GetOutputs();
                                    if (Outputs.Num() > 0 && Input->OutputIndex >= Outputs.Num())
                                    {
                                        TSharedPtr<FJsonObject> Issue = MakeShared<FJsonObject>();
                                        Issue->SetStringField(TEXT("code"), TEXT("invalid-output-index"));
                                        Issue->SetObjectField(TEXT("expressionIdentity"), McpDiagnosticsExpressionIdentity(GraphOwner, Expression, Index));
                                        Issue->SetStringField(TEXT("inputName"), Expression->GetInputName(InputIdx).ToString());
                                        Issue->SetNumberField(TEXT("outputIndex"), Input->OutputIndex);
                                        Issue->SetNumberField(TEXT("maxOutputIndex"), Outputs.Num() - 1);
                                        Issue->SetStringField(TEXT("severity"), TEXT("error"));
                                        Issues.Add(MakeShared<FJsonValueObject>(Issue));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), AssetPath);
        Result->SetArrayField(TEXT("issues"), Issues);
        Result->SetNumberField(TEXT("issueCount"), Issues.Num());
        Result->SetBoolField(TEXT("valid"), Issues.Num() == 0);
        SendAutomationResponse(Socket, RequestId, true, TEXT("Material graph validation complete."), Result);
        return true;
    }

    if (LowerSubAction == TEXT("repair_function_call_pins") ||
        LowerSubAction == TEXT("repair_custom_expression_outputs") ||
        LowerSubAction == TEXT("repair_missing_expression_guids") ||
        LowerSubAction == TEXT("remove_null_expressions"))
    {
        if (GraphOwner.bReadOnly || !GraphOwner.GraphSource)
        {
            SendAutomationError(Socket, RequestId, TEXT("Repair target is not an editable material graph owner."), TEXT("unsupported-operation"));
            return true;
        }

        FScopedTransaction Transaction(NSLOCTEXT("McpAutomationBridge", "MaterialDiagnosticsRepair", "MCP material diagnostics repair"));
        GraphOwner.Asset->Modify();
        if (GraphOwner.GraphSource && GraphOwner.GraphSource != GraphOwner.Asset)
        {
            GraphOwner.GraphSource->Modify();
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), AssetPath);
        bool bChanged = false;

        if (LowerSubAction == TEXT("repair_function_call_pins"))
        {
            UMaterialExpressionMaterialFunctionCall* Call = Cast<UMaterialExpressionMaterialFunctionCall>(
                McpDiagnosticsFindExpression(GraphOwner, Payload));
            if (!Call)
            {
                SendAutomationError(Socket, RequestId, TEXT("Target expression is not a material function call."), TEXT("invalid-pin"));
                return true;
            }
            Call->Modify();
            Call->UpdateFromFunctionResource();
            Result->SetObjectField(TEXT("expressionIdentity"), McpDiagnosticsExpressionIdentity(
                GraphOwner,
                Call,
                McpDiagnosticsExpressionIndex(GraphOwner, Call)));
            Result->SetNumberField(TEXT("changedInputs"), Call->FunctionInputs.Num());
            Result->SetNumberField(TEXT("changedOutputs"), Call->FunctionOutputs.Num());
            bChanged = true;
        }
        else if (LowerSubAction == TEXT("repair_custom_expression_outputs"))
        {
            UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(
                McpDiagnosticsFindExpression(GraphOwner, Payload));
            if (!Custom)
            {
                SendAutomationError(Socket, RequestId, TEXT("Target expression is not a custom expression."), TEXT("invalid-pin"));
                return true;
            }
            Custom->Modify();
            Custom->RebuildOutputs();
            Result->SetObjectField(TEXT("expressionIdentity"), McpDiagnosticsExpressionIdentity(
                GraphOwner,
                Custom,
                McpDiagnosticsExpressionIndex(GraphOwner, Custom)));
            Result->SetArrayField(TEXT("outputPins"), McpDiagnosticsBuildPins(GraphOwner, Custom, false));
            bChanged = true;
        }
        else if (LowerSubAction == TEXT("repair_missing_expression_guids"))
        {
            TArray<TSharedPtr<FJsonValue>> RepairedExpressions;
            TArray<TSharedPtr<FJsonValue>> SkippedExpressions;
            if (const TArray<TObjectPtr<UMaterialExpression>>* Expressions = McpGetGraphExpressions(GraphOwner))
            {
                for (int32 Index = 0; Index < Expressions->Num(); ++Index)
                {
                    UMaterialExpression* Expression = (*Expressions)[Index];
                    if (!Expression)
                    {
                        continue;
                    }
                    if (!Expression->MaterialExpressionGuid.IsValid())
                    {
                        Expression->Modify();
                        Expression->MaterialExpressionGuid = FGuid::NewGuid();
                        RepairedExpressions.Add(MakeShared<FJsonValueObject>(
                            McpDiagnosticsExpressionIdentity(GraphOwner, Expression, Index)));
                        bChanged = true;
                    }
                    else
                    {
                        SkippedExpressions.Add(MakeShared<FJsonValueObject>(
                            McpDiagnosticsExpressionIdentity(GraphOwner, Expression, Index)));
                    }
                }
            }
            Result->SetArrayField(TEXT("repairedExpressions"), RepairedExpressions);
            Result->SetArrayField(TEXT("skippedExpressions"), SkippedExpressions);
        }
        else if (LowerSubAction == TEXT("remove_null_expressions"))
        {
            int32 RemovedCount = 0;
            if (TArray<TObjectPtr<UMaterialExpression>>* Expressions = McpGetGraphExpressionsMutable(GraphOwner))
            {
                RemovedCount = Expressions->RemoveAll([](const TObjectPtr<UMaterialExpression>& Expression)
                {
                    return Expression == nullptr;
                });
            }
            Result->SetNumberField(TEXT("removedCount"), RemovedCount);
            bChanged = RemovedCount > 0;
        }

        if (bChanged)
        {
            FString RebuildError;
            McpRebuildMaterialGraphOwner(GraphOwner, RebuildError);
        }

        bool bSave = false;
        Payload->TryGetBoolField(TEXT("save"), bSave);
        bool bSaved = false;
        if (bSave)
        {
            bSaved = UEditorAssetLibrary::SaveLoadedAsset(GraphOwner.Asset);
            if (!bSaved)
            {
                SendAutomationError(Socket, RequestId, TEXT("Failed to save repaired material-family asset."), TEXT("save-failed"));
                return true;
            }
        }
        Result->SetBoolField(TEXT("saved"), bSaved);
        Result->SetBoolField(TEXT("dirty"), GraphOwner.Asset && GraphOwner.Asset->GetOutermost()->IsDirty());
        SendAutomationResponse(Socket, RequestId, true, TEXT("Material diagnostics repair complete."), Result);
        return true;
    }

    if (LowerSubAction == TEXT("compile_material_diagnostics"))
    {
        const double StartTime = FPlatformTime::Seconds();
        double TimeoutSeconds = 10.0;
        Payload->TryGetNumberField(TEXT("timeoutSeconds"), TimeoutSeconds);
        TimeoutSeconds = FMath::Clamp(TimeoutSeconds, 0.0, 120.0);

        UMaterial* CompileTarget = nullptr;
        FString TargetKind;
        if (UMaterial* Material = Cast<UMaterial>(GraphOwner.Asset))
        {
            TargetKind = TEXT("Material");
            CompileTarget = Material;
            Material->PreEditChange(nullptr);
            Material->PostEditChange();
        }
        else if (UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(GraphOwner.Asset))
        {
            TargetKind = TEXT("MaterialInstance");
            Instance->PreEditChange(nullptr);
            Instance->PostEditChange();
            CompileTarget = Instance->GetMaterial();
        }
        else if (UMaterialFunction* Function = Cast<UMaterialFunction>(GraphOwner.GraphSource))
        {
            TargetKind = GraphOwner.Kind == EMcpMaterialGraphOwnerKind::MaterialFunctionInstance
                ? TEXT("MaterialFunctionInstance")
                : TEXT("MaterialFunction");
            // N1: use canonical compile path for MaterialFunction (updates I/O types and dependent materials)
            FMaterialUpdateContext UpdateContext;
            Function->ForceRecompileForRendering(UpdateContext, nullptr);
        }

        bool bPending = false;
        bool bSuccess = true;
        if (CompileTarget)
        {
            while (CompileTarget->IsCompiling())
            {
                if ((FPlatformTime::Seconds() - StartTime) >= TimeoutSeconds)
                {
                    bPending = true;
                    bSuccess = false;
                    break;
                }
                FPlatformProcess::Sleep(0.05f);
            }
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), AssetPath);
        Result->SetStringField(TEXT("compileTargetPath"), CompileTarget ? CompileTarget->GetPathName() : TEXT(""));
        Result->SetStringField(TEXT("targetKind"), TargetKind);
        Result->SetBoolField(TEXT("success"), bSuccess);
        Result->SetArrayField(TEXT("messages"), TArray<TSharedPtr<FJsonValue>>());
        Result->SetNumberField(TEXT("durationSeconds"), FPlatformTime::Seconds() - StartTime);
        Result->SetBoolField(TEXT("pending"), bPending);
        SendAutomationResponse(Socket, RequestId, true, TEXT("Material compile diagnostics complete."), Result);
        return true;
    }

    if (LowerSubAction == TEXT("verify_save_reload"))
    {
        UObject* Asset = GraphOwner.Asset;
        if (!Asset)
        {
            SendAutomationError(Socket, RequestId, TEXT("Asset not found."), TEXT("invalid-asset"));
            return true;
        }

        bool bSave = true;
        Payload->TryGetBoolField(TEXT("save"), bSave);
        if (!bSave && Asset->GetOutermost()->IsDirty())
        {
            SendAutomationError(Socket, RequestId, TEXT("Asset has unsaved changes and save=false was requested."), TEXT("asset-busy"));
            return true;
        }

        bool bBeforeTruncated = false;
        bool bAfterTruncated = false;
        TSharedPtr<FJsonObject> Before = GraphOwner.GraphSource
            ? McpDiagnosticsBuildGraphSummary(GraphOwner, 1, TEXT("summary"), TArray<FString>(), TArray<FString>(), bBeforeTruncated)
            : McpDiagnosticsObjectSummary(Asset);

        if (!McpDiagnosticsCloseEditorForReload(Asset))
        {
            SendAutomationError(Socket, RequestId, TEXT("Open asset editor could not be closed before reload."), TEXT("asset-busy"));
            return true;
        }

        bool bSaved = false;
        if (bSave)
        {
            bSaved = UEditorAssetLibrary::SaveLoadedAsset(Asset);
            if (!bSaved)
            {
                SendAutomationError(Socket, RequestId, TEXT("Failed to save asset before reload verification."), TEXT("save-failed"));
                return true;
            }
        }

        // Saving reloads from disk on demand later; force a fresh load by clearing stale package state only when safe.
        UObject* ReloadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
        FMcpMaterialGraphOwner ReloadedGraphOwner;
        FString ReloadedError;
        const bool bReloadedGraph = ReloadedAsset && McpResolveMaterialGraphOwner(AssetPath, ReloadedGraphOwner, ReloadedError);

        TSharedPtr<FJsonObject> After = bReloadedGraph
            ? McpDiagnosticsBuildGraphSummary(ReloadedGraphOwner, 1, TEXT("summary"), TArray<FString>(), TArray<FString>(), bAfterTruncated)
            : McpDiagnosticsObjectSummary(ReloadedAsset);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), AssetPath);
        Result->SetObjectField(TEXT("before"), Before);
        Result->SetObjectField(TEXT("after"), After);
        Result->SetBoolField(TEXT("saved"), bSaved);
        Result->SetBoolField(TEXT("reloaded"), ReloadedAsset != nullptr);
        SendAutomationResponse(Socket, RequestId, true, TEXT("Material save/reload verification complete."), Result);
        return true;
    }

    SendAutomationError(
        Socket,
        RequestId,
        FString::Printf(TEXT("Unknown material diagnostics subAction: %s"), *SubAction),
        TEXT("invalid-subaction"));
    return true;
#else
    SendAutomationError(Socket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}
