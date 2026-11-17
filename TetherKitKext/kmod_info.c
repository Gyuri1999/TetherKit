/* kmod_info.c
 * The kmod_info record every kext carries. Xcode generates this file for kext
 * targets; this project builds with a Makefile, so it is written out here.
 * KEXT_BUNDLE_ID and KEXT_VERSION are passed in by the Makefile and must match
 * CFBundleIdentifier and CFBundleVersion in Info.plist.
 */
#include <mach/mach_types.h>

extern kern_return_t _start(kmod_info_t *ki, void *data);
extern kern_return_t _stop(kmod_info_t *ki, void *data);

__attribute__((visibility("default")))
KMOD_EXPLICIT_DECL(KEXT_BUNDLE_ID, KEXT_VERSION, _start, _stop)

__private_extern__ kmod_start_func_t *_realmain = 0;
__private_extern__ kmod_stop_func_t *_antimain = 0;
__private_extern__ int _kext_apple_cc = __APPLE_CC__;
