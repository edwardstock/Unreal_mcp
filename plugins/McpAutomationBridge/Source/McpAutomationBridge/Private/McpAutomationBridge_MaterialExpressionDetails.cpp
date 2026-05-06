// McpAutomationBridge_MaterialExpressionDetails.cpp
#include "McpVersionCompatibility.h"  // MUST be first

#include "McpAutomationBridge_MaterialExpressionDetails.h"
#include "McpAutomationBridgeGlobals.h"

#if WITH_EDITOR
#include "McpAutomationBridgeHelpers.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialExpressionGetMaterialAttributes.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialExpressionLandscapeLayerWeight.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionLandscapePhysicalMaterialOutput.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "McpFunctionInputTypeName.h"
#endif

#if WITH_EDITOR
FString McpFunctionInputTypeName(EFunctionInputType Type)
{
    switch (Type)
    {
        case FunctionInput_Scalar:             return TEXT("Float1");
        case FunctionInput_Vector2:            return TEXT("Float2");
        case FunctionInput_Vector3:            return TEXT("Float3");
        case FunctionInput_Vector4:            return TEXT("Float4");
        case FunctionInput_Texture2D:          return TEXT("Texture2D");
        case FunctionInput_TextureCube:        return TEXT("TextureCube");
        case FunctionInput_Texture2DArray:     return TEXT("Texture2DArray");
        case FunctionInput_VolumeTexture:      return TEXT("VolumeTexture");
        case FunctionInput_StaticBool:         return TEXT("StaticBool");
        case FunctionInput_MaterialAttributes: return TEXT("MaterialAttributes");
        case FunctionInput_TextureExternal:    return TEXT("TextureExternal");
        case FunctionInput_Bool:               return TEXT("Bool");
        case FunctionInput_Substrate:          return TEXT("Substrate");
        default:                               return TEXT("Unknown");
    }
}
#endif

#if WITH_EDITOR
extern void McpEmitInputPinJson(
    const FMcpMaterialGraphOwner& Owner,
    const struct FExpressionInput* Input,
    const FString& PinName,
    const TSharedRef<FJsonObject>& Out);

extern FString McpLandscapeBlendTypeToString(ELandscapeLayerBlendType BlendType);

extern FString McpGetOutputName(UMaterialExpression* Expression, int32 OutputIndex, bool& bOutResolved);
extern TSharedPtr<FJsonObject> McpBuildExpressionRef(
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpression* Expression);
#endif

namespace McpMaterialExpressionDetails
{
    // forward declarations so AppendTypedDetails can call them before the definitions below
    void AppendCustomDetails(const FMcpMaterialGraphOwner& Owner, UMaterialExpressionCustom* Custom, const TSharedRef<FJsonObject>& Resp);
    bool AppendParameterDetails(UMaterialExpression* Expression, const TSharedRef<FJsonObject>& Resp);

    bool AppendTypedDetails(
        const FMcpMaterialGraphOwner& Owner,
        UMaterialExpression* Expression,
        const TSharedRef<FJsonObject>& Resp)
    {
        if (!Expression) return false;

        if (UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expression))
        {
            AppendCustomDetails(Owner, Custom, Resp);
            return true;
        }

        if (auto* SMA = Cast<UMaterialExpressionSetMaterialAttributes>(Expression))
        {
            AppendAttributeSetDetails(Owner, SMA, Resp);
            return true;
        }

        if (auto* GMA = Cast<UMaterialExpressionGetMaterialAttributes>(Expression))
        {
            AppendAttributeGetDetails(GMA, Resp);
            return true;
        }

        if (auto* Break = Cast<UMaterialExpressionBreakMaterialAttributes>(Expression))
        {
            AppendBreakMaterialAttributesDetails(Owner, Break, Resp);
            return true;
        }

        if (AppendParameterDetails(Expression, Resp))
        {
            return true;
        }

        if (UMaterialExpressionConstant* Const = Cast<UMaterialExpressionConstant>(Expression))
        {
            Resp->SetNumberField(TEXT("value"), Const->R);
            return true;
        }
        if (UMaterialExpressionConstant2Vector* Const2 = Cast<UMaterialExpressionConstant2Vector>(Expression))
        {
            TSharedPtr<FJsonObject> ValueObj = MakeShared<FJsonObject>();
            ValueObj->SetNumberField(TEXT("r"), Const2->R);
            ValueObj->SetNumberField(TEXT("g"), Const2->G);
            Resp->SetObjectField(TEXT("value"), ValueObj);
            return true;
        }
        if (UMaterialExpressionConstant3Vector* Const3 = Cast<UMaterialExpressionConstant3Vector>(Expression))
        {
            TSharedPtr<FJsonObject> ValueObj = MakeShared<FJsonObject>();
            ValueObj->SetNumberField(TEXT("r"), Const3->Constant.R);
            ValueObj->SetNumberField(TEXT("g"), Const3->Constant.G);
            ValueObj->SetNumberField(TEXT("b"), Const3->Constant.B);
            Resp->SetObjectField(TEXT("value"), ValueObj);
            return true;
        }
        if (UMaterialExpressionConstant4Vector* Const4 = Cast<UMaterialExpressionConstant4Vector>(Expression))
        {
            TSharedPtr<FJsonObject> ValueObj = MakeShared<FJsonObject>();
            ValueObj->SetNumberField(TEXT("r"), Const4->Constant.R);
            ValueObj->SetNumberField(TEXT("g"), Const4->Constant.G);
            ValueObj->SetNumberField(TEXT("b"), Const4->Constant.B);
            ValueObj->SetNumberField(TEXT("a"), Const4->Constant.A);
            Resp->SetObjectField(TEXT("value"), ValueObj);
            return true;
        }
        if (UMaterialExpressionTextureSample* TexSample = Cast<UMaterialExpressionTextureSample>(Expression))
        {
            if (TexSample->Texture)
            {
                Resp->SetStringField(TEXT("texture"), TexSample->Texture->GetPathName());
                Resp->SetStringField(TEXT("textureName"), TexSample->Texture->GetName());
            }
            return true;
        }
        if (UMaterialExpressionLandscapeLayerWeight* LayerWeight = Cast<UMaterialExpressionLandscapeLayerWeight>(Expression))
        {
            Resp->SetStringField(TEXT("parameterName"), LayerWeight->ParameterName.ToString());
            Resp->SetNumberField(TEXT("previewWeight"), LayerWeight->PreviewWeight);

            TSharedPtr<FJsonObject> BaseObj = MakeShared<FJsonObject>();
            ::McpEmitInputPinJson(Owner, &LayerWeight->Base, TEXT("Base"), BaseObj.ToSharedRef());
            Resp->SetObjectField(TEXT("baseInput"), BaseObj);

            TSharedPtr<FJsonObject> LayerObj = MakeShared<FJsonObject>();
            ::McpEmitInputPinJson(Owner, &LayerWeight->Layer, TEXT("Layer"), LayerObj.ToSharedRef());
            Resp->SetObjectField(TEXT("layerInput"), LayerObj);
            return true;
        }
        if (UMaterialExpressionLandscapeLayerBlend* LayerBlend = Cast<UMaterialExpressionLandscapeLayerBlend>(Expression))
        {
            TArray<TSharedPtr<FJsonValue>> LayersArray;
            for (const FLayerBlendInput& Layer : LayerBlend->Layers)
            {
                TSharedPtr<FJsonObject> LayerObj = MakeShared<FJsonObject>();
                LayerObj->SetStringField(TEXT("name"), Layer.LayerName.ToString());
                LayerObj->SetStringField(TEXT("blendType"), ::McpLandscapeBlendTypeToString(Layer.BlendType));
                LayerObj->SetNumberField(TEXT("previewWeight"), Layer.PreviewWeight);
                LayerObj->SetNumberField(TEXT("constHeightInput"), Layer.ConstHeightInput);

                TSharedPtr<FJsonObject> ConstLayerInput = MakeShared<FJsonObject>();
                ConstLayerInput->SetNumberField(TEXT("x"), Layer.ConstLayerInput.X);
                ConstLayerInput->SetNumberField(TEXT("y"), Layer.ConstLayerInput.Y);
                ConstLayerInput->SetNumberField(TEXT("z"), Layer.ConstLayerInput.Z);
                LayerObj->SetObjectField(TEXT("constLayerInput"), ConstLayerInput);

                TSharedPtr<FJsonObject> LayerInputObj = MakeShared<FJsonObject>();
                ::McpEmitInputPinJson(Owner, &Layer.LayerInput, TEXT("LayerInput"), LayerInputObj.ToSharedRef());
                LayerObj->SetObjectField(TEXT("layerInput"), LayerInputObj);

                TSharedPtr<FJsonObject> HeightInputObj = MakeShared<FJsonObject>();
                ::McpEmitInputPinJson(Owner, &Layer.HeightInput, TEXT("HeightInput"), HeightInputObj.ToSharedRef());
                LayerObj->SetObjectField(TEXT("heightInput"), HeightInputObj);

                LayersArray.Add(MakeShared<FJsonValueObject>(LayerObj));
            }
            Resp->SetArrayField(TEXT("layers"), LayersArray);
            return true;
        }
        if (UMaterialExpressionMaterialFunctionCall* FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
        {
            if (FuncCall->MaterialFunction)
            {
                Resp->SetStringField(TEXT("functionPath"), FuncCall->MaterialFunction->GetPathName());
                Resp->SetStringField(TEXT("functionName"), FuncCall->MaterialFunction->GetName());
            }

            TArray<TSharedPtr<FJsonValue>> FunctionInputs;
            for (int32 InputIndex = 0; InputIndex < FuncCall->FunctionInputs.Num(); ++InputIndex)
            {
                const FFunctionExpressionInput& FunctionInput = FuncCall->FunctionInputs[InputIndex];
                TSharedPtr<FJsonObject> InputObj = MakeShared<FJsonObject>();
                InputObj->SetNumberField(TEXT("index"), InputIndex);
                if (FunctionInput.ExpressionInput)
                {
                    InputObj->SetStringField(TEXT("functionInputId"), FunctionInput.ExpressionInput->Id.ToString());
                }
                ::McpEmitInputPinJson(Owner, &FunctionInput.Input, FuncCall->GetInputName(InputIndex).ToString(), InputObj.ToSharedRef());
                FunctionInputs.Add(MakeShared<FJsonValueObject>(InputObj));
            }
            Resp->SetArrayField(TEXT("functionInputs"), FunctionInputs);

            TArray<TSharedPtr<FJsonValue>> FunctionOutputs;
            for (int32 OutputIndex = 0; OutputIndex < FuncCall->FunctionOutputs.Num(); ++OutputIndex)
            {
                const FFunctionExpressionOutput& FunctionOutput = FuncCall->FunctionOutputs[OutputIndex];
                TSharedPtr<FJsonObject> OutputObj = MakeShared<FJsonObject>();
                OutputObj->SetNumberField(TEXT("index"), OutputIndex);
                OutputObj->SetStringField(TEXT("functionOutputId"), FunctionOutput.ExpressionOutputId.ToString());
                bool bResolvedOutputName = false;
                FString OutputName = ::McpGetOutputName(Expression, OutputIndex, bResolvedOutputName);
                if (OutputName.IsEmpty() && FunctionOutput.ExpressionOutput)
                {
                    OutputName = FunctionOutput.ExpressionOutput->OutputName.ToString();
                    bResolvedOutputName = !OutputName.IsEmpty();
                }
                if (!OutputName.IsEmpty())
                {
                    OutputObj->SetStringField(TEXT("name"), OutputName);
                }
                OutputObj->SetBoolField(TEXT("nameResolved"), bResolvedOutputName);
                FunctionOutputs.Add(MakeShared<FJsonValueObject>(OutputObj));
            }
            Resp->SetArrayField(TEXT("functionOutputs"), FunctionOutputs);
            return true;
        }
        if (UMaterialExpressionNamedRerouteDeclaration* Declaration = Cast<UMaterialExpressionNamedRerouteDeclaration>(Expression))
        {
            Resp->SetStringField(TEXT("rerouteName"), Declaration->Name.ToString());
            Resp->SetStringField(TEXT("rerouteGuid"), Declaration->VariableGuid.ToString());
            TSharedPtr<FJsonObject> InputObj = MakeShared<FJsonObject>();
            ::McpEmitInputPinJson(Owner, &Declaration->Input, TEXT("Input"), InputObj.ToSharedRef());
            Resp->SetObjectField(TEXT("declarationInput"), InputObj);
            AppendRerouteDeclarationUsages(Owner, Declaration, Resp);
            return true;
        }
        if (UMaterialExpressionNamedRerouteUsage* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(Expression))
        {
            Resp->SetStringField(TEXT("declarationGuid"), Usage->DeclarationGuid.ToString());
            if (Usage->Declaration)
            {
                Resp->SetObjectField(TEXT("declaration"), ::McpBuildExpressionRef(Owner, Usage->Declaration));
                Resp->SetStringField(TEXT("declarationName"), Usage->Declaration->Name.ToString());
            }
            return true;
        }
        if (UMaterialExpressionLandscapePhysicalMaterialOutput* PhysicalOutput = Cast<UMaterialExpressionLandscapePhysicalMaterialOutput>(Expression))
        {
            TArray<TSharedPtr<FJsonValue>> Inputs;
            for (int32 InputIndex = 0; InputIndex < PhysicalOutput->Inputs.Num(); ++InputIndex)
            {
                const FPhysicalMaterialInput& Input = PhysicalOutput->Inputs[InputIndex];
                TSharedPtr<FJsonObject> InputObj = MakeShared<FJsonObject>();
                InputObj->SetNumberField(TEXT("index"), InputIndex);
                if (Input.PhysicalMaterial)
                {
                    InputObj->SetStringField(TEXT("physicalMaterial"), Input.PhysicalMaterial->GetPathName());
                }
                ::McpEmitInputPinJson(Owner, &Input.Input, FString::Printf(TEXT("Input%d"), InputIndex), InputObj.ToSharedRef());
                Inputs.Add(MakeShared<FJsonValueObject>(InputObj));
            }
            Resp->SetArrayField(TEXT("physicalMaterialInputs"), Inputs);
            return true;
        }
        if (UMaterialExpressionFunctionInput* FuncIn = Cast<UMaterialExpressionFunctionInput>(Expression))
        {
            Resp->SetStringField(TEXT("inputType"),                ::McpFunctionInputTypeName(FuncIn->InputType));
            Resp->SetStringField(TEXT("description"),              FuncIn->Description);
            Resp->SetNumberField(TEXT("sortPriority"),             FuncIn->SortPriority);
            Resp->SetBoolField  (TEXT("usePreviewValueAsDefault"), FuncIn->bUsePreviewValueAsDefault);

            TSharedRef<FJsonObject> Pv = MakeShared<FJsonObject>();
            Pv->SetNumberField(TEXT("x"), FuncIn->PreviewValue.X);
            Pv->SetNumberField(TEXT("y"), FuncIn->PreviewValue.Y);
            Pv->SetNumberField(TEXT("z"), FuncIn->PreviewValue.Z);
            Pv->SetNumberField(TEXT("w"), FuncIn->PreviewValue.W);
            Resp->SetObjectField(TEXT("previewValue"), Pv);
            return true;
        }
        if (UMaterialExpressionFunctionOutput* FuncOut = Cast<UMaterialExpressionFunctionOutput>(Expression))
        {
            Resp->SetStringField(TEXT("description"),  FuncOut->Description);
            Resp->SetNumberField(TEXT("sortPriority"), FuncOut->SortPriority);
            return true;
        }
        return false;
    }

    void AppendCustomDetails(const FMcpMaterialGraphOwner& Owner, UMaterialExpressionCustom* Custom, const TSharedRef<FJsonObject>& Resp)
    {
        if (!Custom) return;

        Resp->SetStringField(TEXT("code"), Custom->Code);
        Resp->SetStringField(TEXT("outputType"),
            StaticEnum<ECustomMaterialOutputType>()
                ? StaticEnum<ECustomMaterialOutputType>()->GetNameStringByValue(static_cast<int64>(Custom->OutputType))
                : FString());

        TArray<TSharedPtr<FJsonValue>> Inputs;
        for (FCustomInput& CI : Custom->Inputs)
        {
            TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("name"), CI.InputName.ToString());
            ::McpEmitInputPinJson(Owner, &CI.Input, CI.InputName.ToString(), Item);
            Inputs.Add(MakeShared<FJsonValueObject>(Item));
        }
        Resp->SetArrayField(TEXT("inputs"), Inputs);

        TArray<TSharedPtr<FJsonValue>> AdditionalOutputs;
        for (const FCustomOutput& CO : Custom->AdditionalOutputs)
        {
            TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("name"), CO.OutputName.ToString());
            Item->SetStringField(TEXT("outputType"),
                StaticEnum<ECustomMaterialOutputType>()
                    ? StaticEnum<ECustomMaterialOutputType>()->GetNameStringByValue(static_cast<int64>(CO.OutputType))
                    : FString());
            AdditionalOutputs.Add(MakeShared<FJsonValueObject>(Item));
        }
        Resp->SetArrayField(TEXT("additionalOutputs"), AdditionalOutputs);

        TArray<TSharedPtr<FJsonValue>> IncludePaths;
        for (const FString& Path : Custom->IncludeFilePaths)
        {
            IncludePaths.Add(MakeShared<FJsonValueString>(Path));
        }
        Resp->SetArrayField(TEXT("includeFilePaths"), IncludePaths);

        TArray<TSharedPtr<FJsonValue>> Defines;
        for (const FCustomDefine& D : Custom->AdditionalDefines)
        {
            TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("name"), D.DefineName);
            Item->SetStringField(TEXT("value"), D.DefineValue);
            Defines.Add(MakeShared<FJsonValueObject>(Item));
        }
        Resp->SetArrayField(TEXT("additionalDefines"), Defines);
    }
    bool AppendParameterDetails(UMaterialExpression* Expression, const TSharedRef<FJsonObject>& Resp)
    {
        if (!Expression) return false;

        if (auto* Tex2D = Cast<UMaterialExpressionTextureSampleParameter2D>(Expression))
        {
            Resp->SetStringField(TEXT("parameterName"), Tex2D->ParameterName.ToString());
            Resp->SetStringField(TEXT("group"), Tex2D->Group.ToString());
            Resp->SetNumberField(TEXT("sortPriority"), Tex2D->SortPriority);
            if (Tex2D->Texture)
            {
                Resp->SetStringField(TEXT("texture"), Tex2D->Texture->GetPathName());
            }
            else
            {
                Resp->SetField(TEXT("texture"), MakeShared<FJsonValueNull>());
            }
            Resp->SetStringField(TEXT("samplerType"),
                StaticEnum<EMaterialSamplerType>()
                    ? StaticEnum<EMaterialSamplerType>()->GetNameStringByValue(static_cast<int64>(Tex2D->SamplerType))
                    : FString());
            return true;
        }
        if (auto* TexParam = Cast<UMaterialExpressionTextureSampleParameter>(Expression))
        {
            Resp->SetStringField(TEXT("parameterName"), TexParam->ParameterName.ToString());
            Resp->SetStringField(TEXT("group"), TexParam->Group.ToString());
            Resp->SetNumberField(TEXT("sortPriority"), TexParam->SortPriority);
            if (TexParam->Texture)
            {
                Resp->SetStringField(TEXT("texture"), TexParam->Texture->GetPathName());
            }
            else
            {
                Resp->SetField(TEXT("texture"), MakeShared<FJsonValueNull>());
            }
            Resp->SetStringField(TEXT("samplerType"),
                StaticEnum<EMaterialSamplerType>()
                    ? StaticEnum<EMaterialSamplerType>()->GetNameStringByValue(static_cast<int64>(TexParam->SamplerType))
                    : FString());
            return true;
        }
        if (auto* TexObjParam = Cast<UMaterialExpressionTextureObjectParameter>(Expression))
        {
            Resp->SetStringField(TEXT("parameterName"), TexObjParam->ParameterName.ToString());
            Resp->SetStringField(TEXT("group"), TexObjParam->Group.ToString());
            Resp->SetNumberField(TEXT("sortPriority"), TexObjParam->SortPriority);
            if (TexObjParam->Texture)
            {
                Resp->SetStringField(TEXT("texture"), TexObjParam->Texture->GetPathName());
            }
            else
            {
                Resp->SetField(TEXT("texture"), MakeShared<FJsonValueNull>());
            }
            return true;
        }
        if (auto* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expression))
        {
            Resp->SetStringField(TEXT("parameterName"), ScalarParam->ParameterName.ToString());
            Resp->SetNumberField(TEXT("defaultValue"), ScalarParam->DefaultValue);
            Resp->SetNumberField(TEXT("sliderMin"), ScalarParam->SliderMin);
            Resp->SetNumberField(TEXT("sliderMax"), ScalarParam->SliderMax);
            Resp->SetStringField(TEXT("group"), ScalarParam->Group.ToString());
            Resp->SetNumberField(TEXT("sortPriority"), ScalarParam->SortPriority);
            return true;
        }
        if (auto* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expression))
        {
            Resp->SetStringField(TEXT("parameterName"), VectorParam->ParameterName.ToString());
            TSharedPtr<FJsonObject> DefaultObj = MakeShared<FJsonObject>();
            DefaultObj->SetNumberField(TEXT("r"), VectorParam->DefaultValue.R);
            DefaultObj->SetNumberField(TEXT("g"), VectorParam->DefaultValue.G);
            DefaultObj->SetNumberField(TEXT("b"), VectorParam->DefaultValue.B);
            DefaultObj->SetNumberField(TEXT("a"), VectorParam->DefaultValue.A);
            Resp->SetObjectField(TEXT("defaultValue"), DefaultObj);
            Resp->SetStringField(TEXT("group"), VectorParam->Group.ToString());
            Resp->SetNumberField(TEXT("sortPriority"), VectorParam->SortPriority);
            return true;
        }
        if (auto* SwitchParam = Cast<UMaterialExpressionStaticSwitchParameter>(Expression))
        {
            Resp->SetStringField(TEXT("parameterName"), SwitchParam->ParameterName.ToString());
            Resp->SetBoolField(TEXT("defaultValue"), SwitchParam->DefaultValue);
            Resp->SetStringField(TEXT("group"), SwitchParam->Group.ToString());
            Resp->SetNumberField(TEXT("sortPriority"), SwitchParam->SortPriority);
            return true;
        }
        return false;
    }
    void AppendAttributeSetDetails(
        const FMcpMaterialGraphOwner& Owner,
        UMaterialExpressionSetMaterialAttributes* Set,
        const TSharedRef<FJsonObject>& Resp)
    {
        if (!Set) return;

        TArray<TSharedPtr<FJsonValue>> Items;
        for (int32 i = 0; i < Set->AttributeSetTypes.Num(); ++i)
        {
            const FGuid& Guid = Set->AttributeSetTypes[i];
            TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("guid"), Guid.ToString());
            Item->SetStringField(TEXT("attribute"), FMaterialAttributeDefinitionMap::GetAttributeName(Guid));

            // Inputs[0] is the carry-over "MaterialAttributes" pin; AttributeSetTypes[i] matches Inputs[i+1].
            const int32 InputIndex = i + 1;
            if (Set->Inputs.IsValidIndex(InputIndex))
            {
                const FExpressionInput& In = Set->Inputs[InputIndex];
                const bool bConnected = In.Expression != nullptr;
                Item->SetBoolField(TEXT("connected"), bConnected);
                if (bConnected)
                {
                    ::McpEmitInputPinJson(Owner, &In, FMaterialAttributeDefinitionMap::GetAttributeName(Guid), Item.ToSharedRef());
                }
            }
            else
            {
                Item->SetBoolField(TEXT("connected"), false);
            }
            Items.Add(MakeShared<FJsonValueObject>(Item));
        }
        Resp->SetArrayField(TEXT("attributeSetTypes"), Items);
    }
    void AppendAttributeGetDetails(
        UMaterialExpressionGetMaterialAttributes* Get,
        const TSharedRef<FJsonObject>& Resp)
    {
        if (!Get) return;
        TArray<TSharedPtr<FJsonValue>> Items;
        for (const FGuid& Guid : Get->AttributeGetTypes)
        {
            TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("guid"), Guid.ToString());
            Item->SetStringField(TEXT("attribute"), FMaterialAttributeDefinitionMap::GetAttributeName(Guid));
            Items.Add(MakeShared<FJsonValueObject>(Item));
        }
        Resp->SetArrayField(TEXT("attributeGetTypes"), Items);
    }
    void AppendBreakMaterialAttributesDetails(
        const FMcpMaterialGraphOwner& Owner,
        UMaterialExpressionBreakMaterialAttributes* Break,
        const TSharedRef<FJsonObject>& Resp)
    {
        if (!Break) return;

        // Resolve the Break node's outputs once; each entry corresponds to one attribute pin
        // (BaseColor, Metallic, Specular, Roughness, Normal, EmissiveColor, ...).
        const TArray<FExpressionOutput>& BreakOutputs = Break->GetOutputs();

        // Pre-fetch the graph's expression list so consumers can be discovered by walking
        // every other expression's inputs and matching Input->Expression == Break with
        // Input->OutputIndex against this entry's index.
        const TArray<TObjectPtr<UMaterialExpression>>* AllExprs = McpGetGraphExpressions(Owner);

        // Build attribute name -> GUID lookup once by walking the ordered attribute list
        // and asking GetAttributeName for each GUID. FMaterialAttributeDefinitionMap::GetID
        // takes EMaterialProperty (not name) and the name-to-id list helper is private.
        TMap<FString, FGuid> NameToId;
        const TArray<FGuid>& OrderedIds = FMaterialAttributeDefinitionMap::GetOrderedVisibleAttributeList();
        NameToId.Reserve(OrderedIds.Num());
        for (const FGuid& Id : OrderedIds)
        {
            const FString Name = FMaterialAttributeDefinitionMap::GetAttributeName(Id);
            if (!Name.IsEmpty())
            {
                NameToId.Add(Name, Id);
            }
        }

        TArray<TSharedPtr<FJsonValue>> Items;
        for (int32 OutputIndex = 0; OutputIndex < BreakOutputs.Num(); ++OutputIndex)
        {
            const FExpressionOutput& Output = BreakOutputs[OutputIndex];
            const FString AttrName = Output.OutputName.ToString();

            TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();

            // Resolve the matching attribute GUID by name; BreakMaterialAttributes outputs
            // are named after the attributes (BaseColor, Roughness, ...).
            FGuid AttrGuid;
            if (const FGuid* Found = NameToId.Find(AttrName))
            {
                AttrGuid = *Found;
            }
            Item->SetStringField(TEXT("guid"),       AttrGuid.IsValid() ? AttrGuid.ToString() : FString());
            Item->SetStringField(TEXT("attribute"),  AttrName);
            Item->SetNumberField(TEXT("outputIndex"), OutputIndex);

            // Scan the graph for any input whose source is this Break with this OutputIndex.
            TArray<TSharedPtr<FJsonValue>> Consumers;
            if (AllExprs)
            {
                for (int32 ExprIdx = 0; ExprIdx < AllExprs->Num(); ++ExprIdx)
                {
                    UMaterialExpression* Other = (*AllExprs)[ExprIdx];
                    if (!Other || Other == Break) continue;

                    int32 InputIdx = 0;
                    for (FExpressionInputIterator It(Other); It; ++It, ++InputIdx)
                    {
                        FExpressionInput* In = It.Input;
                        if (!In || In->Expression != Break) continue;
                        if (In->OutputIndex != OutputIndex) continue;

                        TSharedRef<FJsonObject> Consumer = MakeShared<FJsonObject>();
                        Consumer->SetObjectField(TEXT("expression"), ::McpBuildExpressionRef(Owner, Other));
                        Consumer->SetStringField(TEXT("inputName"), Other->GetInputName(InputIdx).ToString());
                        Consumer->SetNumberField(TEXT("inputIndex"), InputIdx);
                        Consumers.Add(MakeShared<FJsonValueObject>(Consumer));
                    }
                }
            }
            Item->SetArrayField(TEXT("consumers"), Consumers);

            Items.Add(MakeShared<FJsonValueObject>(Item));
        }
        Resp->SetArrayField(TEXT("attributeBreakTypes"), Items);
    }

    void AppendRerouteDeclarationUsages(
        const FMcpMaterialGraphOwner& Owner,
        UMaterialExpressionNamedRerouteDeclaration* Decl,
        const TSharedRef<FJsonObject>& Resp)
    {
        if (!Decl) return;
        const TArray<TObjectPtr<UMaterialExpression>>* AllPtr = McpGetGraphExpressions(Owner);
        if (!AllPtr) return;

        TArray<TSharedPtr<FJsonValue>> Usages;
        for (int32 Index = 0; Index < AllPtr->Num(); ++Index)
        {
            UMaterialExpression* Expr = (*AllPtr)[Index];
            if (auto* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(Expr))
            {
                if (Usage->DeclarationGuid == Decl->VariableGuid)
                {
                    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
                    Item->SetNumberField(TEXT("index"), Index);
                    Item->SetStringField(TEXT("guid"), Usage->MaterialExpressionGuid.ToString());
                    Item->SetStringField(TEXT("expressionPath"), Usage->GetPathName());
                    Usages.Add(MakeShared<FJsonValueObject>(Item));
                }
            }
        }
        Resp->SetArrayField(TEXT("usages"), Usages);
    }
}
