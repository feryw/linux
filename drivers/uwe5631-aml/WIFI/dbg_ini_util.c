#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mm.h>

#include "wl_core.h"
#include "dbg_ini_util.h"
#include "sprdwl.h"

#define LOAD_BUF_SIZE 1024
#define MAX_PATH_NUM  3

static char *dbg_ini_file_path[MAX_PATH_NUM] = {
	"/data/misc/wifi/wifi_dbg.ini",
	"/vendor/etc/wifi/wifi_dbg.ini",
	"/etc/wifi_dbg.ini"
};

static int dbg_load_ini_resource(char *path[], char *buf, int size)
{
	int ret;
	int index = 0;
	mm_segment_t oldfs;
	struct file *filp = (struct file *)-ENOENT;

	for (index = 0; index < MAX_PATH_NUM; index++) {
		filp = filp_open(path[index], O_RDONLY, S_IRUSR);
		if (!IS_ERR(filp)) {
			pr_info("find wifi_dbg.ini file in %s\n", path[index]);
			break;
		}
	}
	if (IS_ERR(filp))
		return -ENOENT;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 17, 0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
	oldfs = force_uaccess_begin();
#else
	oldfs = get_fs();
	set_fs(KERNEL_DS);
#endif
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 1)
	ret = kernel_read(filp, buf, size, &filp->f_pos);
#else
	ret = kernel_read(filp, filp->f_pos, buf, size);
#endif
#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 10, 0)
	set_fs(oldfs);
#endif

	filp_close(filp, NULL);

	return ret;
}

static int get_valid_line(char *buf, int buf_size, char *line, int line_size)
{
	int i = 0;
	int rem = 0;
	char *p = buf;

	while (1) {
		if (p - buf >= buf_size)
			break;

		if (i >= line_size)
			break;

		if (*p == '#' || *p == ';')
			rem = 1;

		switch (*p) {
		case '\0':
		case '\r':
		case '\n':
			if (i != 0) {
				line[i] = '\0';
				return p - buf + 1;
			} else {
				rem = 0;
			}

			break;

		case ' ':
			break;

		default:
			if (rem == 0)
				line[i++] = *p;
			break;
		}
		p++;
	}

	return -1;
}

static void dbg_ini_parse(struct dbg_ini_cfg *cfg, char *buf, int size)
{
	int ret;
	int sec = 0;
	int index = 0;
	int left = size;
	char *pos = buf;
	char line[256];
	int status[MAX_SEC_NUM] = {0};
	unsigned long value;

	while (1) {
		ret = get_valid_line(pos, left, line, sizeof(line));
		if (ret < 0 || left < ret)
			break;

		left -= ret;
		pos += ret;

		if (line[0] == '[') {
			if (strcmp(line, "[SOFTAP]") == 0)
				sec = SEC_SOFTAP;
			else if (strcmp(line, "[DEBUG]") == 0)
				sec = SEC_DEBUG;
			else
				sec = SEC_INVALID;

			status[sec]++;
			if (status[sec] != 1) {
				pr_info("invalid section %s\n", line);
				sec = SEC_INVALID;
			}
		} else {
			while (line[index] != '=' && line[index] != '\0')
				index++;

			if (line[index] != '=')
				continue;

			line[index++] = '\0';

			switch (sec) {
			case SEC_SOFTAP:
				if (strcmp(line, "channel") == 0) {
					if (!kstrtoul(&line[index], 0, &value))
						cfg->softap_channel = value;
				}

				break;

			case SEC_DEBUG:
				if (strcmp(line, "log_level") == 0) {
					if (!kstrtoul(&line[index], 0, &value))
						if (value >= L_NONE)
							sprdwl_debug_level = value;
				}

				break;

			default:
				pr_info("drop: %s\n", line);
				break;
			}
		}
	}
}

int dbg_util_init(struct dbg_ini_cfg *cfg)
{
	int ret;
	char *buf;

	buf = kmalloc(LOAD_BUF_SIZE, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	ret = dbg_load_ini_resource(dbg_ini_file_path, buf, LOAD_BUF_SIZE);
	if (ret <= 0) {
		kfree(buf);
		return -EINVAL;
	}

	cfg->softap_channel = -1;
	dbg_ini_parse(cfg, buf, ret);

	kfree(buf);
	return 0;
}

bool is_valid_channel(struct wiphy *wiphy, u16 chn)
{
	int i;
	struct ieee80211_supported_band *bands;

	if (chn < 15) {
		if (chn < 1)
			return false;
		return true;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
	bands = wiphy->bands[NL80211_BAND_5GHZ];
#else
	bands = wiphy->bands[IEEE80211_BAND_5GHZ];
#endif
	for (i = 0; i < bands->n_channels; i++)
		if (chn == bands->channels[i].hw_value)
			return true;

	return false;
}

int sprdwl_dbg_new_beacon_head(const u8 *beacon_head, int head_len, u8 *new_head,  u16 chn)
{
	int len;
	u8 *ies = NULL, *new_ies =NULL;
	struct ieee80211_mgmt *mgmt;
	u8 supp_5g_rates[8] = {0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c};
	u8 supp_24g_rates[8] = {0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24};

	if (beacon_head == NULL || new_head == NULL)
		return 0;

	memcpy(new_head, beacon_head, head_len);

	mgmt = (struct ieee80211_mgmt *)beacon_head;
	ies = mgmt->u.beacon.variable;
	len = beacon_head + head_len - ies;

	mgmt = (struct ieee80211_mgmt *)new_head;
	new_ies = mgmt->u.beacon.variable;

	while (len > 2) {
		switch (ies[0]) {
			case WLAN_EID_SUPP_RATES:
				*new_ies++ = WLAN_EID_SUPP_RATES;
				*new_ies++ = 8;
				if (chn > 14)
					memcpy(new_ies, supp_5g_rates, 8);
				else
					memcpy(new_ies, supp_24g_rates, 8);
				new_ies += 8;
				break;

			case WLAN_EID_DS_PARAMS:
				memcpy(new_ies, ies, ies[1] + 2);
				new_ies[2] = chn;
				new_ies += ies[1] + 2;
				break;
			default:
				memcpy(new_ies, ies, ies[1] + 2);
				new_ies += ies[1] + 2;
				break;
		}

		len -= ies[1] + 2;
		ies += ies[1] + 2;
	}

	return new_ies - new_head;
}

/* Helper function to calculate VHT center frequency for 80 MHz channels */
static u8 get_vht80_center_freq(u16 chn)
{
	/* VHT80 channel groups for 5GHz:
	 * Group 1: 36, 40, 44, 48 -> center 42
	 * Group 2: 52, 56, 60, 64 -> center 58
	 * Group 3: 100, 104, 108, 112 -> center 106
	 * Group 4: 116, 120, 124, 128 -> center 122
	 * Group 5: 132, 136, 140, 144 -> center 138
	 * Group 6: 149, 153, 157, 161 -> center 155
	 */
	if (chn >= 36 && chn <= 48)
		return 42;
	else if (chn >= 52 && chn <= 64)
		return 58;
	else if (chn >= 100 && chn <= 112)
		return 106;
	else if (chn >= 116 && chn <= 128)
		return 122;
	else if (chn >= 132 && chn <= 144)
		return 138;
	else if (chn >= 149 && chn <= 161)
		return 155;
	
	return chn; /* Fallback to primary channel */
}

/* Helper function to determine secondary channel offset for HT40 */
static u8 get_ht40_offset(u16 chn)
{
	/* For VHT80, we need HT40 as base
	 * Lower channels in 80MHz group use secondary above (+1)
	 * Upper channels use secondary below (-1)
	 */
	if (chn >= 36 && chn <= 44)
		return 1; /* SCA - Secondary Channel Above */
	else if (chn >= 52 && chn <= 60)
		return 1;
	else if (chn >= 100 && chn <= 108)
		return 1;
	else if (chn >= 116 && chn <= 124)
		return 1;
	else if (chn >= 132 && chn <= 140)
		return 1;
	else if (chn >= 149 && chn <= 157)
		return 1;
	
	return 0; /* No secondary (20MHz only) */
}

int sprdwl_dbg_new_beacon_tail(const u8 *beacon_tail, int tail_len, u8 *new_tail, u16 chn)
{
	int len;
	const u8 *ies = beacon_tail;
	u8 *tail = new_tail;
	u8 ext_24g_rates[] = {0x30, 0x48, 0x60, 0x6c, 0x0d, 0x1a, 0x27,\
						0x34, 0x4e, 0x68, 0x75, 0x82, 0x1a, 0x34,\
						0x4e, 0x68, 0x9c, 0xd0, 0xea, 0x04};
	u8 ext_5g_rates[] = {0x0d, 0x1a, 0x27, 0x34, 0x4e, 0x68, 0x75,\
						0x82, 0x1a, 0x34, 0x4e, 0x68, 0x9c, 0xd0, 0xea, 0x04};

	if (beacon_tail == NULL || new_tail == NULL)
		return 0;

#define ERP_INFO_BARKER_PREAMBLE_MODE 4
	if (chn <= 14) {
	    *tail++ = WLAN_EID_ERP_INFO;
	    *tail++ = 1;
	    *tail++ = ERP_INFO_BARKER_PREAMBLE_MODE;
	}

	while (tail_len > 2) {
		switch(ies[0]) {
			case WLAN_EID_ERP_INFO:
				break;
			case WLAN_EID_EXT_SUPP_RATES:
				*tail++ = WLAN_EID_EXT_SUPP_RATES;
				if (chn <= 14) {
					len = sizeof(ext_24g_rates);
					*tail++ = len;
					memcpy(tail, ext_24g_rates, len);
					tail += len;
				} else {
					len = sizeof(ext_5g_rates);
					*tail++ = len;
					memcpy(tail, ext_5g_rates, len);
					tail += len;
				}
				break;
			case WLAN_EID_HT_OPERATION:
				/* Copy HT Operation IE */
				memcpy(tail, ies, ies[1] + 2);
				/* Set primary channel */
				tail[2] = chn;
				
				/* For 5GHz, configure HT40 as base for VHT80 */
				if (chn > 14 && ies[1] >= 22) {
					u8 ht_info = get_ht40_offset(chn);
					if (ht_info != 0) {
						/* Set HT operation info byte:
						 * - Bit 0-1: Secondary channel offset (1=above, 3=below)
						 * - Bit 2: STA channel width (1=any)
						 */
						tail[3] = (ht_info & 0x03) | 0x04;  /* Secondary offset + any width */
						pr_info("WiFi: Set HT40 for ch %d, offset=%d\n", chn, ht_info);
					}
				}
				
				tail += ies[1] + 2;
				break;
			case 192: /* WLAN_EID_VHT_OPERATION */
				/* Copy VHT Operation IE */
				memcpy(tail, ies, ies[1] + 2);
				
				/* For 5GHz, configure VHT80 */
				if (chn > 14 && ies[1] >= 3) {
					u8 center_freq = get_vht80_center_freq(chn);
					/* Byte 2: Channel Width (1 = 80 MHz) */
					tail[2] = 1;
					/* Byte 3: Center Frequency Segment 0 */
					tail[3] = center_freq;
					/* Byte 4: Center Frequency Segment 1 (0 for non-80+80) */
					tail[4] = 0;
					pr_info("WiFi: Set VHT80 for ch %d, center=%d\n", chn, center_freq);
				}
				
				tail += ies[1] + 2;
				break;
			default:
				memcpy(tail, ies, ies[1] + 2);
				tail += ies[1] + 2;
				break;
		}

		ies += ies[1] + 2;
		tail_len -= ies[1] + 2;
	}

	return tail - new_tail;
}

#if 0
void sprdwl_dbg_reset_head_ds_params(u8 *beacon_head, int head_len, u16 chn)
{
	u8 *ies, *ie;
	int len;
	struct ieee80211_mgmt *mgmt;
	u8 supp_5g_rates[8] = {0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c};
	u8 supp_24g_rates[8] = {0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24};

	if (beacon_head == NULL)
		return 0;

	mgmt = (struct ieee80211_mgmt *)beacon_head;
	ies = mgmt->u.beacon.variable;

	len = beacon_head + head_len - ies;
	ie = (u8 *)cfg80211_find_ie(WLAN_EID_DS_PARAMS, ies, len);
	if (ie != NULL) {
		ie[2] = chn;
	}

	ie = (u8 *)cfg80211_find_ie(WLAN_EID_SUPP_RATES, ies, len);
	if (ie != NULL && ie[1] == 8) {
		if (chn <= 14)
			memcpy(ie + 2, supp_24g_rates, 8);
		else
			memcpy(ie + 2, supp_5g_rates, 8);

	}
}

void sprdwl_dbg_reset_tail_ht_oper(u8 *tail, int tail_len, u16 chn)
{
	u8 *ie;
	ie = (u8 *)cfg80211_find_ie(WLAN_EID_HT_OPERATION, tail, tail_len);
	if (ie != NULL) {
		ie[2] = chn;
	}
}
#endif
