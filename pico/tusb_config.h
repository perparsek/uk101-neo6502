/* tusb_config.h — USB-värd, bara HID-tangentbord.
 *
 * Trimmad variant av Neo6502-firmwarens konfiguration: mus, gamepads,
 * masslagring och hubbar är avstängda i v1, som bara behöver ett tangentbord.
 * Slå på CFG_TUH_HUB om tangentbordet sitter bakom en hubb.
 */
#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_HOST

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS                 OPT_OS_NONE
#endif

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif
#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN          __attribute__((aligned(4)))
#endif

#define CFG_TUH_ENUMERATION_BUFSIZE 256

#define CFG_TUH_HUB                 1
#define CFG_TUH_CDC                 0
#define CFG_TUH_HID                 4   /* ett tangentbord kan ha flera HID-gränssnitt */
#define CFG_TUH_MIDI                0
#define CFG_TUH_MSC                 0
#define CFG_TUH_VENDOR              0

#define CFG_TUH_DEVICE_MAX          (CFG_TUH_HUB ? 5 : 1)
#define CFG_TUH_HID_EPIN_BUFSIZE    64
#define CFG_TUH_HID_EPOUT_BUFSIZE   64

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */
