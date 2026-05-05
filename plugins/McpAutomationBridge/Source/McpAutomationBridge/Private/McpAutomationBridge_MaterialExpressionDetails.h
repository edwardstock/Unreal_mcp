// McpAutomationBridge_MaterialExpressionDetails.h
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UMaterialExpression;
class UMaterialExpressionCustom;
class UMaterialExpressionSetMaterialAttributes;
class UMaterialExpressionGetMaterialAttributes;
class UMaterialExpressionNamedRerouteDeclaration;
struct FMcpMaterialGraphOwner;

namespace McpMaterialExpressionDetails
{
    // Append type-specific JSON fields for `Expression` to `Out`.
    // Returns true if the expression's class matched a known emitter.
    bool AppendTypedDetails(
        const FMcpMaterialGraphOwner& Owner,
        UMaterialExpression* Expression,
        const TSharedRef<FJsonObject>& Out);

    // Specialised single-type appenders, exposed for N1/N2/N3 thin actions.
    void AppendCustomDetails(
        UMaterialExpressionCustom* Custom,
        const TSharedRef<FJsonObject>& Out);

    // Dispatches by most-derived class (TextureSampleParameter2D / TextureSampleParameter / ...).
    bool AppendParameterDetails(
        UMaterialExpression* Expression,
        const TSharedRef<FJsonObject>& Out);

    void AppendAttributeSetDetails(
        const FMcpMaterialGraphOwner& Owner,
        UMaterialExpressionSetMaterialAttributes* Set,
        const TSharedRef<FJsonObject>& Out);

    void AppendAttributeGetDetails(
        UMaterialExpressionGetMaterialAttributes* Get,
        const TSharedRef<FJsonObject>& Out);

    void AppendRerouteDeclarationUsages(
        const FMcpMaterialGraphOwner& Owner,
        UMaterialExpressionNamedRerouteDeclaration* Decl,
        const TSharedRef<FJsonObject>& Out);
}
