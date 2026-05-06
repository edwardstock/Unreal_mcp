// File: Plugins/Unreal_mcp/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Tests/McpMaterialPureUnitTests.cpp
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionScalarParameter.h"

// Pure-function tests only. No mock socket, no subsystem invocation.
// Anything handler-level is verified via live MCP probes documented in the plan.

#endif // WITH_DEV_AUTOMATION_TESTS
