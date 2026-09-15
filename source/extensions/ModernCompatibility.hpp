#pragma once

namespace notsa::modern {
    void Initialise();
    void Service();
    void Shutdown();

    uint32 GetDesktopRefreshRate();
    uint32 GetTargetFrameRate();
}
