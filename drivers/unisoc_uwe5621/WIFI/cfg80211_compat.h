/*
 * Compatibility layer for cfg80211 API changes across kernel versions
 */

#ifndef __CFG80211_COMPAT_H__
#define __CFG80211_COMPAT_H__

#include <linux/version.h>
#include <net/cfg80211.h>

/* In kernel 6.1+, link_id parameter was added to key management functions */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
#define SPRDWL_KEY_PARAMS_LINK_ID , int link_id
#define SPRDWL_KEY_LINK_ID_ARG , link_id
#else
#define SPRDWL_KEY_PARAMS_LINK_ID
#define SPRDWL_KEY_LINK_ID_ARG
#endif

/* In kernel 6.1+, link_id parameter was added to stop_ap */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
#define SPRDWL_STOP_AP_LINK_ID , unsigned int link_id
#else  
#define SPRDWL_STOP_AP_LINK_ID
#endif

/* In kernel 6.0+, cfg80211_roam_info structure changed */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
#define SPRDWL_ROAM_INFO_BSS_FIELD .links[0].bss
#else
#define SPRDWL_ROAM_INFO_BSS_FIELD .bss
#endif

/* In kernel 5.19+, mgmt_frame_register was removed, replaced by update_mgmt_frame_registrations */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0)
#define SPRDWL_HAS_UPDATE_MGMT_FRAME_REGISTRATIONS
#endif

/* In kernel 6.2+, tdls_mgmt added link_id parameter */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 0)
#define SPRDWL_TDLS_MGMT_LINK_ID , int link_id
#else
#define SPRDWL_TDLS_MGMT_LINK_ID  
#endif

#endif /* __CFG80211_COMPAT_H__ */
