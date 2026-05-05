// McpAutomationBridge_MaterialExpressionDetails.cpp
#include "McpVersionCompatibility.h"  // MUST be first

#include "McpAutomationBridge_MaterialExpressionDetails.h"
#include "McpAutomationBridgeGlobals.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
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
    bool AppendTypedDetails(const FMcpMaterialGraphOwner&, UMaterialExpression*, const TSharedRef<FJsonObject>&)
    {
        return false; // implementations land in subsequent tasks
    }

    void AppendCustomDetails(UMaterialExpressionCustom*, const TSharedRef<FJsonObject>&) {}
    bool AppendParameterDetails(UMaterialExpression*, const TSharedRef<FJsonObject>&) { return false; }
    void AppendAttributeSetDetails(const FMcpMaterialGraphOwner&, UMaterialExpressionSetMaterialAttributes*, const TSharedRef<FJsonObject>&) {}
    void AppendAttributeGetDetails(UMaterialExpressionGetMaterialAttributes*, const TSharedRef<FJsonObject>&) {}
    void AppendRerouteDeclarationUsages(const FMcpMaterialGraphOwner&, UMaterialExpressionNamedRerouteDeclaration*, const TSharedRef<FJsonObject>&) {}
}
