// This file aims to make code imported from OpenBSD compile under macOS with the minimum changes.

#ifndef SINETEK_RTSX_OPENBSD_OPENBSD_COMPAT_H
#define SINETEK_RTSX_OPENBSD_OPENBSD_COMPAT_H

// include necessary headers
#include <sys/types.h> // size_t (include this first)
#include <sys/cdefs.h> // __BEGIN_DECLS, __END_DECLS
__BEGIN_DECLS
#include <sys/malloc.h> // _MALLOC, _FREE
#include <sys/queue.h>
#include <sys/systm.h> // MIN, EINVAL, ENOMEM, etc...
__END_DECLS

#include <IOKit/IOLib.h> // IOMalloc / IOFree
#include <IOKit/IOTimerEventSource.h>

// DMA-related functions
#include "openbsd_compat_dma.h"

#ifndef UTL_THIS_CLASS
#define UTL_THIS_CLASS ""
#endif
#include "util.h"

#if RTSX_USE_IOLOCK
//#define splsdmmc(...) UTLsplsdmmc(sc->splsdmmc_rec_lock)
#define splbio(...) UTLsplsdmmc(sc->splsdmmc_rec_lock)
inline int UTLsplsdmmc(IORecursiveLock *l) {
    IORecursiveLockLock(l);
    /* UTL_DEBUG(2, "Locked splsdmmc_lock"); */
    return 0;
}

#define splx(n) do { \
    /* UTL_DEBUG(2, "Unlocking splsdmmc_lock"); */ \
    IORecursiveLockUnlock(sc->splsdmmc_rec_lock); \
} while (0)

#define INFSLP 0 // -1?
#define tlseep_nsec(a1, a2, a3, a4) \
do { \
} while(0)
#endif

// SIMPLEQ -> STAILQ
#define SIMPLEQ_EMPTY       STAILQ_EMPTY
#define SIMPLEQ_ENTRY       STAILQ_ENTRY
#define SIMPLEQ_FIRST       STAILQ_FIRST
#define SIMPLEQ_FOREACH     STAILQ_FOREACH
#define SIMPLEQ_HEAD        STAILQ_HEAD
#define SIMPLEQ_INIT        STAILQ_INIT
#define SIMPLEQ_INSERT_TAIL STAILQ_INSERT_TAIL
#define SIMPLEQ_NEXT        STAILQ_NEXT

// disable execcisve logging from OpenBSD code
#if DEBUG
#define printf(...) UTL_DEBUG(1, __VA_ARGS__)
#else
#define printf(...) do {} while (0)
#endif

#ifdef KASSERT
#undef KASSERT
#define KASSERT(expr) \
do { \
    if (!(expr)) { \
        UTL_ERR("Assertion failed: %s", #expr); \
    } \
} while (0)
#endif

// bit manipulation macros
#define SET(t, f)    ((t) |=  (f))
#define ISSET(t, f)  ((t) &   (f))
#define CLR(t, f)    ((t) &= ~(f))

// bitfield (most probably unused)
#define __packed
#define __aligned(N)

// rwlock
#define rw_assert_wrlock(a1) do {} while (0)
#define rw_enter_write(a1) do {} while (0)
#define rw_exit(a1) do {} while (0)

#define tsleep_nsec(a1, a2, a3, a4) ((int)0) /* expects an expression */

// scsi
#define sdmmc_scsi_attach(a1) do {} while (0)

extern int hz;
#include "openbsd_compat_types.h"

#define sdmmc_softc rtsx_softc // TODO: FIX THIS MESS

// other headers from OpenBSD (they have to be at the end since they use types defined here
#define _KERNEL // needed by some headers
#include "device.h"
#include "sdmmc_ioreg.h"
#include "sdmmcchip.h"
#include "sdmmcdevs.h"
#include "sdmmcreg.h"
#include "sdmmcvar.h"
#include "rtsxreg.h"
#include "rtsxvar.h"
#include "Sinetek_rtsx.hpp"

// config*
#define config_activate_children(a1, a2) (0) /* expects an expression */
#define config_detach(a1, a2) do {} while (0)
#define config_found_sm(a1, a2, a3, a4) (0) /* expects an expression */

//#define sdmmc_needs_discover(a1) ((void)0)

#define be32toh OSSwapBigToHostInt32
#define betoh32 OSSwapBigToHostInt32
#define htole32 OSSwapHostToLittleInt32
#define htole64 OSSwapHostToLittleInt64
#define nitems(v) (sizeof(v) / sizeof((v)[0]))

#ifndef M_DEVBUF
#   define M_DEVBUF 0 // seems like not used on macOS
#endif
static inline void *malloc(size_t size, int type, int flags) {
    return _MALLOC(size, type, flags);
}
static inline void free(void *addr, int type, int flags) {
    _FREE(addr, type);
}
// check if they are microseconds or milliseconds!
static inline void delay(unsigned int microseconds) {
    IODelay(microseconds);
}
#define delay(t) IODelay(t)

#undef READ4
#define READ4(sc, reg) \
({ \
    uint32_t val; \
    sc->memory_descriptor_->readBytes(reg, &val, 4); \
    val; \
})
#undef WRITE4
#define WRITE4(sc, reg, val) \
do { \
    uint32_t _val = val; \
    sc->memory_descriptor_->writeBytes(reg, &_val, 4); \
} while (0)

#define RTSX_F_525A          0x20

#define RTSX_PCI_BAR         0x10
#define RTSX_PCI_BAR_525A    0x14
/* syscl - end of 1.4-1.5 change */

#define PCI_PRODUCT_REALTEK_RTS5209     0x5209          /* RTS5209 PCI-E Card Reader */
#define PCI_PRODUCT_REALTEK_RTS5227     0x5227          /* RTS5227 PCI-E Card Reader */
#define PCI_PRODUCT_REALTEK_RTS5229     0x5229          /* RTS5229 PCI-E Card Reader */
#define PCI_PRODUCT_REALTEK_RTS5249     0x5249          /* RTS5249 PCI-E Card Reader */
#define PCI_PRODUCT_REALTEK_RTL8402     0x5286          /* RTL8402 PCI-E Card Reader */
#define PCI_PRODUCT_REALTEK_RTL8411B    0x5287          /* RTL8411B PCI-E Card Reader */
#define PCI_PRODUCT_REALTEK_RTL8411     0x5289          /* RTL8411 PCI-E Card Reader */

/*
 * syscl - add extra support for new card reader here
 */
#define PCI_PRODUCT_REALTEK_RTS525A     0x525A          /* RTS525A PCI-E Card Reader (XPS 13/15 Series) */

// shut-up warnings
#pragma clang diagnostic ignored "-Wconditional-uninitialized"
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wwritable-strings"

#endif // SINETEK_RTSX_OPENBSD_OPENBSD_COMPAT_H
