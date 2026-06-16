// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_Material_Internal.h
//
// Shared internal types and helpers for the redesigned material graph mutation
// handlers (Tasks C.1 - C.5). The types live in a named namespace so multiple
// translation units (Material_GraphWrites.cpp, Material_CustomExpressions.cpp)
// can produce uniform per-item / per-connection error payloads, and so the
// identifier-resolution helper can be shared instead of duplicated.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

#include "McpAutomationBridgeHelpers.h"

#if WITH_EDITOR
class UMaterialExpression;
#endif

namespace McpMaterialInternal
{
    // Per-item node validation error payload.
    struct FMcpNodeValidationError
    {
        int32           Index = INDEX_NONE;
        FString         LocalId;
        FString         Field;
        FString         Code;
        FString         Message;
        TArray<FString> DidYouMean;
    };

    // Per-connection validation error payload.
    struct FMcpConnectionValidationError
    {
        int32   Index = INDEX_NONE;
        FString Field;
        FString Code;
        FString Message;
    };

    // Format helpers; produce the JSON value shape the protocol returns under
    // "errors[]" entries (scope, index, field, code, message, optional didYouMean).
    TSharedPtr<FJsonValue> McpFormatNodeError(const FMcpNodeValidationError& E);
    TSharedPtr<FJsonValue> McpFormatConnError(const FMcpConnectionValidationError& E);

    // Render an identifier JSON value as a printable string (used in result
    // payloads to echo the caller's input). Defined for both editor and non-
    // editor builds because it is pure JSON.
    FString McpFormatIdentifierForDisplay(const TSharedPtr<FJsonValue>& V);

#if WITH_EDITOR
    // Resolves a per-item identifier (number index or non-numeric string -
    // GUID, expression name, asset path, parameter name, desc) to an existing
    // graph expression. Mirrors the resolution rules used by update_material_nodes
    // (Task C.2). Fills OutError with the appropriate code/field/message and
    // didYouMean suggestions on miss.
    bool McpResolveUpdateIdentifier(
        const FMcpMaterialGraphOwner& Owner,
        const TSharedPtr<FJsonValue>& IdentifierJson,
        UMaterialExpression*& OutExpression,
        FMcpNodeValidationError& OutError);
#endif
}
