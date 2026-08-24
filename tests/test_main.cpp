#include <iostream>

int RunViewInjectionTests();
int RunAimProjectionTests();
int RunConfigTests();
int RunPoseGuardTests();
int RunExePathsTests();
int RunBuildProfileTests();

int main()
{
    std::cout << "Kingdom Come: Deliverance II Head Tracking Tests\n";
    std::cout << "============================================\n";

    int failures = 0;
    failures += RunViewInjectionTests();
    failures += RunAimProjectionTests();
    failures += RunConfigTests();
    failures += RunPoseGuardTests();
    failures += RunExePathsTests();
    failures += RunBuildProfileTests();

    if (failures == 0) {
        std::cout << "All tests passed!\n";
        return 0;
    }
    std::cout << failures << " test(s) FAILED\n";
    return 1;
}
