#pragma once
#include "CoreMinimal.h"

class UClass;

/** Static catalog of registered UMaterialExpression subclasses, built lazily on first Get().
 *  Used by:
 *  - add_material_nodes / update_material_nodes (nodeType resolution + applicability validation)
 *  - list_material_expression_classes (discovery action)
 *
 *  Applicable fields are the union of:
 *  (a) MCP-semantic fields per spec section 7.2 (parameterName, defaultValue, ...) hardcoded per
 *      class, mapped to underlying UE properties by handler-side apply functions.
 *  (b) Class-specific fields auto-discovered via UProperty reflection over each UMaterialExpression
 *      subclass (e.g., worldPositionShaderOffset on MaterialExpressionWorldPosition).
 *  Enum properties additionally surface their valid values in FieldEnums (with per-enum
 *  prefix stripped at first underscore - e.g., "WPT_Default" -> "Default"). */
class FMcpMaterialExpressionCatalog
{
public:
    static const FMcpMaterialExpressionCatalog& Get();

    /** Resolves nodeType by exact match against UClass::GetName(). nullptr if unknown. */
    UClass* ResolveClassByExactName(const FString& Name) const;

    /** Tries exact match, then auto-prefix with "MaterialExpression". Sets bOutAutoPrefixed=true
     *  only when the prefixed form was used. nullptr if neither matches. */
    UClass* ResolveClassWithAutoPrefix(const FString& Name, bool& bOutAutoPrefixed) const;

    /** Returns up to MaxCount class names ordered by Levenshtein distance to Query. */
    TArray<FString> SuggestNames(const FString& Query, int32 MaxCount) const;

    /** Returns the union of MCP-semantic and reflection-discovered applicable fields for the class.
     *  Returns nullptr if the class is unknown. */
    const TArray<FString>* GetApplicableFields(const FString& ClassName) const;

    /** Valid enum values for (className, fieldName). nullptr if non-enum / unknown.
     *  Values are stripped of per-enum prefixes (e.g., "WPT_Default" -> "Default"). */
    const TArray<FString>* GetFieldEnumValues(const FString& ClassName, const FString& FieldName) const;

    /** UE property name (PascalCase) for a reflected catalog field (camelCase).
     *  Empty string if the field is one of the MCP-semantic ones (handler-side apply functions
     *  map those to UE properties via switch-on-class). */
    FString GetUePropertyName(const FString& ClassName, const FString& FieldName) const;

    /** Menu categories from the class's MenuCategories metadata. nullptr if unknown. */
    const TArray<FString>* GetCategories(const FString& ClassName) const;

    /** Full sorted list of registered class names (canonical form). */
    const TArray<FString>& GetAllClasses() const { return AllClassNames; }

    /** Description blurb pulled from class ToolTip metadata, may be empty. */
    FString GetDescription(const FString& ClassName) const;

private:
    FMcpMaterialExpressionCatalog();
    void Build();

    TMap<FString, UClass*>                          NameToClass;
    TMap<FString, TArray<FString>>                  NameToCategories;
    TMap<FString, TArray<FString>>                  NameToApplicableFields;
    TMap<FString, TMap<FString, TArray<FString>>>   NameToFieldEnums;
    TMap<FString, TMap<FString, FString>>           NameToFieldUeProp;
    TMap<FString, FString>                          NameToDescription;
    TArray<FString>                                 AllClassNames;
};
