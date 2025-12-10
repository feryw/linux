//SPX-License-Identifier: GPL-2.0-or-later
/*
Sony cxd2878 family
Copyright (c) 2021 Davin zhang <Davin@tbsdtv.com> www.Turbosight.com
*/
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <media/dvb_frontend.h>
#include <linux/mutex.h>

#include "cxd2878.h"
#include "cxd2878_priv.h"


static LIST_HEAD(cxdlist);

struct cxd_base{
	struct list_head cxdlist;
	struct i2c_adapter *i2c;
	struct mutex i2c_lock; //for two adapter at the same i2c bus
	u8 adr		;		// 
	u32 count	;		//
	struct cxd2878_config *config;	

};

struct cxd2878_dev{
	struct cxd_base *base;
	bool warm; //start
	struct dvb_frontend fe;
	enum sony_dtv_system_t system;
	enum sony_dtv_bandwidth_t bandwidth;
	enum sony_demod_state_t state;
	u8 slvt;  //for slvt addr;
	u8 slvx;	//addr
	u8 slvr;	//addr
	u8 slvm;	//addr
	u8 tuner_addr;
	enum sony_demod_chip_id_t chipid;
	enum sony_ascot3_chip_id_t tunerid;
	struct sony_demod_iffreq_config_t iffreqConfig;

	u32 atscNoSignalThresh;
	u32 atscSignalThresh;
	u32 tune_time;
 };
/* For CXD2856 or newer generation ICs */
static	struct sony_ascot3_adjust_param_t g_param_table_ascot3i[SONY_ASCOT3_TV_SYSTEM_NUM] = {
	/*
	OUTLMT	  IF_BPF_GC 										  BW			  BW_OFFSET 		IF_OUT_SEL
	  |  RF_GAIN  | 	RFOVLD_DET_LV1	  IFOVLD_DET_LV  IF_BPF_F0 |   FIF_OFFSET	  | 	   AGC_SEL |  IS_LOWERLOCAL
	  | 	|	  |    (VL)  (VH)  (U)	 (VL)  (VH)  (U)	|	   |	   |		  | 		 |	   |	 |			*/
	{0x00, AUTO, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, BW_6,	OFFSET(0),	OFFSET(0),	AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_TV_SYSTEM_UNKNOWN */
	/* Analog */
	{0x00, AUTO, 0x05, 0x03, 0x06, 0x03, 0x01, 0x01, 0x01, 0x00, BW_6,	OFFSET(0),	OFFSET(1),	AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_ATV_MN_EIAJ   (System-M (Japan)) */
	{0x00, AUTO, 0x05, 0x03, 0x06, 0x03, 0x01, 0x01, 0x01, 0x00, BW_6,	OFFSET(0),	OFFSET(1),	AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_ATV_MN_SAP	  (System-M (US)) */
	{0x00, AUTO, 0x05, 0x03, 0x06, 0x03, 0x01, 0x01, 0x01, 0x00, BW_6,	OFFSET(3),	OFFSET(1),	AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_ATV_MN_A2	  (System-M (Korea)) */
	{0x00, AUTO, 0x05, 0x03, 0x06, 0x03, 0x01, 0x01, 0x01, 0x00, BW_7,	OFFSET(11), OFFSET(5),	AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_ATV_BG		  (System-B/G) */
	{0x00, AUTO, 0x05, 0x03, 0x06, 0x03, 0x01, 0x01, 0x01, 0x00, BW_8,	OFFSET(2),	OFFSET(-3), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_ATV_I		  (System-I) */
	{0x00, AUTO, 0x05, 0x03, 0x06, 0x03, 0x01, 0x01, 0x01, 0x00, BW_8,	OFFSET(2),	OFFSET(-3), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_ATV_DK		  (System-D/K) */
	{0x00, AUTO, 0x03, 0x04, 0x0A, 0x04, 0x04, 0x04, 0x04, 0x00, BW_8,	OFFSET(2),	OFFSET(-3), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_ATV_L		  (System-L) */
	{0x00, AUTO, 0x03, 0x04, 0x0A, 0x04, 0x04, 0x04, 0x04, 0x00, BW_8,	OFFSET(-1), OFFSET(4),	AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_ATV_L_DASH	  (System-L DASH) */
	/* Digital */
	{0x00, AUTO, 0x08, 0x0B, 0x0B, 0x0B, 0x03, 0x03, 0x03, 0x00, BW_6,	OFFSET(-6), OFFSET(-3), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_8VSB	  (ATSC 8VSB) */
	{0x00, AUTO, 0x08, 0x0B, 0x0B, 0x0B, 0x02, 0x02, 0x02, 0x00, BW_6,	OFFSET(-6), OFFSET(-3), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_QAM 	  (US QAM) */
	{0x00, AUTO, 0x08, 0x0B, 0x0B, 0x0B, 0x02, 0x02, 0x02, 0x00, BW_6,	OFFSET(-9), OFFSET(-5), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_ISDBT_6   (ISDB-T 6MHzBW) */
	{0x00, AUTO, 0x08, 0x0B, 0x0B, 0x0B, 0x02, 0x02, 0x02, 0x00, BW_7,	OFFSET(-7), OFFSET(-6), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_ISDBT_7   (ISDB-T 7MHzBW) */
	{0x00, AUTO, 0x08, 0x0B, 0x0B, 0x0B, 0x02, 0x02, 0x02, 0x00, BW_8,	OFFSET(-5), OFFSET(-7), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_ISDBT_8   (ISDB-T 8MHzBW) */
	{0x00, AUTO, 0x08, 0x0B, 0x0B, 0x0B, 0x02, 0x02, 0x02, 0x00, BW_6,	OFFSET(-8), OFFSET(-3), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_DVBT_5	  (DVB-T 5MHzBW) */
	{0x00, AUTO, 0x08, 0x0B, 0x0B, 0x0B, 0x02, 0x02, 0x02, 0x00, BW_6,	OFFSET(-8), OFFSET(-3), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_DVBT_6	  (DVB-T 6MHzBW) */
	{0x00, AUTO, 0x08, 0x0B, 0x0B, 0x0B, 0x02, 0x02, 0x02, 0x00, BW_7,	OFFSET(-6), OFFSET(-5), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_DVBT_7	  (DVB-T 7MHzBW) */
	{0x00, AUTO, 0x08, 0x0B, 0x0B, 0x0B, 0x02, 0x02, 0x02, 0x00, BW_8,	OFFSET(-4), OFFSET(-6), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_DVBT_8	  (DVB-T 8MHzBW) */
	{0x00, AUTO, 0x08, 0x0B, 0x0B, 0x0B, 0x02, 0x02, 0x02, 0x00, BW_1_7,OFFSET(-10),OFFSET(-10),AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_DVBT2_1_7 (DVB-T2 1.7MHzBW) */
	{0x00, AUTO, 0x08, 0x0B, 0x0B, 0x0B, 0x02, 0x02, 0x02, 0x00, BW_6,	OFFSET(-8), OFFSET(-3), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_DVBT2_5   (DVB-T2 5MHzBW) */
	{0x00, AUTO, 0x08, 0x0B, 0x0B, 0x0B, 0x02, 0x02, 0x02, 0x00, BW_6,	OFFSET(-8), OFFSET(-3), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_DVBT2_6   (DVB-T2 6MHzBW) */
	{0x00, AUTO, 0x08, 0x0B, 0x0B, 0x0B, 0x02, 0x02, 0x02, 0x00, BW_7,	OFFSET(-6), OFFSET(-5), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_DVBT2_7   (DVB-T2 7MHzBW) */
	{0x00, AUTO, 0x08, 0x0B, 0x0B, 0x0B, 0x02, 0x02, 0x02, 0x00, BW_8,	OFFSET(-4), OFFSET(-6), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_DVBT2_8   (DVB-T2 8MHzBW) */
	{0x00, AUTO, 0x04, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x00, BW_6,	OFFSET(-6), OFFSET(-4), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_DVBC_6	  (DVB-C 6MHzBW) */
	{0x00, AUTO, 0x04, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x00, BW_8,	OFFSET(-2), OFFSET(-3), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_DVBC_8	  (DVB-C 8MHzBW) */
	{0x00, AUTO, 0x02, 0x09, 0x09, 0x09, 0x02, 0x02, 0x02, 0x00, BW_6,	OFFSET(-6), OFFSET(-2), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_DVBC2_6   (DVB-C2 6MHzBW) */
	{0x00, AUTO, 0x02, 0x09, 0x09, 0x09, 0x02, 0x02, 0x02, 0x00, BW_8,	OFFSET(-2), OFFSET(0),	AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_DVBC2_8   (DVB-C2 8MHzBW) */
	{0x00, AUTO, 0x08, 0x0B, 0x0B, 0x0B, 0x02, 0x02, 0x02, 0x00, BW_6,	OFFSET(-8), OFFSET(-3), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_ATSC3_6   (ATSC 3.0 6MHzBW) */
	{0x00, AUTO, 0x08, 0x0B, 0x0B, 0x0B, 0x02, 0x02, 0x02, 0x00, BW_7,	OFFSET(-6), OFFSET(-5), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_ATSC3_7   (ATSC 3.0 7MHzBW) */
	{0x00, AUTO, 0x08, 0x0B, 0x0B, 0x0B, 0x02, 0x02, 0x02, 0x00, BW_8,	OFFSET(-4), OFFSET(-6), AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_ATSC3_8   (ATSC 3.0 8MHzBW) */
	{0x00, AUTO, 0x04, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x00, BW_6,	OFFSET(-5), OFFSET(2),	AUTO, AUTO, 0x00}, /**< SONY_ASCOT3_DTV_J83B_5_6  (J.83B 5.6Msps) */
	{0x00, AUTO, 0x03, 0x0B, 0x0B, 0x0B, 0x02, 0x02, 0x02, 0x00, BW_8,	OFFSET(2),	OFFSET(1),	AUTO, AUTO, 0x00}  /**< SONY_ASCOT3_DTV_DTMB	  (DTMB) */
};

static const u8 log2LookUp[] = {
    0, /* 0 */ 4,               /* 0.04439 */
    9, /* 0.08746 */ 13,        /* 0.12928 */
    17, /* 0.16993 */ 21,       /* 0.20945 */
    25, /* 0.24793 */ 29,       /* 0.28540 */
    32, /* 0.32193 */ 36,       /* 0.35755 */
    39, /* 0.39232 */ 43,       /* 0.42627 */
    46, /* 0.45943 */ 49,       /* 0.49185 */
    52, /* 0.52356 */ 55,       /* 0.55249 */
    58, /* 0.58496 */ 61,       /* 0.61471 */
    64, /* 0.64386 */ 67,       /* 0.67246 */
    70, /* 0.70044 */ 73,       /* 0.72792 */
    75, /* 0.75489 */ 78,       /* 0.78136 */
    81, /* 0.80736 */ 83,       /* 0.83289 */
    86, /* 0.85798 */ 88,       /* 0.88264 */
    91, /* 0.90689 */ 93,       /* 0.93074 */
    95, /* 0.95420 */ 98        /* 0.97728 */
};

static u32 sony_math_log2 (u32 x)
{

    u8 count = 0;
    u8 index = 0;
    u32 xval = x;

    /* Get the MSB position. */
    for (x >>= 1; x > 0; x >>= 1) {
        count++;
    }

    x = count * 100;

    if (count > 0) {
        if (count <= MAX_BIT_PRECISION) {
            /* Mask the bottom bits. */
            index = (u8) (xval << (MAX_BIT_PRECISION - count)) & FRAC_BITMASK;
            x += log2LookUp[index];
        }
        else {
            /* Mask the bits just below the radix. */
            index = (u8) (xval >> (count - MAX_BIT_PRECISION)) & FRAC_BITMASK;
            x += log2LookUp[index];
        }
    }

    return (x);
}
static u32 sony_math_log10 (u32 x)
{
    /* log10(x) = log2 (x) / log2 (10) */
    /* Note uses: logN (x) = logM (x) / logM (N) */
    return ((100 * sony_math_log2 (x) + LOG2_10_100X / 2) / LOG2_10_100X);
}
static u32 sony_math_log (u32 x)
{
    /* ln (x) = log2 (x) / log2(e) */
    return ((100 * sony_math_log2 (x) + LOG2_E_100X / 2) / LOG2_E_100X);
}
/*write multi registers*/
static int cxd2878_wrm(struct cxd2878_dev *dev,u8 addr, u8 reg,u8*buf,u8 len)
{
	int ret ;
	u8 b0[50];
	struct i2c_msg msg = {
		.addr = addr,
		.flags = 0,
		.buf = b0,
		.len = len+1,
	};

	b0[0] = reg;
	memcpy(&b0[1],buf,len);

	ret = i2c_transfer(dev->base->i2c,&msg,1);
	if(ret<0){
		dev_warn(&dev->base->i2c->dev,
			"%s: i2c wrm err(%i) @0x%02x (len=%d)\n",
			KBUILD_MODNAME, ret, reg, len);
		return ret;
		}
	
	//printk("wrm : addr = 0x%x args=%*ph\n",addr*2,len+1,b0);
	return 0;

}

/* ... (skipping unchanged functions) ... */

static int cxd2878_atsc_SlaveRWriteRegister (struct cxd2878_dev*dev,
                                                   u8  bank,
                                                   u8  registerAddress,
                                                   u8  value,
                                                   u8  bitMask)
{
		int ret = 0 ;
		u8 cmd[6];
		u8 rdata[6];
		int waittime = 0;
 /*  Write Register Command
     *  byte0:COMMANDID_WRITE_REG 0xC5
     *  byte1:Bank
     *  byte2:SubAddress
     *  byte3:Write Data
     *  byte4:Mask
     *  byte5:Reserved(0x00)
     */
    cmd[0] = 0xC5;
    cmd[1] = bank;
    cmd[2] = registerAddress;
    cmd[3] = value;
    cmd[4] = bitMask;
    cmd[5] = 0;

	ret = cxd2878_wrm(dev,dev->slvr,0x0A,cmd,6);
	
    for(;;){
   	
	ret = cxd2878_rdm(dev,dev->slvr,0x0A,rdata,6);
	
	if(rdata[0]==0x00){
		msleep(10);
		waittime += 10;
		}
		else{
		 if(rdata[5]== cmd[0])
		 	break;
		 else {
		 	ret = -1;
		 	goto err;
			}
		}

		if(waittime>1000){
			ret = -1;
			goto err;}
	  }
   	if((rdata[0]&0x3F)!=0x30){
		ret = -1;
		goto err;
   	}
	return 0;
err:
	dev_err(&dev->base->i2c->dev,"cxd2878_atscSlaveRWriteRegister error.\n");
	return ret;		
}

/* ... (skipping unchanged functions) ... */

static int cxd2878_atsc_softreset(struct cxd2878_dev *dev)
{
	int ret,waittime;
	u8 data[6],rdata[6];
	ret = cxd2878_wr(dev,dev->slvr,0x00,0x01);  //demodabort
	ret |= cxd2878_wr(dev,dev->slvr,0x48,0x01); // atsccpustate = IDLE
	if(ret)
		goto err;

	data[0] = 0xB3;
	data[1] = 0x10; //8vsb = 0x10 ,qam64=0x11 256qam=0x12 auto=0x16
	data[2] = 0;
	data[3] = 0;
	data[4] = 0;
	data[5] = 0;

	ret = cxd2878_wrm(dev,dev->slvr,0x0A,data,6);
	
    for(;;){
   	
	ret = cxd2878_rdm(dev,dev->slvr,0x0A,rdata,6);
	
	if(rdata[0]==0x00){
		msleep(10);
		waittime += 10;
		}
		else{
		 if(rdata[5]== 0xB3)
		 	break;
		 else {
		 	ret = -1;
		 	goto err;
			}
		}

		if(waittime>1000){
			ret = -1;
			goto err;}
	  }	

	 if((rdata[0]&0x3F)!=0x30){
		ret = -1;
		goto err;
   	 }
	return 0;

err:
	dev_err(&dev->base->i2c->dev,"%s :cxd2878_atsc_softreset error! \n",KBUILD_MODNAME);
	return ret;	
}
static int cxd2878_i2c_repeater(struct cxd2878_dev *dev,bool enable)
{
	int ret;

	ret = cxd2878_wr(dev,dev->slvx,0x08,enable?1:0);
	if(ret)
		goto err;

	msleep(20);

	return 0;

err:
	dev_err(&dev->base->i2c->dev,"%s : %sable thee repeater failed! \n",KBUILD_MODNAME,enable?"en":"dis");
	return ret;
}

static int ascot3_read_rssi(struct cxd2878_dev*dev,u32 frequency,s32 *rssi)
{
	int ret = 0;
	u8 ifagc,rfagc,tmp;
	s32 ifgain=0,rfgain=0;
	u8 data[2];
	s32 if_bpf_gc_table[]={0,0,0,0,2,4,6,8,10,12,14,16,18,20,20,20};
	s32 if_bpf_gc_x100 = 0;
	s32 agcreg_x140 = 0,maxagcreg_x140 = 0,rfgainmax_100 = 0;
	
	data[0] = 0xc4;
	data[1] = 0x41; //(x87 x88)
	ret = cxd2878_wrm(dev,dev->tuner_addr,0x87,data,2);
	if(ret)goto err;

	//connect ifagc,startADC
	data[0] = 0x05;
	data[1] = 0x01;
	ret = cxd2878_wrm(dev,dev->tuner_addr,0x59,data,2);
	if(ret)goto err;
		
	ret|=cxd2878_rdm(dev,dev->tuner_addr,0x5B,&ifagc,1);

	//connect rfagc,startADC
	data[0] = 0x03;
	data[1] = 0x01;
	ret |= cxd2878_wrm(dev,dev->tuner_addr,0x59,data,2);
	
	ret|=cxd2878_rdm(dev,dev->tuner_addr,0x5B,&rfagc,1);
	if(ret)goto err;

	ret |= cxd2878_wr(dev,dev->tuner_addr,0x59,0x04);
	ret |= cxd2878_wr(dev,dev->tuner_addr,0x88,0x04);
	ret |= cxd2878_wr(dev,dev->tuner_addr,0x87,0xC0);
	if(ret)goto err;

	agcreg_x140 = ifagc*140;
	
	cxd2878_rdm(dev,dev->tuner_addr,0x69,&tmp,1);
	if_bpf_gc_x100 = if_bpf_gc_table[tmp&0xF]*100;
	
	if(agcreg_x140>9945)
		ifgain = 870+if_bpf_gc_x100;
	else
		ifgain = 870+if_bpf_gc_x100+(769*(9945-agcreg_x140)+1275)/2550;

	if(ifagc>rfagc)
		maxagcreg_x140 = ifagc*140;
	else
		maxagcreg_x140 = rfagc*140;

	if(frequency>700000)
		rfgainmax_100 = 4150;
	else if(frequency>600000)
		rfgainmax_100 = 4130;
	else if(frequency>532000)
		rfgainmax_100 = 4170;
	else if(frequency>464000)
		rfgainmax_100 = 4050;
	else if(frequency>400000)
		rfgainmax_100 = 4280;
	else if(frequency>350000)
		rfgainmax_100 = 4260;
	else if(frequency>320000)
		rfgainmax_100 = 4110;
	else if(frequency>285000)
		rfgainmax_100 = 4310;
	else if(frequency>215000)
		rfgainmax_100 = 4250;
	else if(frequency>184000)
		rfgainmax_100 = 4020;
	else if(frequency>172000)
		rfgainmax_100 = 3920;
	else if(frequency>150000)
		rfgainmax_100 = 4080;
	else if(frequency>86000)
		rfgainmax_100 = 4180;
	else if(frequency>65000)
		rfgainmax_100 = 4200;
	else if(frequency>50000)
		rfgainmax_100 = 4020;
	else
		rfgainmax_100 = 4020;

	
	if(maxagcreg_x140<4896)
		rfgain = rfgainmax_100;
	else if(maxagcreg_x140<5457)
		rfgain = rfgainmax_100-(70*(maxagcreg_x140-4896)+127)/255;
	else if(maxagcreg_x140<8823)
		rfgain = rfgainmax_100-154;
	else if(maxagcreg_x140<24786)
		rfgain = rfgainmax_100 -154 -(70*(maxagcreg_x140-8823)+127)/255;
	else if(maxagcreg_x140<30090)
		rfgain = rfgainmax_100 -4536 -(57*(maxagcreg_x140-24786)+127)/255;
	else 
		rfgain = rfgainmax_100 -4536-1186 -(160*(maxagcreg_x140-30090)+127)/255;

	*rssi = -ifgain-rfgain;

//	printk("frequency= %d,rssi = %d\n",frequency,*rssi);
	return 0;

err:
	dev_err(&dev->base->i2c->dev,"%s : ascot3_read_rssi failed! \n",KBUILD_MODNAME);
	return ret;
}
static int ascot3_tune(struct cxd2878_dev*dev,u32 frequency)
{
	int ret;
	u8 cdata[2] = {0xC4,0x40};
	u8 data[2];
	u8 data1[2];
	u8 data2[9];
	u8 data3[17];
	enum sony_ascot3_tv_system_t aSystem;
    /* Convert system, bandwidth into dtv system. */
    switch (dev->system) {
    case SONY_DTV_SYSTEM_DVBC:
        switch (dev->bandwidth) {
        case SONY_DTV_BW_6_MHZ:
            aSystem = SONY_ASCOT3_DTV_DVBC_6;
            break;
        case SONY_DTV_BW_7_MHZ:
            /* 7MHZ BW setting is the same as 8MHz BW */
        case SONY_DTV_BW_8_MHZ:
            aSystem = SONY_ASCOT3_DTV_DVBC_8;
            break;
        default:
            return 1;
        }
        break;

    case SONY_DTV_SYSTEM_DVBT:
        switch (dev->bandwidth) {
        case SONY_DTV_BW_5_MHZ:
            aSystem = SONY_ASCOT3_DTV_DVBT_5;
            break;
        case SONY_DTV_BW_6_MHZ:
            aSystem = SONY_ASCOT3_DTV_DVBT_6;
            break;
        case SONY_DTV_BW_7_MHZ:
            aSystem = SONY_ASCOT3_DTV_DVBT_7;
            break;
        case SONY_DTV_BW_8_MHZ:
            aSystem = SONY_ASCOT3_DTV_DVBT_8;
            break;
        default:
            return 1;
        }
        break;

    case SONY_DTV_SYSTEM_DVBT2:
        switch (dev->bandwidth) {
        case SONY_DTV_BW_1_7_MHZ:
            aSystem = SONY_ASCOT3_DTV_DVBT2_1_7;
            break;
        case SONY_DTV_BW_5_MHZ:
            aSystem = SONY_ASCOT3_DTV_DVBT2_5;
            break;
        case SONY_DTV_BW_6_MHZ:
            aSystem = SONY_ASCOT3_DTV_DVBT2_6;
            break;
        case SONY_DTV_BW_7_MHZ:
            aSystem = SONY_ASCOT3_DTV_DVBT2_7;
            break;
        case SONY_DTV_BW_8_MHZ:
            aSystem = SONY_ASCOT3_DTV_DVBT2_8;
            break;
        default:
            return 1;
        }
        break;

    case SONY_DTV_SYSTEM_ISDBT:
        switch (dev->bandwidth) {
        case SONY_DTV_BW_6_MHZ:
            aSystem = SONY_ASCOT3_DTV_ISDBT_6;
            break;
        case SONY_DTV_BW_7_MHZ:
            aSystem = SONY_ASCOT3_DTV_ISDBT_7;
            break;
        case SONY_DTV_BW_8_MHZ:
            aSystem = SONY_ASCOT3_DTV_ISDBT_8;
            break;
        default:
            return 1;
        }
        break;

    case SONY_DTV_SYSTEM_ISDBC:
        aSystem = SONY_ASCOT3_DTV_DVBC_6; /* ISDB-C uses DVB-C 6MHz BW setting */
        break;

    case SONY_DTV_SYSTEM_ATSC:
        aSystem = SONY_ASCOT3_DTV_8VSB;
        break;


    case SONY_DTV_SYSTEM_J83B:
        switch (dev->bandwidth) {
        case SONY_DTV_BW_J83B_5_06_5_36_MSPS:
            aSystem = SONY_ASCOT3_DTV_DVBC_6; /* J.83B (5.057, 5.361Msps commonly used in US) uses DVB-C 6MHz BW setting */
            break;
        case SONY_DTV_BW_J83B_5_60_MSPS:
            aSystem = SONY_ASCOT3_DTV_J83B_5_6; /* J.83B (5.6Msps used in Japan) uses special setting */
            break;
        default:
            return 1;
        }
        break;

    /* Intentional fall-through */
    case SONY_DTV_SYSTEM_UNKNOWN:
    default:
        return 1;
    }

	/* Disable IF signal output (IF_OUT_SEL setting) (0x74) */
	cxd2878_SetRegisterBits( dev,dev->tuner_addr, 0x74, 0x02, 0x03);

	cxd2878_wrm(dev,dev->tuner_addr,0x87,cdata,2);
	
	/* Initial setting for internal analog block (0x91, 0x92) */
	if((aSystem== SONY_ASCOT3_DTV_DVBC_6) || (aSystem == SONY_ASCOT3_DTV_DVBC_8) || (aSystem == SONY_ASCOT3_DTV_J83B_5_6)){
		data[0] = 0x16;
		data[1] = 0x26;
	}else{
		data[0] = 0x10;
		data[1] = 0x20;
	}
	cxd2878_wrm(dev,dev->tuner_addr,0x91,data,2);
    data1[0] = 0x00;
    data1[1] = (u8)(g_param_table_ascot3i[aSystem].IS_LOWERLOCAL &0x01);
	cxd2878_wrm(dev,dev->tuner_addr,0x9c,data1,2); 
	
    /* Enable for analog block (0x5E, 0x5F, 0x60) */
    data2[0] = 0xEE;
    data2[1] = 0x02;
    data2[2] = 0x1E;

    /* Tuning setting for CPU (0x61) */ 
     data2[3] = 0x67;
    	
	if((dev->tunerid == SONY_ASCOT3_CHIP_ID_2871A)
            && ((aSystem== SONY_ASCOT3_DTV_DVBC_6) || (aSystem== SONY_ASCOT3_DTV_DVBC_8) || (aSystem== SONY_ASCOT3_DTV_J83B_5_6))){
            /* DVB-C (ASCOT3I) (Fref = 1MHz) */
            switch(dev->base->config->tuner_xtal){
            case SONY_ASCOT3_XTAL_16000KHz:
                data2[4] = 0x10;
                break;
            case SONY_ASCOT3_XTAL_20500KHz:
                data2[4] = 0x14;
                break;
            case SONY_ASCOT3_XTAL_24000KHz:
                data2[4] = 0x18;
                break;
            case SONY_ASCOT3_XTAL_41000KHz:
                data2[4] = 0x28;
                break;
            }
        }else{
            /* Digital (Fref = 8MHz) */
            switch(dev->base->config->tuner_xtal){
            case SONY_ASCOT3_XTAL_16000KHz:
                data2[4] = 0x02;
                break;
            case SONY_ASCOT3_XTAL_20500KHz:
                data2[4] = 0x02;
                break;
            case SONY_ASCOT3_XTAL_24000KHz:
                data2[4] = 0x03;
                break;
            case SONY_ASCOT3_XTAL_41000KHz:
                data2[4] = 0x05;
                break;
            }
        }
		if((aSystem == SONY_ASCOT3_DTV_DVBC_6) || (aSystem == SONY_ASCOT3_DTV_DVBC_8) || (aSystem == SONY_ASCOT3_DTV_J83B_5_6)){
                if(dev->base->config->tuner_xtal == SONY_ASCOT3_XTAL_20500KHz){
                    data2[5] = 0x1C;
                }else{
                    data2[5] = 0x1C;
                }
                data2[6] = 0x78;
                data2[7] = 0x08;
                data2[8] = 0x1C;
            }else{
                if(dev->base->config->tuner_xtal == SONY_ASCOT3_XTAL_20500KHz){
                    data2[5] = 0x8C;
                }else{
                    data2[5] = 0xB4;
                }
                data2[6] = 0x78;
                data2[7] = 0x08;
                data2[8] = 0x30;
            }
			cxd2878_wrm( dev,dev->tuner_addr,0x5E, data2, 9);
			cxd2878_SetRegisterBits(dev,dev->tuner_addr,0x67,0x00,0x06);
			data3[0] = g_param_table_ascot3i[aSystem].OUTLMT&0x03;
			if(g_param_table_ascot3i[aSystem].RF_GAIN == AUTO)
				data3[1] = 0x80;
			else
				data3[1] = (u8)((g_param_table_ascot3i[aSystem].RF_GAIN<<4) &0x70);
			
			data3[1]|= (u8)(g_param_table_ascot3i[aSystem].IF_BPF_GC&0x0F);
			data3[2] = 0x00;

			if(frequency <= 172000){
				data3[3] = (u8)(g_param_table_ascot3i[aSystem].RFOVLD_DET_LV1_VL & 0x0F);
				data3[4] = (u8)(g_param_table_ascot3i[aSystem].IFOVLD_DET_LV_VL & 0x07);
			}else if(frequency<= 464000){
				data3[3] = (u8)(g_param_table_ascot3i[aSystem].RFOVLD_DET_LV1_VH & 0x0F);
				data3[4] = (u8)(g_param_table_ascot3i[aSystem].IFOVLD_DET_LV_VH & 0x07);
			}else{
				data3[3] = (u8)(g_param_table_ascot3i[aSystem].RFOVLD_DET_LV1_U & 0x0F);
				data3[4] = (u8)(g_param_table_ascot3i[aSystem].IFOVLD_DET_LV_U & 0x07);
			}
			data3[4] |= 0x20;
			
			/* Setting for IF frequency and bandwidth */
			
			/* IF filter center frequency offset (IF_BPF_F0) (0x6D) */
			data3[5] = (u8)((g_param_table_ascot3i[aSystem].IF_BPF_F0 << 4) & 0x30);
			
			/* IF filter band width (BW) (0x6D) */
			data3[5] |= (u8)(g_param_table_ascot3i[aSystem].BW & 0x03);
			
			/* IF frequency offset value (FIF_OFFSET) (0x6E) */
			data3[6] = (u8)(g_param_table_ascot3i[aSystem].FIF_OFFSET & 0x1F);
			
			/* IF band width offset value (BW_OFFSET) (0x6F) */
			data3[7] = (u8)(g_param_table_ascot3i[aSystem].BW_OFFSET & 0x1F);
			
			/* RF tuning frequency setting (0x70, 0x71, 0x72) */
			data3[8]  = (u8)(frequency & 0xFF);		   /* FRF_L */
			data3[9]  = (u8)((frequency >> 8) & 0xFF);  /* FRF_M */
			data3[10] = (u8)((frequency >> 16) & 0x0F); /* FRF_H (bit[3:0]) */

			data3[11] = 0xFF;
			data3[12] = 0x11;
			
			if( (aSystem == SONY_ASCOT3_DTV_DVBC_6) || (aSystem == SONY_ASCOT3_DTV_DVBC_8) || (aSystem == SONY_ASCOT3_DTV_J83B_5_6)){
	
					data3[13] = 0xD9;
					data3[14] = 0x0F;
					data3[15] = 0x24;
					data3[16] = 0x87;
				
			}else{
					data3[13] = 0x99;
					data3[14] = 0x00;
					data3[15] = 0x24;
					data3[16] = 0x87;			
			}
			cxd2878_wrm( dev,dev->tuner_addr,0x68, data3, 17);	

			msleep(50);

			cxd2878_wr(dev,dev->tuner_addr,0x88,0x00);
			cxd2878_wr(dev,dev->tuner_addr,0x87,0xC0);
			msleep(10);
			return 0;
}
static int ascot3_init(struct cxd2878_dev*dev)
{
	int ret;
	u8 data = 0;
	u8 cdata[2] ={0x7A,0x01};
	u8 adata[20];
	ret = cxd2878_rdm(dev,dev->tuner_addr,0x7F,&data,1);
	if(ret)
		goto err;
	switch(data&0xF0){
		case 0xC0:/* ASCOT3 ES1 */
		case 0xD0:/* ASCOT3 ES2 */
			dev->tunerid = SONY_ASCOT3_CHIP_ID_UNKNOWN;
			break;
		case 0xE0:  /* ASCOT3I */
			dev_info(&dev->base->i2c->dev," Deceted the Tuner chip ASCOT3 ,ID 2878A ");
			dev->tunerid = SONY_ASCOT3_CHIP_ID_2871A;
			break;
		}
	//x_pon
	cxd2878_wrm(dev,dev->tuner_addr,0x99,cdata,2);

	switch(dev->base->config->tuner_xtal){
        case SONY_ASCOT3_XTAL_16000KHz:
            adata[0] = 0x10;
            break;
        case SONY_ASCOT3_XTAL_20500KHz:
            adata[0] = 0xD4;
            break;
        case SONY_ASCOT3_XTAL_24000KHz:
            adata[0] = 0x18;
            break;
        case SONY_ASCOT3_XTAL_41000KHz:
            adata[0] = 0x69;
            break;
        }
	
	 adata[1] = 0x84; 
	 adata[2] = 0xA8;
	 adata[3] = 0x82; ///* REFOUT_EN = 1, REFOUT_CNT = 2 */
     /* GPIO0, GPIO1 port setting (0x85, 0x86) */
     /* GPIO setting should be done by sony_ascot3_SetGPO after initialization */
     adata[4] = 0x00;
     adata[5] = 0x00;
 
     /* Clock enable for internal logic block (0x87) */
     adata[6] = 0xC4;
 
     /* Start CPU boot-up (0x88) */
     adata[7] = 0x40;
 
     /* For burst-write (0x89) */
     adata[8] = 0x10;
 
     /* Setting for internal RFAGC (0x8A, 0x8B, 0x8C) */
     adata[9] = 0x00;
   	adata[10] = 0x45;
 	//data[10] = 0x01;  
     adata[11] = 0x75;
   //  data[11] = 0x56;
 
     /* Setting for analog block (0x8D) */
     adata[12] = 0x07;

    /* Initial setting for internal analog block (0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94) */
     adata[13] = 0x1C;
     adata[14] = 0x3F;
     adata[15] = 0x02;
     adata[16] = 0x10;
     adata[17] = 0x20;
     adata[18] = 0x0A;
     adata[19] = 0x00;

	 cxd2878_wrm(dev,dev->tuner_addr,0x81,adata,sizeof(adata));

	 cxd2878_wr(dev,dev->tuner_addr,0x9B,0x00);
	 
	 msleep(10);
	 
	 u8 rdata;
	 cxd2878_rdm(dev,dev->tuner_addr,0x1A,&rdata,1);
	 if(rdata != 0)
	 	return rdata;
	 
	  /* Chip ID auto detection (for CXD2871/2872/2875) */
     if(dev->tunerid == SONY_ASCOT3_CHIP_ID_UNKNOWN){
		 u8 cdata[2] = {0x8C,0x06};
		
		 cxd2878_wrm(dev,dev->tuner_addr,0x17,cdata,sizeof(cdata));
		 msleep(1);
		 cxd2878_rdm(dev,dev->tuner_addr, 0x19, &data, 1);
		 if(data&0x08)
		 	dev->tunerid = SONY_ASCOT3_CHIP_ID_2875;

		 if(dev->tunerid == SONY_ASCOT3_CHIP_ID_UNKNOWN){
			  u8 cdata[2] = {0x96, 0x06}; /* 0x17, 0x18 */
			 cxd2878_wrm(dev,dev->tuner_addr,0x17,cdata,sizeof(cdata));
			 msleep(1);
			 cxd2878_rdm(dev,dev->tuner_addr, 0x19, &data, 1);
			 if(data&0x40)
			 	dev->tunerid = SONY_ASCOT3_CHIP_ID_2871;
			 else
			 	dev->tunerid = SONY_ASCOT3_CHIP_ID_2872;

		 }
	 } 
	  /* VCO current setting */
	  if(dev->tunerid ==SONY_ASCOT3_CHIP_ID_2871A){
	  	 u8 cdata[2] = {0x2A, 0x0E}; /* 0x17, 0x18 */
		 cxd2878_wrm(dev,dev->tuner_addr,0x17,cdata,sizeof(cdata));
		 msleep(1);
		 cxd2878_rdm(dev,dev->tuner_addr, 0x19, &data, 1);
		 cxd2878_wr(dev,dev->tuner_addr,0x95, (data&0x0F));
	  }
	  else{
		  u8 cdata[2] = {0x8D, 0x06};
		  cxd2878_wrm(dev,dev->tuner_addr,0x17,cdata,sizeof(cdata));
		  msleep(1);
		  cxd2878_rdm(dev,dev->tuner_addr, 0x19, &data, 1);
		  cxd2878_wr(dev,dev->tuner_addr,0x95, (u8)(data>>4));

	  }
	  if(dev->tunerid ==SONY_ASCOT3_CHIP_ID_2871A){ 
		  cxd2878_wr(dev,dev->tuner_addr, 0xB0, 0x00);	  
		  cxd2878_wr(dev,dev->tuner_addr, 0x30, 0xE0);
		  cxd2878_wr(dev,dev->tuner_addr, 0xB1, 0x1E);	  
		  cxd2878_wr(dev,dev->tuner_addr, 0xB3, 0x02);
		  msleep(1);
		  cxd2878_rdm(dev,dev->tuner_addr, 0xB4, &data, 1);	  
		  cxd2878_wr(dev,dev->tuner_addr, 0xB3, 0x00);
		  msleep(1);
		  cxd2878_wr(dev,dev->tuner_addr, 0xB1, 0x00);
		  cxd2878_wr(dev,dev->tuner_addr, 0x30, 0xE1);	  
		  cxd2878_wr(dev,dev->tuner_addr, 0xB0, 0x01);
	  }

	  //x_fine
	    /* Keep RF_EXT bit */
      cxd2878_SetRegisterBits(dev,dev->tuner_addr, 0x67, 0x00, 0xFE);
	      /* Disable IF signal output (IF_OUT_SEL setting) (0x74) */
      cxd2878_SetRegisterBits(dev,dev->tuner_addr, 0x74, 0x02, 0x03);

	  u8 cdata1[3]={0x15, 0x00, 0x00};
	  cxd2878_wrm(dev,dev->tuner_addr,0x5E,cdata1,sizeof(cdata1));
      /* Standby setting for CPU (0x88) */
      cxd2878_wr(dev,dev->tuner_addr, 0x88, 0x00);
    
      /* Standby setting for internal logic block (0x87) */
      cxd2878_wr(dev,dev->tuner_addr, 0x87, 0xC0);
      /* Load capacitance control setting for crystal oscillator (0x80) */
      cxd2878_wr(dev,dev->tuner_addr, 0x80, 0x01);	

	  return 0;
err:
	dev_err(&dev->base->i2c->dev,"%s: Tuner ASCOT3 i2c error !",KBUILD_MODNAME);
	return ret;
}
static int cxd2878_setstreamoutput(struct cxd2878_dev*dev,int enable)
{
	int ret;
	u8 data = 0;
    /* slave    Bank    Addr    Bit    default    Name
     * ---------------------------------------------------
     * <SLV-T>  00h     A9h     [1:0]  2'b0       OREG_TSTLVALPSEL
     */

    /* Set SLV-T Bank : 0x00 */
    if (cxd2878_wr (dev,dev->slvt , 0x00, 0x00) != 0) {
       goto err;
    }
    if (cxd2878_rdm (dev,dev->slvt, 0xA9, &data, 1) != 0) {
        goto err;
    }

    if ((data & 0x03) == 0x00) {
        /* TS output */
        /* Set SLV-T Bank : 0x00 */
        if (cxd2878_wr (dev,dev->slvt , 0x00, 0x00) != 0) {
            goto err;
        }
        /* Enable TS output */
        if (cxd2878_wr (dev,dev->slvt , 0xC3, enable ? 0x00 : 0x01) != 0) {
            goto err;
        }
    }
		return 0;
	err:
		dev_err(&dev->base->i2c->dev,"%s: cxd2878_setstreamoutput error !",KBUILD_MODNAME);
		return ret; 

}
static int cxd2878_setTSClkModeAndFreq(struct cxd2878_dev *dev)
{
	int ret;
	u8 serialTS;
	u8 tsRateCtrlOff = 0;
	
    struct  sony_demod_ts_clk_configuration_t tsClkConfiguration;
   
    struct sony_demod_ts_clk_configuration_t serialTSClkSettings [2][6] =
    {{ /* Gated Clock */
       /* OSERCKMODE  OSERDUTYMODE  OTSCKPERIOD  OREG_CKSEL_TSTLVIF                         */
        {      3,          1,            8,             0        }, /* High Freq, full rate */
        {      3,          1,            8,             1        }, /* Mid Freq,  full rate */
        {      3,          1,            8,             2        }, /* Low Freq,  full rate */
        {      0,          2,            16,            0        }, /* High Freq, half rate */
        {      0,          2,            16,            1        }, /* Mid Freq,  half rate */
        {      0,          2,            16,            2        }  /* Low Freq,  half rate */
    },
    {  /* Continuous Clock */
       /* OSERCKMODE  OSERDUTYMODE  OTSCKPERIOD  OREG_CKSEL_TSTLVIF                         */
        {      1,          1,            8,             0        }, /* High Freq, full rate */
        {      1,          1,            8,             1        }, /* Mid Freq,  full rate */
        {      1,          1,            8,             2        }, /* Low Freq,  full rate */
        {      2,          2,            16,            0        }, /* High Freq, half rate */
        {      2,          2,            16,            1        }, /* Mid Freq,  half rate */
        {      2,          2,            16,            2        }  /* Low Freq,  half rate */
    }};

    struct sony_demod_ts_clk_configuration_t parallelTSClkSetting =
    {  /* OSERCKMODE  OSERDUTYMODE  OTSCKPERIOD  OREG_CKSEL_TSTLVIF */
               0,          0,            8,             1
    };
    /* NOTE: For ISDB-S3, OREG_CKSEL_TSTLVIF should be 1 */

//    struct sony_demod_ts_clk_configuration_t backwardsCompatibleSerialTSClkSetting [2] =
 //   {  /* OSERCKMODE  OSERDUTYMODE  OTSCKPERIOD  OREG_CKSEL_TSTLVIF                         */
 //       {      3,          1,            8,             1        }, /* Gated Clock          */
  //      {      1,          1,            8,             1        }  /* Continuous Clock     */
 //   };

//    struct sony_demod_ts_clk_configuration_t backwardsCompatibleParallelTSClkSetting =
//    {  /* OSERCKMODE  OSERDUTYMODE  OTSCKPERIOD  OREG_CKSEL_TSTLVIF */
//               0,          0,            8,             1
 //   };	
	
	ret = cxd2878_wr(dev,dev->slvt,0x00,0x00);
	if(ret)
		goto err;
	ret = cxd2878_rdm(dev,dev->slvt, 0xC4, &serialTS, 1);
	if(ret)
		goto err;
	if((dev->system ==SONY_DTV_SYSTEM_ISDBT)||(dev->system == SONY_DTV_SYSTEM_ISDBC)||(dev->system == SONY_DTV_SYSTEM_ATSC))
		tsRateCtrlOff = 1;
	
	cxd2878_SetRegisterBits(dev,dev->slvt,0xD3, tsRateCtrlOff, 0x01);
	cxd2878_SetRegisterBits(dev,dev->slvt,0xDE, 0x00, 0x01);
	cxd2878_SetRegisterBits(dev,dev->slvt,0xDA, 0x00, 0x01);	
	if (serialTS & 0x80) {
	        /* Serial TS */
	        /* Intentional fall through */
	        tsClkConfiguration = serialTSClkSettings[1][1];
	    }
	    else {
	        /* Parallel TS */
	        tsClkConfiguration = parallelTSClkSetting;
	        tsClkConfiguration.tsClkPeriod = 0x08;
	    }	

	  	if (serialTS & 0x80) {
	  		/* Serial TS, so set serial TS specific registers */
	  
	  		/* slave	Bank	Addr	Bit    default	  Name
	  		 * -----------------------------------------------------
	  		 * <SLV-T>	00h 	C4h 	[1:0]  2'b01	  OSERCKMODE
	  		 */
	  		cxd2878_SetRegisterBits(dev,dev->slvt, 0xC4, tsClkConfiguration.serialClkMode, 0x03);
	  
	  
	  		/* slave	Bank	Addr	Bit    default	  Name
	  		 * -------------------------------------------------------
	  		 * <SLV-T>	00h 	D1h 	[1:0]  2'b01	  OSERDUTYMODE
	  		 */
	  		cxd2878_SetRegisterBits(dev,dev->slvt, 0xD1, tsClkConfiguration.serialDutyMode, 0x03);
	  	}

	ret = cxd2878_wr(dev,dev->slvt,0xD9, tsClkConfiguration.tsClkPeriod);
	if(ret)
		goto err;
    /* Disable TS IF Clock */
    /* slave    Bank    Addr    Bit    default    Name
     * -------------------------------------------------------
     * <SLV-T>  00h     32h     [0]    1'b1       OREG_CK_TSTLVIF_EN
     */
    cxd2878_SetRegisterBits(dev,dev->slvt, 0x32, 0x00, 0x01);


    /* slave    Bank    Addr    Bit    default    Name
     * -------------------------------------------------------
     * <SLV-T>  00h     33h     [1:0]  2'b01      OREG_CKSEL_TSTLVIF
     */
    cxd2878_SetRegisterBits(dev,dev->slvt, 0x33, tsClkConfiguration.clkSelTSIf, 0x03);

    /* Enable TS IF Clock */
    /* slave    Bank    Addr    Bit    default    Name
     * -------------------------------------------------------
     * <SLV-T>  00h     32h     [0]    1'b1       OREG_CK_TSTLVIF_EN
     */
    cxd2878_SetRegisterBits(dev,dev->slvt, 0x32, 0x01, 0x01);
         /* Set parity period enable / disable based on backwards compatible TS configuration.
         * These registers are set regardless of broadcasting system for simplicity.
         */
            /* Enable parity period for DVB-T */
        /* Set SLV-T Bank : 0x10 */
		 ret = cxd2878_wr(dev,dev->slvt,0x00, 0x10);
		 if(ret)
			 goto err;

        /* slave    Bank    Addr    Bit    default    Name
         * ---------------------------------------------------------------
         * <SLV-T>  10h     66h     [0]    1'b1       OREG_TSIF_PCK_LENGTH
         */
       cxd2878_SetRegisterBits(dev,dev->slvt, 0x66, 0x01, 0x01);
 
        /* Enable parity period for DVB-C (but affect to ISDB-C/J.83B) */
        /* Set SLV-T Bank : 0x40 */
		ret = cxd2878_wr(dev,dev->slvt,0x00, 0x40);
		if(ret)
			goto err;

        /* slave    Bank    Addr    Bit    default    Name
         * ---------------------------------------------------------------
         * <SLV-T>  40h     66h     [0]    1'b1       OREG_TSIF_PCK_LENGTH
         */
       cxd2878_SetRegisterBits(dev,dev->slvt, 0x66, 0x01, 0x01);

	 return 0;
err:
	dev_err(&dev->base->i2c->dev,"%s: set TSClkModeAndFreq error !",KBUILD_MODNAME);
	return ret;

}
static int cxd2878_setTSDataPinHiZ(struct cxd2878_dev*dev,u8 enable)
{
	u8 data = 0,tsDataMask = 0;
	int ret = 0;

    /* slave    Bank    Addr    Bit    default    Name
     * ---------------------------------------------------
     * <SLV-T>  00h     A9h     [0]    1'b0       OREG_TSTLVSEL
     *
     * <SLV-T>  00h     C4h     [7]    1'b0       OSERIALEN
     * <SLV-T>  00h     C4h     [3]    1'b1       OSEREXCHGB7
     *
     * <SLV-T>  01h     C1h     [7]    1'b0       OTLV_SERIALEN
     * <SLV-T>  01h     C1h     [3]    1'b1       OTLV_SEREXCHGB7
     * <SLV-T>  01h     CFh     [0]    1'b0       OTLV_PAR2SEL
     * <SLV-T>  01h     EAh     [6:4]  3'b1       OTLV_PAR2_B1SET
     * <SLV-T>  01h     EAh     [2:0]  3'b0       OTLV_PAR2_B0SET
     */	
	/* Set SLV-T Bank : 0x00 */
	if (cxd2878_wr(dev,dev->slvt, 0x00, 0x00) != 0) {
		goto err;
	}
	
	if (cxd2878_rdm(dev,dev->slvt, 0xA9, &data, 1) != 0) {
		goto err;
	}
	
	if (data & 0x01) {
		/* TLV output */
		/* Set SLV-T Bank : 0x01 */
		if (cxd2878_wr(dev,dev->slvt, 0x00, 0x01) != 0) {
			goto err;
		}
	
		if (cxd2878_rdm (dev,dev->slvt, 0xC1, &data, 1) != 0) {
			goto err;
		}
	
		switch (data & 0x88) {
		case 0x80:
			/* Serial TLV, output from TSDATA0 */
			tsDataMask = 0x01;
			break;
		case 0x88:
			/* Serial TLV, output from TSDATA7 */
			tsDataMask = 0x80;
			break;
		case 0x08:
		case 0x00:
		default:
			/* Parallel TLV */
			if (cxd2878_rdm (dev,dev->slvt, 0xCF, &data, 1) != 0) {
				goto err;
			}
			if (data & 0x01) {
				/* TLV 2bit-parallel */
				if (cxd2878_rdm (dev,dev->slvt, 0xEA, &data, 1) != 0) {
					goto err;
				}
				tsDataMask = (0x01 << (data & 0x07)); /* LSB pin */
				tsDataMask |= (0x01 << ((data >> 4) & 0x07)); /* MSB pin */
			} else {
				/* TLV 8bit-parallel */
				tsDataMask = 0xFF;
			}
			break;
		}
	} else
	
	{
		/* TS output */
		if (cxd2878_rdm ( dev,dev->slvt, 0xC4, &data, 1) != 0) {
			goto err;
		}
	
		switch (data & 0x88) {
		case 0x80:
			/* Serial TS, output from TSDATA0 */
			tsDataMask = 0x01;
			break;
		case 0x88:
			/* Serial TS, output from TSDATA7 */
			tsDataMask = 0x80;
			break;
		case 0x08:
		case 0x00:
		default:
			/* Parallel TS */
			tsDataMask = 0xFF;
			break;
		}
	}
	/* slave	Bank	Addr	Bit    default	  Name
	 * ---------------------------------------------------
	 * <SLV-T>	 00h	81h    [7:0]	8'hFF	OREG_TSDATA_HIZ
	 */
	/* Set SLV-T Bank : 0x00 */
	if (cxd2878_wr (dev,dev->slvt ,0x00, 0x00) != 0) {
		goto err;
	}
	
	if (cxd2878_SetRegisterBits (dev,dev->slvt, 0x81, (u8) (enable ? 0xFF : 0x00), tsDataMask) != 0) {
		goto err;
	}

	return 0;
err:
	dev_err(&dev->base->i2c->dev,"%s: cxd2878_setTSDataPinHiZ error !",KBUILD_MODNAME);
	return ret;	
	
}
static int cxd2878_sleep(struct cxd2878_dev *dev)
{
	u8 data[4];

	if(dev->state == SONY_DEMOD_STATE_ACTIVE){
		
		cxd2878_setstreamoutput(dev,0);
		cxd2878_wr(dev,dev->slvt,0x00,0x00);
		cxd2878_SetRegisterBits(dev,dev->slvt,0x80, 0x1F, 0x1F);
		cxd2878_setTSDataPinHiZ(dev,1);
	

        switch (dev->system) {
        case SONY_DTV_SYSTEM_DVBT:
			/* Cancel DVB-T Demod parameter setting*/
            	cxd2878_wr(dev,dev->slvt,0x00,0x17);
				data[0] = 0x01;
				data[1] = 0x02;
				cxd2878_wrm(dev,dev->slvt,0x38,data,2);
				cxd2878_wr(dev,dev->slvt,0x00,0x18);
				cxd2878_wr(dev,dev->slvt,0x31,0x00);
            break;
        case SONY_DTV_SYSTEM_DVBT2:
            //sony_demod_dvbt2_Sleep (dev);
            // Cancel DVB-T2 setting
            cxd2878_wr(dev,dev->slvt,0x00,0x10);
			cxd2878_wr(dev,dev->slvt,0xA5,0x01);
			cxd2878_wr(dev,dev->slvt,0x00,0x13);
			cxd2878_wr(dev,dev->slvt,0x83,0x40);
			cxd2878_wr(dev,dev->slvt,0x86,0x21);
			cxd2878_wr(dev,dev->slvt,0x9F,0xFB);
            break;
        case SONY_DTV_SYSTEM_DVBC:
            //cancel DVB-C Demod parameter setting
            cxd2878_wr(dev,dev->slvt,0x00,0x11);
			cxd2878_wr(dev,dev->slvt,0xA3,0x00);
			cxd2878_wr(dev,dev->slvt,0x00,0x48);
			cxd2878_wr(dev,dev->slvt,0x2C,0x01);
            break;
        case SONY_DTV_SYSTEM_ISDBT:
           // sony_demod_isdbt_Sleep (dev);
           cxd2878_wr(dev,dev->slvt,0x00,0x10);
		   cxd2878_wr(dev,dev->slvt,0x69,0x05);
		   cxd2878_wr(dev,dev->slvt,0x6B,0x07);
		   cxd2878_wr(dev,dev->slvt,0x9D,0x14);
		   cxd2878_wr(dev,dev->slvt,0xD3,0x00);
		   cxd2878_wr(dev,dev->slvt,0xED,0x01);
		   cxd2878_wr(dev,dev->slvt,0xE2,0x4E);
		   cxd2878_wr(dev,dev->slvt,0xF2,0x03);
		   cxd2878_wr(dev,dev->slvt,0xDE,0x32);
		   cxd2878_wr(dev,dev->slvt,0x00,0x15);
		   cxd2878_wr(dev,dev->slvt,0xDE,0x03);
		   cxd2878_wr(dev,dev->slvt,0x00,0x17);
		   data[0] = 0x01;
		   data[1] = 0x02;
		   cxd2878_wrm(dev,dev->slvt,0x38,data,2);
		   cxd2878_wr(dev,dev->slvt,0x00,0x1E);
		   cxd2878_wr(dev,dev->slvt,0x73,0x00);
		   cxd2878_wr(dev,dev->slvt,0x00,0x63);
		   cxd2878_wr(dev,dev->slvt,0x81,0x01);
            break;
        case SONY_DTV_SYSTEM_ISDBC:
           cxd2878_wr(dev,dev->slvt,0x00,0x11);
		   cxd2878_wr(dev,dev->slvt,0xA3,0x00);
		   cxd2878_wr(dev,dev->slvt,0x00,0x48);
		   cxd2878_wr(dev,dev->slvt,0x2C,0x01);
		   cxd2878_wr(dev,dev->slvt,0x00,0x40);
		   cxd2878_wr(dev,dev->slvt,0x14,0x1F);
		   cxd2878_wr(dev,dev->slvt,0x1E,0x00);
            break;
        case SONY_DTV_SYSTEM_J83B:
           cxd2878_wr(dev,dev->slvt,0x00,0x11);
		   cxd2878_wr(dev,dev->slvt,0xA3,0x00);
		   cxd2878_wr(dev,dev->slvt,0x00,0x40);
		   cxd2878_wr(dev,dev->slvt,0x21,0x00);
		   cxd2878_wr(dev,dev->slvt,0xC3,0x00);
		   cxd2878_wr(dev,dev->slvt,0xB3,0x02);
		   cxd2878_wr(dev,dev->slvt,0x1E,0x00);
		   cxd2878_wr(dev,dev->slvt,0x8E,0x0E);
		   cxd2878_wr(dev,dev->slvt,0x00,0x41);
		   cxd2878_wr(dev,dev->slvt,0xCF,0x77);
		   cxd2878_wr(dev,dev->slvt,0x00,0x40);
		   cxd2878_wr(dev,dev->slvt,0x14,0x1F);
		   data[0]=0x09;
		   data[1]=0x9A;
		   data[2]=0x00;
		   data[3]=0xEE;
		   cxd2878_wrm(dev,dev->slvt,0x26,data,4);
            break;

        case SONY_DTV_SYSTEM_ATSC:          
		   cxd2878_wr(dev,dev->slvr,0x00,0x01);          
		   cxd2878_wr(dev,dev->slvr,0x48,0x01);		   
		   cxd2878_wr(dev,dev->slvt,0x00,0x00);	   
		   cxd2878_wr(dev,dev->slvt,0xD3,0x00);
            break;
        /* Intentional fall-through */
        case SONY_DTV_SYSTEM_UNKNOWN:
        default:
            break;
        }
	}
	 /* Set SLV-X Bank : 0x00 */
	cxd2878_wr(dev,dev->slvx,0x00,0x00);
	/* TADC setting */
	cxd2878_wr(dev,dev->slvx,0x18,0x01);
	/* Set SLV-T Bank : 0x00 */
	cxd2878_wr(dev,dev->slvt,0x00,0x00);
	/* TADC setting */
	cxd2878_wr(dev,dev->slvt,0x49,0x33);
	/* TADC setting */
	cxd2878_wr(dev,dev->slvt,0x4B,0x21);
	/* Demodulator SW reset */
	cxd2878_wr(dev,dev->slvt,0xFE,0x01);
	/* Disable demodulator clock */
	cxd2878_wr(dev,dev->slvt,0x2C,0x00);
	/* Set tstlv mode to default */	
	cxd2878_wr(dev,dev->slvt,0xA9,0x00);
	/* Set demodulator mode to default */
	cxd2878_wr(dev,dev->slvx,0x17,0x01);
	
    dev->state = SONY_DEMOD_STATE_SLEEP;
    dev->system = SONY_DTV_SYSTEM_UNKNOWN;	

	return 0;
}
static int cxd2878_tuneEnd(struct cxd2878_dev *dev)
{
	cxd2878_wr(dev,dev->slvt,0x00,0x00);
	cxd2878_wr(dev,dev->slvt,0xFE,0x01);
	cxd2878_setstreamoutput(dev,1);
	
	return 0;
}
static int SLtoAT_BandSetting(struct cxd2878_dev *dev)
{
	int ret = 0;
	u8 bandtmp[3];
	u8 data[2]={0x01,0x14};
	u8 dataxD9[2] = {0x15,0x28};
	u8 datax38[2] = {0x01,0x02};
	u8 dataxD9_1[2] = {0x1F,0xF8};
	u8 datax38_1[2] = {0x00,0x03};
	u8 dataxD9_2[2] = {0x25,0x4C};
	u8 datax38_2[2] = {0x00,0x03};
	u8 dataxD9_3[2] = {0x2C,0xC2};
	u8 datax38_3[2] = {0x00,0x03};
	u8 nominalRate_8M[5] = {0x15,0x00,0x00,0x00,0x00};
 	u8 itbCoef_8M[14] = {
            /*  COEF01 COEF02 COEF11 COEF12 COEF21 COEF22 COEF31 COEF32 COEF41 COEF42 COEF51 COEF52 COEF61 COEF62 */
                0x2F,  0xBA,  0x28,  0x9B,  0x28,  0x9D,  0x28,  0xA1,  0x29,  0xA5,  0x2A,  0xAC,  0x29,  0xB5
            };
	u8 nominalRate_7M[5] = {
        /* TRCG Nominal Rate [37:0] */
        0x18, 0x00, 0x00, 0x00, 0x00
    };
	u8 itbCoef_7M[14] = {
    /*  COEF01 COEF02 COEF11 COEF12 COEF21 COEF22 COEF31 COEF32 COEF41 COEF42 COEF51 COEF52 COEF61 COEF62 */
        0x30,  0xB1,  0x29,  0x9A,  0x28,  0x9C,  0x28,  0xA0,  0x29,  0xA2,  0x2B,  0xA6,  0x2B,  0xAD
    };
	u8 nominalRate_6M[5] = {
                /* TRCG Nominal Rate [37:0] */
                0x1C, 0x00, 0x00, 0x00, 0x00
            };
	u8 itbCoef_6M[14] = {
			/*	COEF01 COEF02 COEF11 COEF12 COEF21 COEF22 COEF31 COEF32 COEF41 COEF42 COEF51 COEF52 COEF61 COEF62 */
				0x31,  0xA8,  0x29,  0x9B,	0x27,  0x9C,  0x28,  0x9E,	0x29,  0xA4,  0x29,  0xA2,	0x29,  0xA8
			};
	u8 nominalRate_5M[5] = {
        /* TRCG Nominal Rate [37:0] */
        0x21, 0x99, 0x99, 0x99, 0x99
    };
	u8 itbCoef_5M[14] = {
        /*  COEF01 COEF02 COEF11 COEF12 COEF21 COEF22 COEF31 COEF32 COEF41 COEF42 COEF51 COEF52 COEF61 COEF62 */
            0x31,  0xA8,  0x29,  0x9B,  0x27,  0x9C,  0x28,  0x9E,  0x29,  0xA4,  0x29,  0xA2,  0x29,  0xA8
        };
	ret = cxd2878_wr(dev,dev->slvt,0x00,0x13);
	if(ret)goto err;
	
	u8 data[2]={0x01,0x14};
	cxd2878_wrm(dev,dev->slvt,0x9C,data,2);

	cxd2878_wr(dev,dev->slvt,0x00,0x10);

	switch(dev->bandwidth){
	case SONY_DTV_BW_8_MHZ:
        cxd2878_wrm(dev,dev->slvt,0x9F,nominalRate_8M,5);      
		cxd2878_wrm(dev,dev->slvt,0xA6,itbCoef_8M,14);	
		
		bandtmp[0] = (u8)((dev->iffreqConfig.configDVBT_8>>16)&0xFF);
		bandtmp[1] = (u8) ((dev->iffreqConfig.configDVBT_8 >> 8) & 0xFF);
        bandtmp[2] = (u8) (dev->iffreqConfig.configDVBT_8 & 0xFF);
		
		cxd2878_wrm(dev,dev->slvt,0xB6,bandtmp,3); //if freq setting
		cxd2878_wr(dev,dev->slvt,0xD7,0x00); //system bandwith setting
		cxd2878_wrm(dev,dev->slvt,0xD9,dataxD9,2);
		cxd2878_wr(dev,dev->slvt,0x00,0x17);
		cxd2878_wrm(dev,dev->slvt,0x38,datax38,2);		
		break;
	case SONY_DTV_BW_7_MHZ:
		cxd2878_wrm(dev,dev->slvt,0x9F,nominalRate_7M,5);
		cxd2878_wrm(dev,dev->slvt,0xA6,itbCoef_7M,14);
		
		bandtmp[0] = (u8)((dev->iffreqConfig.configDVBT_7>>16)&0xFF);
		bandtmp[1] = (u8) ((dev->iffreqConfig.configDVBT_7 >> 8) & 0xFF);
        bandtmp[2] = (u8) (dev->iffreqConfig.configDVBT_7 & 0xFF);
		cxd2878_wrm(dev,dev->slvt,0xB6,bandtmp,3); //if freq setting
		
		cxd2878_wr(dev,dev->slvt,0xD7,0x02); //system bandwith setting
		cxd2878_wrm(dev,dev->slvt,0xD9,dataxD9_1,2);
		cxd2878_wr(dev,dev->slvt,0x00,0x17);
		cxd2878_wrm(dev,dev->slvt,0x38,datax38_1,2);
		break;
	case SONY_DTV_BW_6_MHZ:

		cxd2878_wrm(dev,dev->slvt,0x9F,nominalRate_6M,5);
		cxd2878_wrm(dev,dev->slvt,0xA6,itbCoef_6M,14);	

		bandtmp[0] = (u8)((dev->iffreqConfig.configDVBT_6>>16)&0xFF);
		bandtmp[1] = (u8) ((dev->iffreqConfig.configDVBT_6 >> 8) & 0xFF);
        bandtmp[2] = (u8) (dev->iffreqConfig.configDVBT_6 & 0xFF);
		cxd2878_wrm(dev,dev->slvt,0xB6,bandtmp,3); //if freq setting
		
		cxd2878_wr(dev,dev->slvt,0xD7,0x04); //system bandwith setting
		cxd2878_wrm(dev,dev->slvt,0xD9,dataxD9_2,2);
		cxd2878_wr(dev,dev->slvt,0x00,0x17);
		cxd2878_wrm(dev,dev->slvt,0x38,datax38_2,2);
		break;
	case SONY_DTV_BW_5_MHZ:
		cxd2878_wrm(dev,dev->slvt,0x9F,nominalRate_5M,5);
		cxd2878_wrm(dev,dev->slvt,0xA6,itbCoef_5M,14);	
		bandtmp[0] = (u8)((dev->iffreqConfig.configDVBT_6>>16)&0xFF);
		bandtmp[1] = (u8) ((dev->iffreqConfig.configDVBT_6 >> 8) & 0xFF);
        bandtmp[2] = (u8) (dev->iffreqConfig.configDVBT_6 & 0xFF);
		cxd2878_wrm(dev,dev->slvt,0xB6,bandtmp,3); //if freq setting
		cxd2878_wr(dev,dev->slvt,0xD7,0x06); //system bandwith setting
		u8 dataxD9_3[2] = {0x2C,0xC2};
		cxd2878_wrm(dev,dev->slvt,0xD9,dataxD9_3,2);
		cxd2878_wr(dev,dev->slvt,0x00,0x17);
		u8 datax38_3[2] = {0x00,0x03};
		cxd2878_wrm(dev,dev->slvt,0x38,datax38_3,2);
		break;
	default:
		goto err;
	}

	
	return 0;
	
err:
	dev_err(&dev->base->i2c->dev,"%s: SLtoAT_BandSetting error !",KBUILD_MODNAME);
	return ret;		
}
static int SLtoAT(struct cxd2878_dev*dev)
{
	int ret = 0;
	u8 data[] = {0x01,0x01};
	u8 datax33[]= {0x00,0x03,0x3B};

	ret = cxd2878_setTSClkModeAndFreq(dev);
	if(ret)
		goto err;

	ret = cxd2878_wr(dev,dev->slvx,0x00,0x00);
	if(ret)goto err;
	
	cxd2878_wr(dev,dev->slvx,0x17,0x01);
	cxd2878_wr(dev,dev->slvt,0x00,0x00);
	cxd2878_wr(dev,dev->slvt,0xA9,0x00);
	cxd2878_wr(dev,dev->slvt,0x2C,0x01);
	cxd2878_wr(dev,dev->slvt,0x4B,0x74);
	cxd2878_wr(dev,dev->slvt,0x49,0x00);
	cxd2878_wr(dev,dev->slvx,0x18,0x00);
	cxd2878_wr(dev,dev->slvt,0x00,0x11);
	cxd2878_wr(dev,dev->slvt,0x6A,0x50);
	cxd2878_wr(dev,dev->slvt,0x00,0x10);
	cxd2878_wr(dev,dev->slvt,0xA5,0x01);
	cxd2878_wr(dev,dev->slvt,0x00,0x18);
	cxd2878_wr(dev,dev->slvt,0x31,0x01);
	cxd2878_wr(dev,dev->slvt,0x00,0x00);

	cxd2878_wrm(dev,dev->slvt,0xCE,data,2);
	
	cxd2878_wr(dev,dev->slvt,0x00,0x11);
	cxd2878_wrm(dev,dev->slvt,0x33,datax33,3);
	
	ret |= SLtoAT_BandSetting(dev);
	cxd2878_wr(dev,dev->slvt,0x00,0x00);

	ret |= cxd2878_SetRegisterBits(dev,dev->slvt,0x80,0x08,0x1F);
	ret |= cxd2878_setTSDataPinHiZ(dev,0);
	if(ret)
		goto err;
	
	return 0;
	dev->slvt   = config->addr_slvt;
	dev->slvx	= config->addr_slvt+2;
	dev->slvr	= config->addr_slvt-0x20;
	dev->slvm	= config->addr_slvt-0x54;
	dev->tuner_addr = config->tuner_addr;

	dev->state	= SONY_DEMOD_STATE_UNKNOWN;
	dev->system	= SONY_DTV_SYSTEM_UNKNOWN;
	
	dev->iffreqConfig.configDVBT_5 = SONY_DEMOD_MAKE_IFFREQ_CONFIG(3.6);
	dev->iffreqConfig.configDVBT_6 = SONY_DEMOD_MAKE_IFFREQ_CONFIG(3.6);
	dev->iffreqConfig.configDVBT_7 = SONY_DEMOD_MAKE_IFFREQ_CONFIG(4.2);
	dev->iffreqConfig.configDVBT_8 = SONY_DEMOD_MAKE_IFFREQ_CONFIG(4.8);

	dev->iffreqConfig.configDVBT2_1_7 = SONY_DEMOD_MAKE_IFFREQ_CONFIG(3.5);
	dev->iffreqConfig.configDVBT2_5 = SONY_DEMOD_MAKE_IFFREQ_CONFIG(3.6);
	dev->iffreqConfig.configDVBT2_6 = SONY_DEMOD_MAKE_IFFREQ_CONFIG(3.6);
	dev->iffreqConfig.configDVBT2_7 = SONY_DEMOD_MAKE_IFFREQ_CONFIG(4.2);
	dev->iffreqConfig.configDVBT2_8 = SONY_DEMOD_MAKE_IFFREQ_CONFIG(4.8);

	dev->iffreqConfig.configDVBC_6 = SONY_DEMOD_MAKE_IFFREQ_CONFIG(3.7);
	dev->iffreqConfig.configDVBC_7 = SONY_DEMOD_MAKE_IFFREQ_CONFIG(4.9);
	dev->iffreqConfig.configDVBC_8 = SONY_DEMOD_MAKE_IFFREQ_CONFIG(4.9);

	dev->iffreqConfig.configATSC = SONY_DEMOD_ATSC_MAKE_IFFREQ_CONFIG(3.7);

	dev->iffreqConfig.configISDBT_6 = SONY_DEMOD_MAKE_IFFREQ_CONFIG(3.55);
	dev->iffreqConfig.configISDBT_7 = SONY_DEMOD_MAKE_IFFREQ_CONFIG(4.15);
	dev->iffreqConfig.configISDBT_8 = SONY_DEMOD_MAKE_IFFREQ_CONFIG(4.75);

	dev->iffreqConfig.configJ83B_5_06_5_36 = SONY_DEMOD_MAKE_IFFREQ_CONFIG(3.7);
	dev->iffreqConfig.configJ83B_5_60 = SONY_DEMOD_MAKE_IFFREQ_CONFIG(3.75);

	dev->atscNoSignalThresh = 0x7FFB61;
	dev->atscSignalThresh = 0x7C4926;
	dev->warm	 = 0;
	
	
	memcpy(&dev->fe.ops,&cxd2878_ops,sizeof(struct dvb_frontend_ops));
	dev->fe.demodulator_priv = dev;
	
	base = match_base(i2c,config->addr_slvt);
	if(base){
		base->count++;
		dev->base = base;
	}else{
		base = kzalloc(sizeof(struct cxd_base),GFP_KERNEL);
		if(!base)
			goto err1;
		base->i2c =i2c;
		base->config = (struct cxd2878_config *)config;
		base->adr =config->addr_slvt;
		base->count = 1;
		mutex_init(&base->i2c_lock);
		dev->base = base;
		list_add(&base->cxdlist,&cxdlist);
	}
	cxd2878_wr(dev,dev->slvx,0x00,0x00);
	cxd2878_rdm(dev,dev->slvx, 0xFB, &data[0], 1);
	cxd2878_rdm(dev,dev->slvx, 0xFD, &data[1], 1);
	
	id = ((data[0] & 0x03) << 8) | data[1];
	
	switch(id){ 
		
		case SONY_DEMOD_CHIP_ID_CXD2856 :  /**< CXD2856 / CXD6800(SiP) */
			dev_info(&i2c->dev,"Detect CXD2856/CXD6800(SiP) chip.");
			break;
		
		case SONY_DEMOD_CHIP_ID_CXD2857 :  /**< CXD2857 */
			dev_info(&i2c->dev,"Detect CXD2857 chip.");
			break;
		case SONY_DEMOD_CHIP_ID_CXD2878 :  /**< CXD2878 / CXD6801(SiP) */
			dev_info(&i2c->dev,"Detect CXD2878/CXD6801(SiP) chip.");
			break;
		case SONY_DEMOD_CHIP_ID_CXD2879 :  /**< CXD2879 */
			dev_info(&i2c->dev,"Detect CXD2879 chip.");
			break;
		case SONY_DEMOD_CHIP_ID_CXD6802	: /**< CXD6802(SiP) */
			dev_info(&i2c->dev,"Detect CXD2878/CXD6802(SiP) chip.");
			break;
		default:
		case SONY_DEMOD_CHIP_ID_UNKNOWN: /**< Unknown */		
			dev_err(&i2c->dev,"%s:Can not decete the chip.\n",KBUILD_MODNAME);
			goto err1;
			break;
	}
	dev->chipid = id;
	
	dev_dbg(&i2c->dev,"%s: attaching frontend successfully.\n",KBUILD_MODNAME);
	
	return &dev->fe;

err1:
	kfree(dev);
err:
	dev_err(&i2c->dev,"%s:error attaching frontend.\n",KBUILD_MODNAME);
	return NULL;

	
}

EXPORT_SYMBOL_GPL(cxd2878_attach);

MODULE_AUTHOR("Davin zhang<Davin@tbsdtv.com>");
MODULE_DESCRIPTION("sony cxd2878 family demodulator driver");
MODULE_LICENSE("GPL");


