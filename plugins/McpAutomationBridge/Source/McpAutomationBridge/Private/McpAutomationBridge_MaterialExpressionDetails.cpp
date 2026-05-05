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
#endif

namespace McpMaterialExpressionDetails
{
    bool AppendTypedDetails(
        const FMcpMaterialGraphOwner& /*Owner*/,
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
        return false;
    }

    void AppendCustomDetails(UMaterialExpressionCustom*, const TSharedRef<FJsonObject>&) {}
    bool AppendParameterDetails(UMaterialExpression*, const TSharedRef<FJsonObject>&) { return false; }
    void AppendAttributeSetDetails(const FMcpMaterialGraphOwner&, UMaterialExpressionSetMaterialAttributes*, const TSharedRef<FJsonObject>&) {}
    void AppendAttributeGetDetails(UMaterialExpressionGetMaterialAttributes*, const TSharedRef<FJsonObject>&) {}
    void AppendRerouteDeclarationUsages(const FMcpMaterialGraphOwner&, UMaterialExpressionNamedRerouteDeclaration*, const TSharedRef<FJsonObject>&) {}
}
