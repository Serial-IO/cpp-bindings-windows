#include "version_config.h"

#include "serial.h"

#ifdef __cplusplus
extern "C"
{
#endif

    static const Version version{};

    void getVersion(Version out)
    {
        out = version;
    }

#ifdef __cplusplus
}
#endif
