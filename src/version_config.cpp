#include "version_config.h"

#include "serial.h"

#ifdef __cplusplus
extern "C"
{
#endif

    static const Version version{};

    Version getVersion()
    {
        return version;
    }

#ifdef __cplusplus
}
#endif
