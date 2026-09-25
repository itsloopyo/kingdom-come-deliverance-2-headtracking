#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

int RunViewInjectionTests();
int RunAimProjectionTests();
int RunConfigTests();
int RunPoseGuardTests();
int RunExePathsTests();
int RunBuildProfileTests();
int RunAdsGateTests();

std::string RenderCommittedConfig();

int main(int argc, char** argv)
{
    // `pixi run render-config`: rewrite the committed HeadTracking.ini from the
    // config table and run no tests.
    if (argc == 3 && std::strcmp(argv[1], "--render-config") == 0) {
        std::ofstream out(argv[2], std::ios::binary | std::ios::trunc);
        out << RenderCommittedConfig();
        if (!out) {
            std::cout << "could not write " << argv[2] << "\n";
            return 1;
        }
        std::cout << "wrote " << argv[2] << "\n";
        return 0;
    }

    std::cout << "Kingdom Come: Deliverance II Head Tracking Tests\n";
    std::cout << "============================================\n";

    int failures = 0;
    failures += RunViewInjectionTests();
    failures += RunAimProjectionTests();
    failures += RunConfigTests();
    failures += RunPoseGuardTests();
    failures += RunExePathsTests();
    failures += RunBuildProfileTests();
    failures += RunAdsGateTests();

    if (failures == 0) {
        std::cout << "All tests passed!\n";
        return 0;
    }
    std::cout << failures << " test(s) FAILED\n";
    return 1;
}
