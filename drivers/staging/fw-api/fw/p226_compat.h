/* p226 compat header — force-included via KCFLAGS -include for the 2022-era
 * fw-api/qcacld trio on 4.19. Provides the Qualcomm fw macros that the 2022
 * headers expect the Android build system to supply. Guarded, idempotent. */
#ifndef P226_COMPAT_H
#define P226_COMPAT_H

#ifndef A_OFFSETOF
#define A_OFFSETOF(type, field) offsetof(type, field)
#endif

#ifndef PREPACK
#define PREPACK
#endif

#ifndef POSTPACK
#define POSTPACK
#endif

#ifndef __ATTRIB_PACK
#define __ATTRIB_PACK __attribute__((packed))
#endif

#endif /* P226_COMPAT_H */
