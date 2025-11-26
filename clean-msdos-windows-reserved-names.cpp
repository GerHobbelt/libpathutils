
// - detect and replace MS-DOS and Windows reserved filenames, e.g. `CON`, `PRN`, `AUX`, `NUL`, `COM1` ... `COM9`, `LPT1` ... `LPT9`
// - when a reserved name is detected, prefix it with an underscore `_`, e.g. `CON` -> `_CON`, `$MFT` -> `_$MFT`

#include "pathutils.h"
