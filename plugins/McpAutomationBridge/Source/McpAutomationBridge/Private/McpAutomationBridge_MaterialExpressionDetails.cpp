// McpAutomationBridge_MaterialExpressionDetails.cpp
#include "McpVersionCompatibility.h"  // MUST be first

#include "McpAutomationBridge_MaterialExpressionDetails.h"
#include "McpAutomationBridgeGlobals.h"

#if WITH_EDITOR
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
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialExpressionLandscapeLayerWeight.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionLandscapePhysicalMaterialOutput.h"
#endif

#if WITH_EDITOR
extern void McpAddConnectedExpressionInfo(
    const FMcpMaterialGraphOwner& Owner,
    const struct FExpressionInput* Input,
    const TSharedRef<FJsonObject>& Obj);

extern FString McpLandscapeBlendTypeToString(ELandscapeLayerBlendType BlendType);

extern FString McpGetOutputName(UMaterialExpression* Expression, int32 OutputIndex, bool& bOutResolved);
extern TSharedPtr<FJsonObject> McpBuildExpressionRef(
    const FMcpMaterialGraphOwner& Owner,
    UMaterialExpression* Expression);
#endif

namespace McpMaterialExpressionDetails
{
    bool AppendTypedDetails(
        const FMcpMaterialGraphOwner& Owner,
        UMaterialExpression* Expression,
        const TSharedRef<FJsonObject>& Resp)
    {
        if (!Expression) return false;

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
        if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expression))
        {
            Resp->SetStringField(TEXT("parameterName"), ScalarParam->ParameterName.ToString());
            Resp->SetNumberField(TEXT("defaultValue"), ScalarParam->DefaultValue);
            return true;
        }
        if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expression))
        {
            Resp->SetStringField(TEXT("parameterName"), VectorParam->ParameterName.ToString());
            TSharedPtr<FJsonObject> DefaultObj = MakeShared<FJsonObject>();
            DefaultObj->SetNumberField(TEXT("r"), VectorParam->DefaultValue.R);
            DefaultObj->SetNumberField(TEXT("g"), VectorParam->DefaultValue.G);
            DefaultObj->SetNumberField(TEXT("b"), VectorParam->DefaultValue.B);
            DefaultObj->SetNumberField(TEXT("a"), VectorParam->DefaultValue.A);
            Resp->SetObjectField(TEXT("defaultValue"), DefaultObj);
            return true;
        }
        if (UMaterialExpressionStaticSwitchParameter* SwitchParam = Cast<UMaterialExpressionStaticSwitchParameter>(Expression))
        {
            Resp->SetStringField(TEXT("parameterName"), SwitchParam->ParameterName.ToString());
            Resp->SetBoolField(TEXT("defaultValue"), SwitchParam->DefaultValue);
            return true;
        }
        if (UMaterialExpressionLandscapeLayerWeight* LayerWeight = Cast<UMaterialExpressionLandscapeLayerWeight>(Expression))
        {
            Resp->SetStringField(TEXT("parameterName"), LayerWeight->ParameterName.ToString());
            Resp->SetNumberField(TEXT("previewWeight"), LayerWeight->PreviewWeight);

            TSharedPtr<FJsonObject> BaseObj = MakeShared<FJsonObject>();
            BaseObj->SetStringField(TEXT("name"), TEXT("Base"));
            ::McpAddConnectedExpressionInfo(Owner, &LayerWeight->Base, BaseObj.ToSharedRef());
            Resp->SetObjectField(TEXT("baseInput"), BaseObj);

            TSharedPtr<FJsonObject> LayerObj = MakeShared<FJsonObject>();
            LayerObj->SetStringField(TEXT("name"), TEXT("Layer"));
            ::McpAddConnectedExpressionInfo(Owner, &LayerWeight->Layer, LayerObj.ToSharedRef());
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
                ::McpAddConnectedExpressionInfo(Owner, &Layer.LayerInput, LayerInputObj.ToSharedRef());
                LayerObj->SetObjectField(TEXT("layerInput"), LayerInputObj);

                TSharedPtr<FJsonObject> HeightInputObj = MakeShared<FJsonObject>();
                ::McpAddConnectedExpressionInfo(Owner, &Layer.HeightInput, HeightInputObj.ToSharedRef());
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
                InputObj->SetStringField(TEXT("name"), FuncCall->GetInputName(InputIndex).ToString());
                if (FunctionInput.ExpressionInput)
                {
                    InputObj->SetStringField(TEXT("functionInputId"), FunctionInput.ExpressionInput->Id.ToString());
                }
                ::McpAddConnectedExpressionInfo(Owner, &FunctionInput.Input, InputObj.ToSharedRef());
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
            ::McpAddConnectedExpressionInfo(Owner, &Declaration->Input, InputObj.ToSharedRef());
            Resp->SetObjectField(TEXT("declarationInput"), InputObj);
            // usages[] backref is added by AppendRerouteDeclarationUsages in Phase B (R7).
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
                ::McpAddConnectedExpressionInfo(Owner, &Input.Input, InputObj.ToSharedRef());
                Inputs.Add(MakeShared<FJsonValueObject>(InputObj));
            }
            Resp->SetArrayField(TEXT("physicalMaterialInputs"), Inputs);
            return true;
        }
        return false;
    }

    void AppendCustomDetails(UMaterialExpressionCustom*, const TSharedRef<FJsonObject>&) {}
    bool AppendParameterDetails(UMaterialExpression*, const TSharedRef<FJsonObject>&) { return false; }
    void AppendAttributeSetDetails(const FMcpMaterialGraphOwner&, UMaterialExpressionSetMaterialAttributes*, const TSharedRef<FJsonObject>&) {}
    void AppendAttributeGetDetails(UMaterialExpressionGetMaterialAttributes*, const TSharedRef<FJsonObject>&) {}
    void AppendRerouteDeclarationUsages(const FMcpMaterialGraphOwner&, UMaterialExpressionNamedRerouteDeclaration*, const TSharedRef<FJsonObject>&) {}
}
