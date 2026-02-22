@echo off
setlocal

cl /nologo /utf-8 /EHsc ^
  /std:c++17 ^
  /I third_party\googletest\googletest\include ^
  /I third_party\googletest\googletest ^
  /I nlmeans_filter ^
  third_party\googletest\googletest\src\gtest-all.cc ^
  third_party\googletest\googletest\src\gtest_main.cc ^
  nlmeans_filter\tests\BackendSelectionTests.cpp ^
  nlmeans_filter\tests\ExecutionPolicyTests.cpp ^
  nlmeans_filter\tests\GpuAdapterSelectionTests.cpp ^
  nlmeans_filter\tests\GpuFallbackExecutionTests.cpp ^
  nlmeans_filter\tests\GpuFallbackPolicyTests.cpp ^
  nlmeans_filter\tests\GpuRunnerDispatchTests.cpp ^
  nlmeans_filter\tests\ModeIdsGoogleTest.cpp ^
  nlmeans_filter\tests\NlmFrameOutputMultiCaseTests.cpp ^
  nlmeans_filter\tests\NlmFrameOutputTests.cpp ^
  nlmeans_filter\tests\NlmFrameReferenceTests.cpp ^
  nlmeans_filter\tests\NlmKernelTests.cpp ^
  nlmeans_filter\tests\ProcessingRoutePolicyTests.cpp ^
  nlmeans_filter\tests\UiSelectionRouteTests.cpp ^
  nlmeans_filter\tests\UiToDispatcherIntegrationTests.cpp ^
  nlmeans_filter\tests\VideoProcessingDispatcherTests.cpp ^
  /Fe:GoogleTests.exe
if errorlevel 1 exit /b 1

GoogleTests.exe
if errorlevel 1 exit /b 1

exit /b 0
