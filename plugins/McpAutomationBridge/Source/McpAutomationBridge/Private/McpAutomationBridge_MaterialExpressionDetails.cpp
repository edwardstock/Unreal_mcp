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
#endif

#if WITH_EDITOR
extern void McpAddConnectedExpressionInfo(
    const FMcpMaterialGraphOwner& Owner,
    const struct FExpressionInput* Input,
    const TSharedRef<FJsonObject>& Obj);

extern FString McpLandscapeBlendTypeToString(ELandscapeLayerBlendType BlendType);
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
        return false;
    }

    void AppendCustomDetails(UMaterialExpressionCustom*, const TSharedRef<FJsonObject>&) {}
    bool AppendParameterDetails(UMaterialExpression*, const TSharedRef<FJsonObject>&) { return false; }
    void AppendAttributeSetDetails(const FMcpMaterialGraphOwner&, UMaterialExpressionSetMaterialAttributes*, const TSharedRef<FJsonObject>&) {}
    void AppendAttributeGetDetails(UMaterialExpressionGetMaterialAttributes*, const TSharedRef<FJsonObject>&) {}
    void AppendRerouteDeclarationUsages(const FMcpMaterialGraphOwner&, UMaterialExpressionNamedRerouteDeclaration*, const TSharedRef<FJsonObject>&) {}
}
