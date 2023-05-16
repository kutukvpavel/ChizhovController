#include "compat_api.h"

namespace compat
{
    void uDelay(uint32_t us)
    {
        uint32_t start = micros();
        while ((micros() - start) < us);
    }
} // namespace compat
