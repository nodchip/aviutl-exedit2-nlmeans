@echo off
setlocal

if exist nlmeans_filter\aviutl2_sdk\filter2.h (
  cl /nologo /utf-8 /EHsc ^
    /std:c++17 ^
    /wd4828 ^
    /I third_party\googletest\googletest\include ^
    /I third_party\googletest\googletest ^
    /I nlmeans_filter ^
    third_party\googletest\googletest\src\gtest-all.cc ^
    third_party\googletest\googletest\src\gtest_main.cc ^
    nlmeans_filter\tests\BackendSelectionTests.cpp ^
    nlmeans_filter\tests\ExecutionPolicyTests.cpp ^
    nlmeans_filter\tests\FastModeConfigTests.cpp ^
    nlmeans_filter\tests\FastNlmTests.cpp ^
    nlmeans_filter\tests\GpuAdapterSelectionTests.cpp ^
    nlmeans_filter\tests\GpuFallbackExecutionTests.cpp ^
    nlmeans_filter\tests\GpuFallbackPolicyTests.cpp ^
    nlmeans_filter\tests\GpuQualityComparisonTests.cpp ^
    nlmeans_filter\tests\GpuRunnerDispatchTests.cpp ^
    nlmeans_filter\tests\ModeIdsGoogleTest.cpp ^
    nlmeans_filter\tests\NlmFrameOutputMultiCaseTests.cpp ^
    nlmeans_filter\tests\NlmFrameOutputTests.cpp ^
    nlmeans_filter\tests\NlmFrameReferenceTests.cpp ^
    nlmeans_filter\tests\NlmKernelTests.cpp ^
    nlmeans_filter\tests\ProcessingRoutePolicyTests.cpp ^
    nlmeans_filter\tests\UiSelectionRouteTests.cpp ^
    nlmeans_filter\tests\UiToDispatcherIntegrationTests.cpp ^
    nlmeans_filter\tests\TemporalNlmTests.cpp ^
    nlmeans_filter\tests\VideoProcessingDispatcherTests.cpp ^
    nlmeans_filter\exedit2\Exedit2GpuRunner.cpp ^
    /Fe:GoogleTests.exe ^
    /link dxgi.lib
) else (
  cl /nologo /utf-8 /EHsc ^
    /std:c++17 ^
    /wd4828 ^
    /I third_party\googletest\googletest\include ^
    /I third_party\googletest\googletest ^
    /I nlmeans_filter ^
    third_party\googletest\googletest\src\gtest-all.cc ^
    third_party\googletest\googletest\src\gtest_main.cc ^
    nlmeans_filter\tests\BackendSelectionTests.cpp ^
    nlmeans_filter\tests\ExecutionPolicyTests.cpp ^
    nlmeans_filter\tests\FastModeConfigTests.cpp ^
    nlmeans_filter\tests\FastNlmTests.cpp ^
    nlmeans_filter\tests\GpuAdapterSelectionTests.cpp ^
    nlmeans_filter\tests\GpuFallbackExecutionTests.cpp ^
    nlmeans_filter\tests\GpuFallbackPolicyTests.cpp ^
    nlmeans_filter\tests\GpuQualityComparisonTests.cpp ^
    nlmeans_filter\tests\GpuRunnerDispatchTests.cpp ^
    nlmeans_filter\tests\ModeIdsGoogleTest.cpp ^
    nlmeans_filter\tests\NlmFrameOutputMultiCaseTests.cpp ^
    nlmeans_filter\tests\NlmFrameOutputTests.cpp ^
    nlmeans_filter\tests\NlmFrameReferenceTests.cpp ^
    nlmeans_filter\tests\NlmKernelTests.cpp ^
    nlmeans_filter\tests\ProcessingRoutePolicyTests.cpp ^
    nlmeans_filter\tests\UiSelectionRouteTests.cpp ^
    nlmeans_filter\tests\UiToDispatcherIntegrationTests.cpp ^
    nlmeans_filter\tests\TemporalNlmTests.cpp ^
    nlmeans_filter\tests\VideoProcessingDispatcherTests.cpp ^
    /Fe:GoogleTests.exe
)
if errorlevel 1 exit /b 1

GoogleTests.exe
if errorlevel 1 exit /b 1

exit /b 0
