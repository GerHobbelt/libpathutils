
// - detect and replace NTFS-reserved filenames, e.g. `$MFT`, `$MFTMirr`, `$LogFile`, `$Volume`, `$AttrDef`, `$Bitmap`, `$Boot`, `$BadClus`, `$Secure`, `$UpCase`, `$Extend`, `$ObjId`, `$Quota`, `$Reparse`, `$UsnJrnl`:
//   as there's already NTFS v4 and NTFS v5, we assume that all NTFS-reserved filenames will start with a `$` and replace that lead.
// - when a reserved name is detected, prefix it with an underscore `_`, e.g. `CON` -> `_CON`, `$MFT` -> `_$MFT`

#include "pathutils.h"
