/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 5F, No. 36 Taiyuan St.
 * Jhubei City
 * Hsinchu County 302, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2008, Ralink Technology, Inc.
 *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the                         *
 * Free Software Foundation, Inc.,                                       *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                       *
 *************************************************************************

 Module Name:
 rtmp_data.c

 Abstract:
 Ralink USB driver Tx/Rx functions

 Revision History:
 Who			When			What
 --------	----------		----------------------------------------------

 */
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/in.h>

#include "rt_config.h"
#include <net/iw_handler.h>

#include "pdu.h"

uint64_t rts_cts_frame_duration = 0;

extern UCHAR Phy11BGNextRateUpward[]; // defined in mlme.c

UCHAR SNAP_802_1H[] = { 0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00 };
UCHAR SNAP_BRIDGE_TUNNEL[] = { 0xaa, 0xaa, 0x03, 0x00, 0x00, 0xf8 };
UCHAR EAPOL_LLC_SNAP[] = { 0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8e };
UCHAR EAPOL[] = { 0x88, 0x8e };
UCHAR IPX[] = { 0x81, 0x37 };
UCHAR APPLE_TALK[] = { 0x80, 0xf3 };
UCHAR RateIdToPlcpSignal[12] = { 0, /* RATE_1 */1, /* RATE_2 */2, /* RATE_5_5 */
		3, /* RATE_11 */// see BBP spec
		11, /* RATE_6 */15, /* RATE_9 */10, /* RATE_12 */14, /* RATE_18 */// see IEEE802.11a-1999 p.14
		9, /* RATE_24 */13, /* RATE_36 */8, /* RATE_48 */12 /* RATE_54 */}; // see IEEE802.11a-1999 p.14

UCHAR OfdmSignalToRateId[16] = { RATE_54, RATE_54, RATE_54, RATE_54, // OFDM PLCP Signal = 0,  1,  2,  3 respectively
		RATE_54, RATE_54, RATE_54, RATE_54, // OFDM PLCP Signal = 4,  5,  6,  7 respectively
		RATE_48, RATE_24, RATE_12, RATE_6, // OFDM PLCP Signal = 8,  9,  10, 11 respectively
		RATE_54, RATE_36, RATE_18, RATE_9, // OFDM PLCP Signal = 12, 13, 14, 15 respectively
		};
UCHAR default_cwmin[] = { CW_MIN_IN_BITS, CW_MIN_IN_BITS, CW_MIN_IN_BITS - 1,
		CW_MIN_IN_BITS - 2 };
UCHAR default_cwmax[] = { CW_MAX_IN_BITS, CW_MAX_IN_BITS, CW_MIN_IN_BITS,
		CW_MIN_IN_BITS - 1 };
UCHAR default_sta_aifsn[] = { 3, 7, 2, 2 };
UCHAR MapUserPriorityToAccessCategory[8] = { QID_AC_BE, QID_AC_BK, QID_AC_BK,
		QID_AC_BE, QID_AC_VI, QID_AC_VI, QID_AC_VO, QID_AC_VO };

// Macro for rx indication
VOID REPORT_ETHERNET_FRAME_TO_LLC( IN PRTMP_ADAPTER pAd, IN PUCHAR p8023hdr,
		IN PUCHAR pData, IN ULONG DataSize, IN struct net_device *net_dev) {
	struct sk_buff *pSkb;

#ifdef RTMP_EMBEDDED
	if ((pSkb = __dev_alloc_skb(DataSize + LENGTH_802_3 + 2, GFP_DMA|GFP_ATOMIC)) != NULL)
#else
	if ((pSkb = dev_alloc_skb(DataSize + LENGTH_802_3 + 2)) != NULL)
#endif

	{
		pSkb->dev = net_dev;
		skb_reserve(pSkb, 2); // 16 byte align the IP header
		memcpy(skb_put(pSkb, LENGTH_802_3), p8023hdr, LENGTH_802_3);
		memcpy(skb_put(pSkb, DataSize), pData, DataSize);
		pSkb->protocol = eth_type_trans(pSkb, net_dev);

		netif_rx(pSkb);

		pAd->net_dev->last_rx = jiffies;
		pAd->stats.rx_packets++;

		pAd->Counters8023.GoodReceives++;
	}
}

NDIS_STATUS RTMPDecryptPktBySoftware( IN PRTMP_ADAPTER pAd, IN PUCHAR pData,
		IN PRXD_STRUC pRxD) {
	UCHAR KeyIdx = pAd->PortCfg.DefaultKeyId;

	DBGPRINT(RT_DEBUG_INFO, "RTMPDecryptPktBySoftware ==>\n");

	// handle WEP decryption
	if (pAd->PortCfg.WepStatus == Ndis802_11WEPEnabled) {
		UCHAR *pPayload = (UCHAR *) pData + LENGTH_802_11;

		if (RTMPDecryptData(pAd, pPayload, pRxD->DataByteCnt - LENGTH_802_11)
				== FALSE) {
			DBGPRINT(RT_DEBUG_ERROR, "ERROR : Software decrypt WEP data fails.\n");
			return (NDIS_STATUS_FAILURE);
		} else {
			pRxD->DataByteCnt -= 8; //Minus IV[4] & ICV[4]
			pRxD->CipherAlg = CIPHER_WEP64;
		}

		DBGPRINT(RT_DEBUG_INFO, "RTMPDecryptData WEP data Complete \n");
	}
	// handle TKIP decryption
	else if (pAd->PortCfg.WepStatus == Ndis802_11Encryption2Enabled) {
		if (RTMPSoftDecryptTKIP(pAd, pData, pRxD->DataByteCnt, 0,
				&pAd->SharedKey[KeyIdx])) {
			DBGPRINT(RT_DEBUG_INFO, "RTMPSoftDecryptTKIP Complete \n");
			pRxD->DataByteCnt -= 20; //Minus 8 bytes MIC, 8 bytes IV/EIV, 4 bytes ICV
			pRxD->CipherAlg = CIPHER_TKIP;
		} else {
			DBGPRINT(RT_DEBUG_ERROR, "ERROR : RTMPSoftDecryptTKIP Failed\n");
			return (NDIS_STATUS_FAILURE);
		}
	}
	// handle AES decryption
	else if (pAd->PortCfg.WepStatus == Ndis802_11Encryption3Enabled) {
		if (RTMPSoftDecryptAES(pAd, pData, pRxD->DataByteCnt,
				&pAd->SharedKey[KeyIdx])) {
			DBGPRINT(RT_DEBUG_INFO, "RTMPSoftDecryptAES Complete \n");
			pRxD->DataByteCnt -= 16; //8 bytes MIC, 8 bytes IV/EIV (CCMP Header)
			pRxD->CipherAlg = CIPHER_AES;
		} else {
			DBGPRINT(RT_DEBUG_ERROR, "ERROR : RTMPSoftDecryptAES Failed\n");
			return (NDIS_STATUS_FAILURE);
		}
	} else {
		return (NDIS_STATUS_FAILURE);
	}

	return (NDIS_STATUS_SUCCESS);
}

// Enqueue this frame to MLME engine
// We need to enqueue the whole frame because MLME need to pass data type
// information from 802.11 header
// edit by johnli, fix WPAPSK/WPA2PSK bugs for receiving EAPoL fragmentation packets
/* 
 #define REPORT_MGMT_FRAME_TO_MLME(_pAd, _pFrame, _FrameSize, _Rssi, _PlcpSignal)		\
   {																						\
   MlmeEnqueueForRecv(_pAd, (UCHAR)_Rssi, _FrameSize, _pFrame, (UCHAR)_PlcpSignal);   \
   }
 */
#define REPORT_MGMT_FRAME_TO_MLME(_pAd, _p80211hdr, _pFrame, _FrameSize, _Rssi, _PlcpSignal) \
  {									\
    MlmeEnqueueForRecv(_pAd, (PFRAME_802_11)_p80211hdr, (UCHAR)_Rssi, _FrameSize, _pFrame, (UCHAR)_PlcpSignal);	\
  }
// end johnli

// NOTE: we do have an assumption here, that Byte0 and Byte1 always reasid at the same 
//		 scatter gather buffer
NDIS_STATUS Sniff2BytesFromNdisBuffer( IN struct sk_buff *pFirstSkb,
		IN UCHAR DesiredOffset, OUT PUCHAR pByte0, OUT PUCHAR pByte1) {
	PUCHAR pBufferVA;
	ULONG BufferLen, AccumulateBufferLen, BufferBeginOffset;

	pBufferVA = (PVOID) pFirstSkb->data;
	BufferLen = pFirstSkb->len;
	BufferBeginOffset = 0;
	AccumulateBufferLen = BufferLen;

	*pByte0 = *(PUCHAR) (pBufferVA + DesiredOffset - BufferBeginOffset);
	*pByte1 = *(PUCHAR) (pBufferVA + DesiredOffset - BufferBeginOffset + 1);
	return NDIS_STATUS_SUCCESS;
}

__be16 udp_checksum(struct iphdr* iphdr, struct udphdr* udphdr,
		unsigned char* data) {
	__be32 sum = 0;
	__be16 proto = 0x1100; //17 udp
	__be16 data_length = (__be16) ntohs(udphdr->len) - sizeof(struct udphdr);
	__be16 src[2];
	__be16 dest[2];
	__be16 *padded_data;
	int padded_data_length, i;

	if(data_length % 2 != 0)
	padded_data_length = (int) data_length / 2 + 1;
	else
	padded_data_length = (int) data_length / 2;

	padded_data = kmalloc(padded_data_length * sizeof(__be16), GFP_ATOMIC);

	if(!padded_data) {
		printk("%s %s:%u: kmalloc failed to allocate space for padded data in udp checksum. As a result checksum is not calculated.\n", __FILE__, __FUNCTION__, __LINE__);
    return 0;
  }

  padded_data[padded_data_length - 1] = 0;
  memcpy(padded_data,data, data_length);

  src[0] = (__be16) (iphdr->saddr >> 16);
  src[1] = (__be16) (iphdr->saddr);
  dest[0] = (__be16) (iphdr->daddr >> 16);
  dest[1] = (__be16) (iphdr->daddr);

  data_length = (__be16) htons(data_length);


  sum = src[0] + src[1] + dest[0] + dest[1] + proto + udphdr->len + udphdr->source + udphdr->dest + udphdr->len;
  
  for(i = 0; i < padded_data_length; i++)
    sum += padded_data[i];
 
  while(sum >> 16)
    sum = (__be16) (sum & 0xFFFF) + (__be16) (sum >> 16);

  kfree(padded_data);
  
  return (__be16) ~sum;
}

int64_t swap_64bit_word_byte_order(int64_t time) {
	unsigned char* bytes = (unsigned char*) &time;
	uint32_t word;

	memcpy(&word, bytes, sizeof(uint32_t));
	word = htonl(word);
	memcpy(bytes, &word, sizeof(uint32_t));

	memcpy(&word, bytes + sizeof(uint32_t), sizeof(uint32_t));
	word = htonl(word);
	memcpy(bytes + sizeof(uint32_t), &word, sizeof(uint32_t));

	return *((int64_t *) bytes);
}

uint8_t check_port(uint16_t port){
	uint8_t i;
	for(i = 0; i < port_array_count; i++)
		if(port_array[i] == port){
			printk(KERN_EMERG "This port is in port array.\n");
			return 1;
		}
	return 0;
}

/*
 ========================================================================

 Routine	Description:
 This routine classifies outgoing frames into several AC (Access
 Category) and enqueue them into corresponding s/w waiting queues.

 Arguments:
 pAd	Pointer	to our adapter
 pPacket		Pointer to send packet

 Return Value:
 None

 Note:

 ========================================================================
 */NDIS_STATUS RTMPSendPacket( IN PRTMP_ADAPTER pAd, IN struct sk_buff *pSkb) {
	PUCHAR pSrcBufVA;
	UINT AllowFragSize;
	UCHAR NumberOfFrag;
	UCHAR RTSRequired;
	UCHAR QueIdx, UserPriority;
	NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
	PQUEUE_HEADER pTxQueue;
	UCHAR PsMode;
	unsigned long IrqFlags;
	static ULONG OldFailLowValue = 0, OldFailHighValue = 0, OldRetryLowValue =
	  0, OldRetryHighValue = 0;
	static ULONG fails = 0, retries = 0;

	/*Fred's stuff*/
	struct iphdr* iph;
	struct udphdr* udph = NULL;
	uint8_t store_duration = 0;
	static uint8_t last_packet_was_important = 0;
	uint64_t total_time = 0;
	__tp(pdu) *pkt = NULL;
	/*
	 * 802.11g:
	 * SIFS = 10 us
	 * Slot Time = 20 us
	 * DIFS = SIFS + 2 x Slot Time = 50 us
	 *
	 * Without RTS+CTS, a transmission takes exactly (Without retries):
	 * DIFS + TIME TO SEND DATA
	 */

	uint8_t DIFS = 50 /* us */;
	/*==========*/ULONG retry_dif1 = 0, retry_dif2 = 0, fail_dif1 = 0,
			fail_dif2 = 0;

	DBGPRINT(RT_DEBUG_INFO, "====> RTMPSendPacket\n");

	// Prepare packet information structure for buffer descriptor
	pSrcBufVA = (PVOID) pSkb->data;

	// STEP 1. Check for virtual address allocation, it might fail !!!
	if (pSrcBufVA == NULL) {
		// Resourece is low, system did not allocate virtual address
		// return NDIS_STATUS_FAILURE directly to upper layer
		return NDIS_STATUS_FAILURE;
	}

	//
	// Check for multicast or broadcast (First byte of DA)
	//
	if ((*((PUCHAR) pSrcBufVA) & 0x01) != 0) {
		// For multicast & broadcast, there is no fragment allowed
		NumberOfFrag = 1;
	} else {
		// Check for payload allowed for each fragment
		AllowFragSize = (pAd->PortCfg.FragmentThreshold) - LENGTH_802_11
				- LENGTH_CRC;

		// Calculate fragments required
		NumberOfFrag = ((pSkb->len - LENGTH_802_3 + LENGTH_802_1_H)
				/ AllowFragSize) + 1;
		// Minus 1 if the size just match to allowable fragment size
		if (((pSkb->len - LENGTH_802_3 + LENGTH_802_1_H) % AllowFragSize) == 0) {
			NumberOfFrag--;
		}
	}

	// Save fragment number to Ndis packet reserved field
	RTMP_SET_PACKET_FRAGMENTS(pSkb, NumberOfFrag);

	// STEP 2. Check the requirement of RTS:
	//	   If multiple fragment required, RTS is required only for the first fragment
	//	   if the fragment size large than RTS threshold

	if (NumberOfFrag > 1)
		RTSRequired = (pAd->PortCfg.FragmentThreshold
				> pAd->PortCfg.RtsThreshold) ? 1 : 0;
	else
		RTSRequired = (pSkb->len > pAd->PortCfg.RtsThreshold) ? 1 : 0;

	//
	// Remove the following lines to avoid confusion.
	// CTS requirement will not use Flag "RTSRequired", instead moveing the
	// following lines to RTUSBHardTransmit(..)
	//
	// RTS/CTS may also be required in order to protect OFDM frame
	//if ((pAd->PortCfg.TxRate >= RATE_FIRST_OFDM_RATE) &&
	//	OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_BG_PROTECTION_INUSED))
	//	RTSRequired = 1;

	// Save RTS requirement to Ndis packet reserved field
	RTMP_SET_PACKET_RTS(pSkb, RTSRequired);
	RTMP_SET_PACKET_TXRATE(pSkb, pAd->PortCfg.TxRate);

	iph = ip_hdr(pSkb);
	if (iph->protocol == IPPROTO_UDP) {
		udph = (struct udphdr*) (((char*) iph) + (iph->ihl << 2));
		if (check_port(ntohs(udph->dest))) //I don't like to make this hardcoded, but for now it'll have to do.
			store_duration = 1;
	}

	if (OldRetryLowValue != pAd->WlanCounters.RetryCount.vv.LowPart
			|| OldRetryHighValue != pAd->WlanCounters.RetryCount.vv.HighPart) {
		retry_dif1 = pAd->WlanCounters.RetryCount.vv.HighPart
				- OldRetryHighValue;
		retry_dif2 = pAd->WlanCounters.RetryCount.vv.LowPart - OldRetryLowValue;
		//This code is buggy. dif1 and dif2 are ULONG which should have 4 bytes not 8 as I thought.
		/*      if ( ((int64_t) dif2) < 0 )
		 {
		 dif1 -= 1;
		 dif2 = 0xFFFFFFFFFFFFFFFF + (int64_t) dif2;
		 }*/
	}

	OldRetryLowValue = pAd->WlanCounters.RetryCount.vv.LowPart;
	OldRetryHighValue = pAd->WlanCounters.RetryCount.vv.HighPart;

	if (OldFailLowValue != pAd->WlanCounters.RetryCount.vv.LowPart
			|| OldFailHighValue != pAd->WlanCounters.RetryCount.vv.HighPart) {
		fail_dif1 = pAd->WlanCounters.FailedCount.vv.HighPart
				- OldFailHighValue;
		fail_dif2 = pAd->WlanCounters.FailedCount.vv.LowPart - OldFailLowValue;
		//This code is buggy. dif1 and dif2 are ULONG which should have 4 bytes not 8 as I thought.
		/*      if ( ((int64_t) dif2) < 0 )
		 {
		 dif1 -= 1;
		 dif2 = 0xFFFFFFFFFFFFFFFF + (int64_t) dif2;
		 }*/
	}

	OldFailLowValue = pAd->WlanCounters.FailedCount.vv.LowPart;
	OldFailHighValue = pAd->WlanCounters.FailedCount.vv.HighPart;

	if(last_packet_was_important){
	  fails = fail_dif2;
	  retries = retry_dif2;
	  printk(KERN_EMERG "Last packet was important [%df%dr].\n", fails, retries);
	}

	//=====================================================================================
	/*This is how I store values in an application payload*/
	if (store_duration) { //Store duration in packet
		total_time = DIFS + RTMPCalcDuration(
				pAd, RTMP_GET_PACKET_TXRATE (pSkb), pSkb->len); // DIFS + Data

		//First locate the place where duration value should be stored. It should be, after the ip header, plus the udp header + 16 bytes,
		//that is, after 16 bytes of application payload.
		pkt = (__tp(pdu)*) (((char*) iph) + (iph->ihl << 2)
				+ sizeof(struct udphdr));
		pkt->air = swap_64bit_word_byte_order(total_time);

		printk(KERN_EMERG "Time to transmit current packet %llu\n",
				total_time);

		pkt->fails = fails;
		pkt->retries = retries;
		fails = 0;
		retries = 0;

		//Don't forget to recalculte udp checksum
		udph->check = udp_checksum(iph, udph,
					   ((char*) iph) + (iph->ihl << 2) + sizeof(struct udphdr));
		//If udp checksum is 0 then we have to make it 0xFFFF, because 0 disables udp checksum.
		if (!udph->check)
			udph->check = 0xFFFF;

		last_packet_was_important = 1;
	}else
	  last_packet_was_important = 0;
	//===================================================

	//
	// STEP 3. Traffic classification. outcome = <UserPriority, QueIdx>
	//
	UserPriority = 0;
	QueIdx = QID_AC_BE;
	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED)) {
		USHORT Protocol;
		UCHAR LlcSnapLen = 0, Byte0, Byte1;
		do {
			// get Ethernet protocol field
			Protocol = (USHORT)((pSrcBufVA[12] << 8) + pSrcBufVA[13]);
			if (Protocol <= 1500) {
				// get Ethernet protocol field from LLC/SNAP
				if (Sniff2BytesFromNdisBuffer(pSkb, LENGTH_802_3 + 6, &Byte0,
						&Byte1) != NDIS_STATUS_SUCCESS)
					break;

				Protocol = (USHORT)((Byte0 << 8) + Byte1);
				LlcSnapLen = 8;
			}

			// always AC_BE for non-IP packet
			if (Protocol != 0x0800)
				break;

			// get IP header
			if (Sniff2BytesFromNdisBuffer(pSkb, LENGTH_802_3 + LlcSnapLen,
					&Byte0, &Byte1) != NDIS_STATUS_SUCCESS)
				break;

			// return AC_BE if packet is not IPv4
			if ((Byte0 & 0xf0) != 0x40)
				break;

			UserPriority = (Byte1 & 0xe0) >> 5;
			QueIdx = MapUserPriorityToAccessCategory[UserPriority];

			// TODO: have to check ACM bit. apply TSPEC if ACM is ON
			// TODO: downgrade UP & QueIdx before passing ACM
			if (pAd->PortCfg.APEdcaParm.bACM[QueIdx]) {
				UserPriority = 0;
				QueIdx = QID_AC_BE;
			}
		} while (FALSE);
	}

	RTMP_SET_PACKET_UP(pSkb, UserPriority);

	// Make sure SendTxWait queue resource won't be used by other threads
	NdisAcquireSpinLock(&pAd->SendTxWaitQueueLock[QueIdx], IrqFlags);

	pTxQueue = &pAd->SendTxWaitQueue[QueIdx];
	if (pTxQueue->Number > pAd->MaxTxQueueSize) {
#ifdef BLOCK_NET_IF
		StopNetIfQueue(pAd, QueIdx, pSkb);
#endif // BLOCK_NET_IF //
		NdisReleaseSpinLock(&pAd->SendTxWaitQueueLock[QueIdx], IrqFlags);
		return NDIS_STATUS_FAILURE;
	}

	//
	// For infrastructure mode, enqueue this frame immediately to sendwaitqueue
	// For Ad-hoc mode, check the DA power state, then decide which queue to enqueue
	//
	if (INFRA_ON(pAd)) {
		// In infrastructure mode, simply enqueue the packet into Tx waiting queue.
		DBGPRINT(RT_DEBUG_INFO, "Infrastructure -> Enqueue one frame\n");

		// Enqueue Ndis packet to end of Tx wait queue
		InsertTailQueue(pTxQueue, pSkb);
		Status = NDIS_STATUS_SUCCESS;
#ifdef DBG
		pAd->RalinkCounters.OneSecOsTxCount[QueIdx]++; // TODO: for debug only. to be removed
#endif
	} else {
		// In IBSS mode, power state of destination should be considered.
		PsMode = PWR_ACTIVE; // Faked
		if (PsMode == PWR_ACTIVE) {
			DBGPRINT(RT_DEBUG_INFO,"Ad-Hoc -> Enqueue one frame\n");

			// Enqueue Ndis packet to end of Tx wait queue
			InsertTailQueue(pTxQueue, pSkb);
			Status = NDIS_STATUS_SUCCESS;
#ifdef DBG
			pAd->RalinkCounters.OneSecOsTxCount[QueIdx]++; // TODO: for debug only. to be removed
#endif
		}
	}

	NdisReleaseSpinLock(&pAd->SendTxWaitQueueLock[QueIdx], IrqFlags); DBGPRINT(RT_DEBUG_INFO, "<==== RTMPSendPacket\n");
	return (Status);
}

/*
 ========================================================================

 Routine Description:
 SendPackets handler

 Arguments:
 skb 			point to sk_buf which upper layer transmit
 net_dev 		point to net_dev
 Return Value:
 None

 Note:

 ========================================================================
 */INT RTMPSendPackets( IN struct sk_buff *pSkb, IN struct net_device *net_dev) {
	PRTMP_ADAPTER pAd = RTMP_OS_NETDEV_GET_PRIV(net_dev);
	NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
	INT Index;

	DBGPRINT(RT_DEBUG_INFO, "===> RTMPSendPackets\n");

#ifdef RALINK_ATE
	if (pAd->ate.Mode != ATE_STASTART)
	{
		RTUSBFreeSkbBuffer(pSkb);
		return 0;
	}
#endif

	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS)
			|| RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS)) {
		// Drop send request since hardware is in reset state
		RTUSBFreeSkbBuffer(pSkb);
		return 0;
	}
	// Drop packets if no associations
	else if (!INFRA_ON(pAd) && !ADHOC_ON(pAd)) {
		RTUSBFreeSkbBuffer(pSkb);
		return 0;
	} else {
		// Record that orignal packet source is from protocol layer,so that
		// later on driver knows how to release this skb buffer
		RTMP_SET_PACKET_SOURCE(pSkb, PKTSRC_NDIS);
		pAd->RalinkCounters.PendingNdisPacketCount++;

		Status = RTMPSendPacket(pAd, pSkb);
		if (Status != NDIS_STATUS_SUCCESS) {
			// Errors before enqueue stage
			RELEASE_NDIS_PACKET(pAd, pSkb); DBGPRINT(RT_DEBUG_TRACE,"<---RTUSBSendPackets not dequeue\n");
			return 0;
		}
	}

	// Dequeue one frame from SendTxWait queue and process it
	// There are two place calling dequeue for TX ring.
	// 1. Here, right after queueing the frame.
	// 2. At the end of TxRingTxDone service routine.
	if ((!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS))
			&& (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF))
			&& (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS))
			&& (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS))) {
		for (Index = 0; Index < 4; Index++) {
			if (pAd->SendTxWaitQueue[Index].Number > 0) {
				RTMPDeQueuePacket(pAd, Index);
			}
		}
	}

	// Kick bulk out
	RTUSBKickBulkOut(pAd);

	DBGPRINT(RT_DEBUG_INFO, "<=== RTMPSendPackets\n");

	return 0;
}

/*
 ========================================================================

 Routine	Description:
 Copy frame from waiting queue into relative ring buffer and set
 appropriate ASIC register to kick hardware encryption before really
 sent out to air.

 Arguments:
 pAd				Pointer	to our adapter
 PNDIS_PACKET	Pointer to outgoing Ndis frame
 NumberOfFrag	Number of fragment required

 Return Value:
 None

 Note:

 ========================================================================
 */
#ifdef BIG_ENDIAN
static inline
#endif
NDIS_STATUS RTUSBHardTransmit( IN PRTMP_ADAPTER pAd, IN struct sk_buff *pSkb,
		IN UCHAR NumberRequired, IN UCHAR QueIdx) {
	UINT LengthQosPAD = 0;
	UINT BytesCopied;
	UINT TxSize;
	UINT FreeMpduSize;
	UINT SrcRemainingBytes;
	USHORT Protocol;
	UCHAR FrameGap;
	HEADER_802_11 Header_802_11;
	PHEADER_802_11 pHeader80211;
	PUCHAR pDest;
	//	PUCHAR			pSrc;
	PTX_CONTEXT pTxContext;
	PTXD_STRUC pTxD;
#ifdef BIG_ENDIAN
	PTXD_STRUC pDestTxD;
	TXD_STRUC TxD;
#endif
	//	PURB			pUrb;
	BOOLEAN StartOfFrame;
	BOOLEAN bEAPOLFrame;
	ULONG Iv16;
	ULONG Iv32;
	BOOLEAN MICFrag;
	//	PCIPHER_KEY		pWpaKey = NULL;
	NDIS_802_11_WEP_STATUS EncryptType = Ndis802_11EncryptionDisabled;
	ULONG TransferBufferLength;
	USHORT AckDuration = 0;
	USHORT EncryptionOverhead = 0;
	UCHAR CipherAlg;
	BOOLEAN bAckRequired;
	UCHAR RetryMode = SHORT_RETRY;
	UCHAR UserPriority;
	UCHAR MpduRequired, RtsRequired;
	UCHAR TxRate;
	PCIPHER_KEY pKey = NULL;
	PUCHAR pSrcBufVA = NULL;
	ULONG SrcBufLen;
	PUCHAR pExtraLlcSnapEncap = NULL; // NULL: no extra LLC/SNAP is required
	UCHAR KeyIdx, KeyLength = 0;
	UCHAR KeyTable = SHARED_KEY_TABLE;
	PUCHAR pWirelessPacket;
	ULONG NextMpduSize;
	BOOLEAN bRTS_CTSFrame = FALSE;
	unsigned long IrqFlags; //BensonLiu modify

	DBGPRINT(RT_DEBUG_INFO, "====> RTUSBHardTransmit\n");

	if ((pAd->PortCfg.bIEEE80211H == 1) && (pAd->PortCfg.RadarDetect.RDMode
			!= RD_NORMAL_MODE)) {
		DBGPRINT(RT_DEBUG_INFO,"RTUSBHardTransmit --> radar detect not in normal mode !!!\n");
		return (NDIS_STATUS_FAILURE);
	}

	TxRate = RTMP_GET_PACKET_TXRATE(pSkb);
	MpduRequired = RTMP_GET_PACKET_FRAGMENTS(pSkb);
	RtsRequired = RTMP_GET_PACKET_RTS(pSkb);
	UserPriority = RTMP_GET_PACKET_UP(pSkb);

	//
	// Prepare packet information structure which will be query for buffer descriptor
	//
	pSrcBufVA = (PVOID) pSkb->data;
	SrcBufLen = pSkb->len;

	// Check for virtual address allocation, it might fail !!!
	if (pSrcBufVA == NULL) {
		DBGPRINT_RAW(RT_DEBUG_TRACE, "pSrcBufVA == NULL\n");
		return (NDIS_STATUS_RESOURCES);
	}
	if (SrcBufLen < 14) {
		DBGPRINT_RAW(RT_DEBUG_ERROR, "RTUSBHardTransmit --> Skb buffer error !!!\n");
		return (NDIS_STATUS_FAILURE);
	}

	//
	// If DHCP datagram or ARP datagram , we need to send it as Low rates.
	//
	if (pAd->PortCfg.Channel <= 14) {
		//
		// Case 802.11 b/g
		// basic channel means that we can use CCKM's low rate as RATE_1.
		//
		if ((TxRate != RATE_1) && RTMPCheckDHCPFrame(pAd, pSkb))
			TxRate = RATE_1;
	} else {
		//
		// Case 802.11a
		// We must used OFDM's low rate as RATE_6, note RATE_1 is not allow
		// Only OFDM support on Channel > 14
		//
		if ((TxRate != RATE_6) && RTMPCheckDHCPFrame(pAd, pSkb))
			TxRate = RATE_6;
	}

	// ------------------------------------------
	// STEP 0.1 Add 802.1x protocol check.
	// ------------------------------------------
	// For non-WPA network, 802.1x message should not encrypt even privacy is on.
	if (NdisEqualMemory(EAPOL, pSrcBufVA + 12, 2)) {
		bEAPOLFrame = TRUE;
		if (pAd->PortCfg.MicErrCnt >= 2)
			pAd->PortCfg.MicErrCnt++;
	} else
		bEAPOLFrame = FALSE;

	//
	// WPA 802.1x secured port control - drop all non-802.1x frame before port secured
	//
	{
		if (((pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA)
				|| (pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPAPSK)
				|| (pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA2)
				|| (pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA2PSK)
#if defined(RALINK_WPA_SUPPLICANT_SUPPORT) || defined(NATIVE_WPA_SUPPLICANT_SUPPORT)
	 || (pAd->PortCfg.IEEE8021X == TRUE)
#endif
	 )
				&& ((pAd->PortCfg.PortSecured == WPA_802_1X_PORT_NOT_SECURED)
						|| (pAd->PortCfg.MicErrCnt >= 2)) && (bEAPOLFrame
				== FALSE)) {
			DBGPRINT_RAW(RT_DEBUG_INFO, "RTUSBHardTransmit --> Drop packet before port secured !!!\n");
			return (NDIS_STATUS_FAILURE);
		}
	}

	if (*pSrcBufVA & 0x01) // Multicast or Broadcast
	{
		bAckRequired = FALSE;
		INC_COUNTER64(pAd->WlanCounters.MulticastTransmittedFrameCount);
		EncryptType = pAd->PortCfg.GroupCipher; // Cipher for Multicast or Broadcast
	} else {
		bAckRequired = TRUE;
		EncryptType = pAd->PortCfg.PairCipher; // Cipher for Unicast
	}

	// 1. traditional TX burst
	if (pAd->PortCfg.bEnableTxBurst && (pAd->Sequence & 0x7))
		FrameGap = IFS_SIFS;
	// 2. frame belonging to AC that has non-zero TXOP
	else if (pAd->PortCfg.APEdcaParm.bValid
			&& pAd->PortCfg.APEdcaParm.Txop[QueIdx])
		FrameGap = IFS_SIFS;
	// 3. otherwise, always BACKOFF before transmission
	else
		FrameGap = IFS_BACKOFF; // Default frame gap mode

	Protocol = *(pSrcBufVA + 12) * 256 + *(pSrcBufVA + 13);
	// if orginal Ethernet frame contains no LLC/SNAP, then an extra LLC/SNAP encap is required

	if (Protocol > 1500) {
		pExtraLlcSnapEncap = SNAP_802_1H;
		if (NdisEqualMemory(IPX, pSrcBufVA + 12, 2)
				|| NdisEqualMemory(APPLE_TALK, pSrcBufVA + 12, 2)) {
			pExtraLlcSnapEncap = SNAP_BRIDGE_TUNNEL;
		}
	} else
		pExtraLlcSnapEncap = NULL;

	// Update software power save state
	OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_DOZE);
	pAd->PortCfg.Psm = PWR_ACTIVE;

	// -----------------------------------------------------------------
	// STEP 2. MAKE A COMMON 802.11 HEADER SHARED BY ENTIRE FRAGMENT BURST.
	// -----------------------------------------------------------------

	pAd->Sequence = ((pAd->Sequence) + 1) & (MAX_SEQ_NUMBER);
	MAKE_802_11_HEADER(pAd, Header_802_11, pSrcBufVA, pAd->Sequence);

	// --------------------------------------------------------
	// STEP 3. FIND ENCRYPT KEY AND DECIDE CIPHER ALGORITHM
	//		Find the WPA key, either Group or Pairwise Key
	//		LEAP + TKIP also use WPA key.
	// --------------------------------------------------------
	// Decide WEP bit and cipher suite to be used. Same cipher suite should be used for whole fragment burst
	// In Cisco CCX 2.0 Leap Authentication
	//		   WepStatus is Ndis802_11Encryption1Enabled but the key will use PairwiseKey
	//		   Instead of the SharedKey, SharedKey Length may be Zero.
	KeyIdx = 0xff;
	KeyTable = SHARED_KEY_TABLE;

	if (bEAPOLFrame) {
		ASSERT(pAd->SharedKey[0].CipherAlg <= CIPHER_CKIP128);
		if ((pAd->SharedKey[0].CipherAlg) && (pAd->SharedKey[0].KeyLen)
				&& ((pAd->PortCfg.PortSecured == WPA_802_1X_PORT_PASS_4_WAY)
						|| (pAd->PortCfg.PortSecured == WPA_802_1X_PORT_SECURED))) {
			CipherAlg = pAd->SharedKey[0].CipherAlg;
			KeyIdx = 0;
			KeyLength = pAd->SharedKey[KeyIdx].KeyLen;
		}
	} else if (EncryptType == Ndis802_11Encryption1Enabled) {
		// standard WEP64 or WEP128
		KeyIdx = pAd->PortCfg.DefaultKeyId;
		KeyLength = pAd->SharedKey[KeyIdx].KeyLen;
	} else if ((EncryptType == Ndis802_11Encryption2Enabled) || (EncryptType
			== Ndis802_11Encryption3Enabled)) {
		if (Header_802_11.Addr1[0] & 0x01) // multicast
			KeyIdx = pAd->PortCfg.DefaultKeyId;
		else if (pAd->SharedKey[0].KeyLen)
			KeyIdx = 0;
		else
			KeyIdx = pAd->PortCfg.DefaultKeyId;

		KeyLength = pAd->SharedKey[KeyIdx].KeyLen;
	}

	if (KeyIdx == 0xff)
		CipherAlg = CIPHER_NONE;
	else if ((EncryptType == Ndis802_11EncryptionDisabled) || (KeyLength == 0))
		CipherAlg = CIPHER_NONE;
	else {
		Header_802_11.FC.Wep = 1;

		CipherAlg = pAd->SharedKey[KeyIdx].CipherAlg;
		pKey = &pAd->SharedKey[KeyIdx];

	}

	DBGPRINT(RT_DEBUG_INFO,"RTUSBHardTransmit(bEAP=%d) - %s key#%d, KeyLen=%d\n",
			bEAPOLFrame, CipherName[CipherAlg], KeyIdx, pAd->SharedKey[KeyIdx].KeyLen);

	// STEP 3.1 if TKIP is used and fragmentation is required. Driver has to
	//			append TKIP MIC at tail of the scatter buffer (This must be the
	//			ONLY scatter buffer in the skb buffer).
	//			MAC ASIC will only perform IV/EIV/ICV insertion but no TKIP MIC
	if ((MpduRequired > 1) && (CipherAlg == CIPHER_TKIP)) {
		pSkb->len += 8;
		CipherAlg = CIPHER_TKIP_NO_MIC;
	}

	// ----------------------------------------------------------------
	// STEP 4. Make RTS frame or CTS-to-self frame if required
	// ----------------------------------------------------------------

	//
	// calcuate the overhead bytes that encryption algorithm may add. This
	// affects the calculate of "duration" field
	//
	if ((CipherAlg == CIPHER_WEP64) || (CipherAlg == CIPHER_WEP128))
		EncryptionOverhead = 8; //WEP: IV[4] + ICV[4];
	else if (CipherAlg == CIPHER_TKIP_NO_MIC)
		EncryptionOverhead = 12;//TKIP: IV[4] + EIV[4] + ICV[4], MIC will be added to TotalPacketLength
	else if (CipherAlg == CIPHER_TKIP)
		EncryptionOverhead = 20;//TKIP: IV[4] + EIV[4] + ICV[4] + MIC[8]
	else if (CipherAlg == CIPHER_AES)
		EncryptionOverhead = 16; // AES: IV[4] + EIV[4] + MIC[8]
	else
		EncryptionOverhead = 0;

	// decide how much time an ACK/CTS frame will consume in the air
	AckDuration = RTMPCalcDuration(pAd, pAd->PortCfg.ExpectedACKRate[TxRate],
			14);

	// If fragment required, MPDU size is maximum fragment size
	// Else, MPDU size should be frame with 802.11 header & CRC
	if (MpduRequired > 1)
		NextMpduSize = pAd->PortCfg.FragmentThreshold;
	else {
		NextMpduSize = pSkb->len + LENGTH_802_11 + LENGTH_CRC - LENGTH_802_3;
		if (pExtraLlcSnapEncap)
			NextMpduSize += LENGTH_802_1_H;
	}

	if (RtsRequired
			|| OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_RTS_PROTECTION_ENABLE)) {
		RTMPSendRTSCTSFrame(pAd, Header_802_11.Addr1,
				NextMpduSize + EncryptionOverhead, TxRate,
				pAd->PortCfg.RtsRate, AckDuration, QueIdx, FrameGap,
				SUBTYPE_RTS);

		// RTS/CTS-protected frame should use LONG_RETRY (=4) and SIFS
		RetryMode = LONG_RETRY;
		FrameGap = IFS_SIFS;
		bRTS_CTSFrame = TRUE;

		if (RtsRequired)
			NumberRequired--;
	} else if ((pAd->PortCfg.TxRate >= RATE_FIRST_OFDM_RATE)
			&& OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_BG_PROTECTION_INUSED)) {
		RTMPSendRTSCTSFrame(pAd, Header_802_11.Addr1,
				NextMpduSize + EncryptionOverhead, TxRate,
				pAd->PortCfg.RtsRate, AckDuration, QueIdx, FrameGap,
				SUBTYPE_CTS);

		// RTS/CTS-protected frame should use LONG_RETRY (=4) and SIFS
		RetryMode = LONG_RETRY;
		FrameGap = IFS_SIFS;
		bRTS_CTSFrame = TRUE;
	}

	// --------------------------------------------------------
	// STEP 5. START MAKING MPDU(s)
	//		Start Copy Ndis Packet into Ring buffer.
	//		For frame required more than one ring buffer (fragment), all ring buffers
	//		have to be filled before kicking start tx bit.
	//		Make sure TX ring resource won't be used by other threads
	// --------------------------------------------------------
	SrcRemainingBytes = pSkb->len - LENGTH_802_3;
	SrcBufLen -= LENGTH_802_3; // skip 802.3 header

	StartOfFrame = TRUE;
	MICFrag = FALSE; // Flag to indicate MIC shall spread into two MPDUs

	NdisAcquireSpinLock(&pAd->TxRingLock, IrqFlags);//BensonLiu modify

	// Start Copy Ndis Packet into Ring buffer.
	// For frame required more than one ring buffer (fragment), all ring buffers
	// have to be filled before kicking start tx bit.
	do {
		//
		// STEP 5.1 Get the Tx Ring descriptor & Dma Buffer address
		//
		pTxContext = &pAd->TxContext[QueIdx][pAd->NextTxIndex[QueIdx]];

		if ((pTxContext->bWaitingBulkOut == TRUE)
				|| (pTxContext->InUse == TRUE)
				|| (pAd->TxRingTotalNumber[QueIdx] >= TX_RING_SIZE)) {
			DBGPRINT_ERR("RTUSBHardTransmit: TX RING full\n");
			pAd->RalinkCounters.TxRingErrCount++;
			NdisReleaseSpinLock(&pAd->TxRingLock, IrqFlags);//BensonLiu modify
			return (NDIS_STATUS_RESOURCES);
		}
		pTxContext->InUse = TRUE;

		// Increase & maintain Tx Ring Index
		pAd->NextTxIndex[QueIdx]++;
		if (pAd->NextTxIndex[QueIdx] >= TX_RING_SIZE) {
			pAd->NextTxIndex[QueIdx] = 0;
		}

#ifndef BIG_ENDIAN
		pTxD = (PTXD_STRUC) &pTxContext->TransferBuffer->TxDesc;
#else
		pDestTxD = (PTXD_STRUC) &pTxContext->TransferBuffer->TxDesc;
		TxD = *pDestTxD;
		pTxD = &TxD;
		RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
#endif
		NdisZeroMemory(pTxD, sizeof(TXD_STRUC));
		pWirelessPacket = pTxContext->TransferBuffer->u.WirelessPacket;

		//
		// STEP 5.2 PUT IVOFFSET, IV, EIV INTO TXD
		//

		pTxD->IvOffset = LENGTH_802_11;

#if 0
		if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED))
		pTxD->IvOffset += 2; // add QOS CONTROL bytes
#endif

		if ((CipherAlg == CIPHER_WEP64) || (CipherAlg == CIPHER_WEP128)) {
			PUCHAR pTmp;
			pTmp = (PUCHAR) &pTxD->Iv;
			*pTmp = RandomByte(pAd);
			*(pTmp + 1) = RandomByte(pAd);
			*(pTmp + 2) = RandomByte(pAd);
			*(pTmp + 3) = (KeyIdx << 6);
		} else if ((CipherAlg == CIPHER_TKIP) || (CipherAlg
				== CIPHER_TKIP_NO_MIC)) {
			UCHAR kidx;

			kidx = KeyIdx;

			RTMPInitTkipEngine(pAd, pKey->Key, kidx, //KeyIdx,		// This might cause problem when using peer key
					Header_802_11.Addr2, pKey->TxMic, pKey->TxTsc, &Iv16, &Iv32);

			NdisMoveMemory(&pTxD->Iv, &Iv16, 4); // Copy IV
			NdisMoveMemory(&pTxD->Eiv, &Iv32, 4); // Copy EIV
			INC_TX_TSC(pKey->TxTsc); // Increase TxTsc for next transmission
		} else if (CipherAlg == CIPHER_AES) {
			PUCHAR pTmp;
			UCHAR kidx;

			kidx = KeyIdx;

			pTmp = (PUCHAR) &Iv16;
			*pTmp = pKey->TxTsc[0];
			*(pTmp + 1) = pKey->TxTsc[1];
			*(pTmp + 2) = 0;
			*(pTmp + 3) = (/*pAd->PortCfg.DefaultKeyId*/kidx << 6) | 0x20;
			Iv32 = *(PULONG) (&pKey->TxTsc[2]);

			NdisMoveMemory(&pTxD->Iv, &Iv16, 4); // Copy IV
			NdisMoveMemory(&pTxD->Eiv, &Iv32, 4); // Copy EIV
			INC_TX_TSC(pKey->TxTsc); // Increase TxTsc for next transmission
		}

		//
		// STEP 5.3 COPY 802.11 HEADER INTO 1ST DMA BUFFER
		//
		pDest = pWirelessPacket;
		NdisMoveMemory(pDest, &Header_802_11, sizeof(Header_802_11));
		pDest += sizeof(Header_802_11);

		//
		// Fragmentation is not allowed on multicast & broadcast
		// So, we need to used the MAX_FRAG_THRESHOLD instead of pAd->PortCfg.FragmentThreshold
		// otherwise if pSkb->len > pAd->PortCfg.FragmentThreshold then
		// packet will be fragment on multicast & broadcast.
		//
		// MpduRequired equals to 1 means this could be Aggretaion case.
		//
		if ((Header_802_11.Addr1[0] & 0x01) || MpduRequired == 1) {
			FreeMpduSize = MAX_FRAG_THRESHOLD - sizeof(Header_802_11)
					- LENGTH_CRC;
		} else {
			FreeMpduSize = pAd->PortCfg.FragmentThreshold
					- sizeof(Header_802_11) - LENGTH_CRC;
		}

		//
		// STEP 5.4 COPY LLC/SNAP, CKIP MIC INTO 1ST DMA BUFFER ONLY WHEN THIS
		//			MPDU IS THE 1ST OR ONLY FRAGMENT
		//
		if (Header_802_11.Frag == 0) {
			if (pExtraLlcSnapEncap) {
				if ((CipherAlg == CIPHER_TKIP_NO_MIC) && (pKey != NULL)) {
					// Calculate MSDU MIC Value
					RTMPCalculateMICValue(pAd, pSkb, pExtraLlcSnapEncap, pKey);
				}

				// Insert LLC-SNAP encapsulation
				NdisMoveMemory(pDest, pExtraLlcSnapEncap, 6);
				pDest += 6;
				NdisMoveMemory(pDest, pSrcBufVA + 12, 2);
				pDest += 2;
				pSrcBufVA += LENGTH_802_3;
				FreeMpduSize -= LENGTH_802_1_H;

			} else {
				if ((CipherAlg == CIPHER_TKIP_NO_MIC) && (pKey != NULL)) {
					// Calculate MSDU MIC Value
					RTMPCalculateMICValue(pAd, pSkb, pExtraLlcSnapEncap, pKey);
				}
				pSrcBufVA += LENGTH_802_3;
			}
		}

		// Start copying payload
		BytesCopied = 0;
		do {
			if (SrcBufLen >= FreeMpduSize) {
				// Copy only the free fragment size, and save the pointer
				// of current buffer descriptor for next fragment buffer.
				NdisMoveMemory(pDest, pSrcBufVA, FreeMpduSize);
				BytesCopied += FreeMpduSize;
				pSrcBufVA += FreeMpduSize;
				pDest += FreeMpduSize;
				SrcBufLen -= FreeMpduSize;
				break;
			} else {
				// Copy the rest of this buffer descriptor pointed data
				// into ring buffer.
				NdisMoveMemory(pDest, pSrcBufVA, SrcBufLen);
				BytesCopied += SrcBufLen;
				pDest += SrcBufLen;
				FreeMpduSize -= SrcBufLen;
			}

			// No more buffer descriptor
			// Add MIC value if needed

			//if ((pAd->PortCfg.WepStatus == Ndis802_11Encryption2Enabled) &&
			//	(MICFrag == FALSE) &&
			//	(pKey != NULL))

			if ((CipherAlg == CIPHER_TKIP_NO_MIC) && (MICFrag == FALSE)
					&& (pKey != NULL)) {
				// Fregment and TKIP//
				INT i;

				SrcBufLen = 8; // Set length to MIC length
				DBGPRINT_RAW(RT_DEBUG_INFO, "Calculated TX MIC value =");
				for (i = 0; i < 8; i++) {
					DBGPRINT_RAW(RT_DEBUG_INFO, "%02x:", pAd->PrivateInfo.Tx.MIC[i]);
				} DBGPRINT_RAW(RT_DEBUG_INFO, "\n");

				if (FreeMpduSize >= SrcBufLen) {
					NdisMoveMemory(pDest, pAd->PrivateInfo.Tx.MIC, SrcBufLen);
					BytesCopied += SrcBufLen;
					pDest += SrcBufLen;
					FreeMpduSize -= SrcBufLen;
					SrcBufLen = 0;
				} else {
					NdisMoveMemory(pDest, pAd->PrivateInfo.Tx.MIC, FreeMpduSize);
					BytesCopied += FreeMpduSize;
					pSrcBufVA = pAd->PrivateInfo.Tx.MIC + FreeMpduSize;
					pDest += FreeMpduSize;
					SrcBufLen -= FreeMpduSize;
					MICFrag = TRUE;
				}
			}
		} while (FALSE); // End of copying payload

		// Real packet size, No 802.1H header for fragments except the first one.
		if ((StartOfFrame == TRUE) && (pExtraLlcSnapEncap != NULL)) {
			TxSize = BytesCopied + LENGTH_802_11 + LENGTH_802_1_H
					+ LengthQosPAD;
		} else {
			TxSize = BytesCopied + LENGTH_802_11 + LengthQosPAD;
		}

		SrcRemainingBytes -= BytesCopied;

		//
		// STEP 5.6 MODIFY MORE_FRAGMENT BIT & DURATION FIELD. WRITE TXD
		//
		pHeader80211 = (PHEADER_802_11) pWirelessPacket;
		if (SrcRemainingBytes > 0) // more fragment is required
		{
			ULONG NextMpduSize;

			pHeader80211->FC.MoreFrag = 1;
			NextMpduSize
					= min((ULONG)SrcRemainingBytes, (ULONG)pAd->PortCfg.FragmentThreshold);

			if (NextMpduSize < pAd->PortCfg.FragmentThreshold) {
				// In this case, we need to include LENGTH_802_11 and LENGTH_CRC for calculating Duration.
				pHeader80211->Duration = (3 * pAd->PortCfg.Dsifs) + (2
						* AckDuration) + RTMPCalcDuration(
						pAd,
						TxRate,
						NextMpduSize + EncryptionOverhead + LENGTH_802_11
								+ LENGTH_CRC);
			} else {
				pHeader80211->Duration = (3 * pAd->PortCfg.Dsifs) + (2
						* AckDuration) + RTMPCalcDuration(pAd, TxRate,
						NextMpduSize + EncryptionOverhead);
			}

#ifdef BIG_ENDIAN
			RTMPFrameEndianChange(pAd, (PUCHAR)pHeader80211, DIR_WRITE, FALSE);
			RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
			*pDestTxD = TxD;
			pTxD = pDestTxD;
#endif
			RTUSBWriteTxDescriptor(pAd, pTxD, CipherAlg, KeyTable/*0*/, KeyIdx,
					bAckRequired, TRUE, FALSE, RetryMode, FrameGap, TxRate,
					TxSize, QueIdx, 0, bRTS_CTSFrame);

			FrameGap = IFS_SIFS; // use SIFS for all subsequent fragments
			Header_802_11.Frag++; // increase Frag #
		} else {
			pHeader80211->FC.MoreFrag = 0;
			if (pHeader80211->Addr1[0] & 0x01) // multicast/broadcast
				pHeader80211->Duration = 0;
			else
				pHeader80211->Duration = pAd->PortCfg.Dsifs + AckDuration;

			if ((bEAPOLFrame) && (TxRate > RATE_6))
				TxRate = RATE_6;

#ifdef BIG_ENDIAN
			RTMPFrameEndianChange(pAd, (PUCHAR)pHeader80211, DIR_WRITE, FALSE);
			RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
			*pDestTxD = TxD;
			pTxD = pDestTxD;
#endif
			RTUSBWriteTxDescriptor(pAd, pTxD, CipherAlg, KeyTable/*0*/, KeyIdx,
					bAckRequired, FALSE, FALSE, RetryMode, FrameGap, TxRate,
					TxSize, QueIdx, 0, bRTS_CTSFrame);

			if (pAd->SendTxWaitQueue[QueIdx].Number > 1)
				pTxD->Burst = 1;

		}

		TransferBufferLength = TxSize + sizeof(TXD_STRUC);

		if ((TransferBufferLength % 4) == 1)
			TransferBufferLength += 3;
		else if ((TransferBufferLength % 4) == 2)
			TransferBufferLength += 2;
		else if ((TransferBufferLength % 4) == 3)
			TransferBufferLength += 1;

		if ((TransferBufferLength % pAd->BulkOutMaxPacketSize) == 0)
			TransferBufferLength += 4;

		pTxContext->BulkOutSize = TransferBufferLength;
		pTxContext->bWaitingBulkOut = TRUE;
		RTUSB_SET_BULK_FLAG(pAd, (fRTUSB_BULK_OUT_DATA_NORMAL << QueIdx));

		// Set frame gap for the rest of fragment burst.
		// It won't matter if there is only one fragment (single fragment frame).
		StartOfFrame = FALSE;
		NumberRequired--;
		if (NumberRequired == 0) {
			pTxContext->LastOne = TRUE;
		} else {
			pTxContext->LastOne = FALSE;
		}

		pAd->TxRingTotalNumber[QueIdx]++; // sync. to TxCount
		atomic_inc(&pAd->TxCount);

	} while (NumberRequired > 0);

	NdisReleaseSpinLock(&pAd->TxRingLock, IrqFlags);//BensonLiu modify

	//
	// Check if MIC error twice within 60 seconds and send EAPOL MIC error to TX queue
	// then we enqueue a message for disasociating with the current AP
	//

	// Check for EAPOL frame sent after MIC countermeasures
	if (pAd->PortCfg.MicErrCnt >= 3) {
		MLME_DISASSOC_REQ_STRUCT DisassocReq;

		// disassoc from current AP first
		printk(
				"<0>MLME - disassociate with current AP after sending second continuous EAPOL frame\n");
		DBGPRINT(RT_DEBUG_TRACE, "MLME - disassociate with current AP after sending second continuous EAPOL frame\n");
		DisassocParmFill(pAd, &DisassocReq, pAd->PortCfg.Bssid,
				REASON_MIC_FAILURE);
		MlmeEnqueue(pAd, ASSOC_STATE_MACHINE, MT2_MLME_DISASSOC_REQ,
				sizeof(MLME_DISASSOC_REQ_STRUCT), &DisassocReq);

		pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_DISASSOC;
		pAd->PortCfg.bBlockAssoc = TRUE;
		printk("<0>bBlockAssoc = %d\n", pAd->PortCfg.bBlockAssoc);
	}

	// release the skb buffer
	RELEASE_NDIS_PACKET(pAd, pSkb); DBGPRINT(RT_DEBUG_INFO, "<==== RTUSBHardTransmit\n");
	return (NDIS_STATUS_SUCCESS);
}

/*
 ========================================================================

 Routine	Description:
 Copy frame from waiting queue into relative ring buffer and set
 appropriate ASIC register to kick hardware transmit function

 Arguments:
 pAd			Pointer	to our adapter
 pBuffer		Pointer to	memory of outgoing frame
 Length		Size of outgoing management frame

 Return Value:
 NDIS_STATUS_FAILURE
 NDIS_STATUS_PENDING
 NDIS_STATUS_SUCCESS

 Note:

 ========================================================================
 */VOID RTUSBMlmeHardTransmit( IN PRTMP_ADAPTER pAd, IN PMGMT_STRUC pMgmt) {
	PTX_CONTEXT pMLMEContext;
	PTXD_STRUC pTxD;
#ifdef BIG_ENDIAN
	PTXD_STRUC pDestTxD;
	TXD_STRUC TxD;
#endif
	PUCHAR pDest;
	PHEADER_802_11 pHeader_802_11;
	BOOLEAN AckRequired, InsertTimestamp;
	ULONG TransferBufferLength;
	PVOID pBuffer = pMgmt->pBuffer;
	ULONG Length = pMgmt->Length;
	UCHAR QueIdx;
	UCHAR MlmeRate;

	DBGPRINT_RAW(RT_DEBUG_INFO, "--->MlmeHardTransmit\n");

	//pAd->PrioRingTxCnt++;

	pMLMEContext = &pAd->MLMEContext[pAd->NextMLMEIndex];
	pMLMEContext->InUse = TRUE;

	// Increase & maintain Tx Ring Index
	pAd->NextMLMEIndex++;
	if (pAd->NextMLMEIndex >= PRIO_RING_SIZE) {
		pAd->NextMLMEIndex = 0;
	}

	pDest = pMLMEContext->TransferBuffer->u.WirelessPacket;

#ifndef BIG_ENDIAN
	pTxD = (PTXD_STRUC) (pMLMEContext->TransferBuffer);
#else
	pDestTxD = (PTXD_STRUC)(pMLMEContext->TransferBuffer);
	TxD = *pDestTxD;
	pTxD = &TxD;
	RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
#endif
	NdisZeroMemory(pTxD, sizeof(TXD_STRUC));

	pHeader_802_11 = (PHEADER_802_11) pBuffer;

	// Verify Mlme rate for a / g bands.
	if (pHeader_802_11->Addr1[0] & 0x01) {
		MlmeRate = pAd->PortCfg.BasicMlmeRate;
	} else {
		MlmeRate = pAd->PortCfg.MlmeRate;
	}

	if ((pAd->LatchRfRegs.Channel > 14) && (MlmeRate < RATE_6)) // 11A band
		MlmeRate = RATE_6;

	DBGPRINT(RT_DEBUG_TRACE, "<---MlmeRate %d	Channel %d\n",MlmeRate, pAd->LatchRfRegs.Channel );

	// Before radar detection done, mgmt frame can not be sent but probe req
	// Because we need to use probe req to trigger driver to send probe req in passive scan
	if ((pHeader_802_11->FC.SubType != SUBTYPE_PROBE_REQ)
			&& (pAd->PortCfg.bIEEE80211H == 1)
			&& (pAd->PortCfg.RadarDetect.RDMode != RD_NORMAL_MODE)) {
		DBGPRINT(RT_DEBUG_ERROR, "RTUSBMlmeHardTransmit --> radar detect not in normal mode !!!\n");
		return;
	}

	if (pHeader_802_11->FC.PwrMgmt != PWR_SAVE) {
		pHeader_802_11->FC.PwrMgmt = (pAd->PortCfg.Psm == PWR_SAVE);
	}

	InsertTimestamp = FALSE;
	if (pHeader_802_11->FC.Type == BTYPE_CNTL) // must be PS-POLL
	{
		AckRequired = FALSE;
	} else // BTYPE_MGMT or BMGMT_DATA(must be NULL frame)
	{
		pAd->Sequence = ((pAd->Sequence) + 1) & (MAX_SEQ_NUMBER);
		pHeader_802_11->Sequence = pAd->Sequence;

		if (pHeader_802_11->Addr1[0] & 0x01) // MULTICAST, BROADCAST
		{
			INC_COUNTER64(pAd->WlanCounters.MulticastTransmittedFrameCount);
			AckRequired = FALSE;
			pHeader_802_11->Duration = 0;
		} else {
			AckRequired = TRUE;
			pHeader_802_11->Duration = RTMPCalcDuration(pAd, MlmeRate, 14);
			if (pHeader_802_11->FC.SubType == SUBTYPE_PROBE_RSP) {
				InsertTimestamp = TRUE;
			}
		}
	}

#ifdef BIG_ENDIAN
	RTMPFrameEndianChange(pAd, (PUCHAR)pBuffer, DIR_WRITE, FALSE);
#endif

	NdisMoveMemory(pDest, pBuffer, Length);

	// Initialize Priority Descriptor
	// For inter-frame gap, the number is for this frame and next frame
	// For MLME rate, we will fix as 2Mb to match other vendor's implement

	QueIdx = QID_AC_BE;

#ifdef BIG_ENDIAN
	RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
	*pDestTxD = TxD;
	pTxD = pDestTxD;
#endif
	RTUSBWriteTxDescriptor(pAd, pTxD, CIPHER_NONE, 0, 0, AckRequired, FALSE,
			FALSE, SHORT_RETRY, IFS_BACKOFF, MlmeRate, /*Length+4*/Length,
			QueIdx, PID_MGMT_FRAME, FALSE);

#ifdef DBG
	{
#if 0
		UINT i;
		PUCHAR ptr = (PUCHAR)pDestTxD;

		DBGPRINT(RT_DEBUG_TRACE, "pAd->NextMLMEIndex = %d *pDestTxD :\n", pAd->NextMLMEIndex);
		DBGPRINT(RT_DEBUG_TRACE, "ptr = %d\n",ptr);
		for (i=0;i<64; i+=16)
		{
			DBGPRINT_RAW(RT_DEBUG_TRACE,"%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x - %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
					*ptr,*(ptr+1),*(ptr+2),*(ptr+3),*(ptr+4),*(ptr+5),*(ptr+6),*(ptr+7),
					*(ptr+8),*(ptr+9),*(ptr+10),*(ptr+11),*(ptr+12),*(ptr+13),*(ptr+14),*(ptr+15));
			ptr += 16;

		}
#endif
	}
#endif

	// Build our URB for USBD
	TransferBufferLength = sizeof(TXD_STRUC) + Length;
	if ((TransferBufferLength % 2) == 1)
		TransferBufferLength++;

	if ((TransferBufferLength % pAd->BulkOutMaxPacketSize) == 0)
		TransferBufferLength += 2;

	pMLMEContext->BulkOutSize = TransferBufferLength;
	pMLMEContext->bWaitingBulkOut = TRUE;
	RTUSB_SET_BULK_FLAG(pAd, fRTUSB_BULK_OUT_MLME);
	pAd->PrioRingTxCnt++;//2007/12/24:WY fixed queeue full bug
	//DBGPRINT(RT_DEBUG_INFO, "<---MlmeHardTransmit\n");
}

/*
 ========================================================================

 Routine	Description:
 This subroutine will scan through releative ring descriptor to find
 out avaliable free ring descriptor and compare with request size.

 Arguments:
 pAd			Pointer	to our adapter
 RingType	Selected Ring

 Return Value:
 NDIS_STATUS_FAILURE		Not enough free descriptor
 NDIS_STATUS_SUCCESS		Enough free descriptor

 Note:

 ========================================================================
 */NDIS_STATUS RTUSBFreeDescriptorRequest( IN PRTMP_ADAPTER pAd,
		IN UCHAR RingType, IN UCHAR BulkOutPipeId, IN UCHAR NumberRequired) {
	UCHAR FreeNumber = 0;
	UINT Index;
	NDIS_STATUS Status = NDIS_STATUS_FAILURE;

	switch (RingType) {
	case TX_RING:
		Index = (pAd->NextTxIndex[BulkOutPipeId] + 1) % TX_RING_SIZE;
		do {
			PTX_CONTEXT pTxD = &pAd->TxContext[BulkOutPipeId][Index];

			// While Owner bit is NIC, obviously ASIC still need it.
			// If valid bit is TRUE, indicate that TxDone has not process yet
			// We should not use it until TxDone finish cleanup job
			if (pTxD->InUse == FALSE) {
				// This one is free
				FreeNumber++;
			} else {
				break;
			}
			Index = (Index + 1) % TX_RING_SIZE;
		} while (FreeNumber < NumberRequired); // Quit here ! Free number is enough !

		if (FreeNumber >= NumberRequired) {
			Status = NDIS_STATUS_SUCCESS;
		}

		break;

	case PRIO_RING:
		Index = pAd->NextMLMEIndex;
		do {
			PTX_CONTEXT pTxD = &pAd->MLMEContext[Index];

			// While Owner bit is NIC, obviously ASIC still need it.
			// If valid bit is TRUE, indicate that TxDone has not process yet
			// We should not use it until TxDone finish cleanup job
			if (pTxD->InUse == FALSE) {
				// This one is free
				FreeNumber++;
			} else {
				break;
			}

			Index = (Index + 1) % PRIO_RING_SIZE;
		} while (FreeNumber < NumberRequired); // Quit here ! Free number is enough !

		if (FreeNumber >= NumberRequired) {
			Status = NDIS_STATUS_SUCCESS;
		}
		break;

	default:
		DBGPRINT_RAW(RT_DEBUG_ERROR, "--->RTUSBFreeDescriptorRequest() -----!! \n");

		break;
	}

	return (Status);
}

/*
 ========================================================================

 Routine Description:

 Arguments:

 Return Value:

 Note:
 
 ========================================================================
 */VOID RTUSBRejectPendingPackets( IN PRTMP_ADAPTER pAd) {
	UCHAR Index;
	PQUEUE_HEADER pQueue;
	struct sk_buff *skb;
	unsigned long IrqFlags;

	DBGPRINT_RAW(RT_DEBUG_TRACE, "--->RejectPendingPackets\n");

	for (Index = 0; Index < 4; Index++) {
		NdisAcquireSpinLock(&pAd->SendTxWaitQueueLock[Index], IrqFlags);
		while (pAd->SendTxWaitQueue[Index].Head != NULL) {
			pQueue = (PQUEUE_HEADER) &(pAd->SendTxWaitQueue[Index]);

			skb = (struct sk_buff *) RemoveHeadQueue(pQueue);
			RTUSBFreeSkbBuffer(skb);
		}
		NdisReleaseSpinLock(&pAd->SendTxWaitQueueLock[Index], IrqFlags);
	}

	DBGPRINT_RAW(RT_DEBUG_TRACE, "<---RejectPendingPackets\n");
}

/*
 ========================================================================

 Routine	Description:
 Calculates the duration which is required to transmit out frames
 with given size and specified rate.

 Arguments:
 pTxD		Pointer to transmit descriptor
 Ack			Setting for Ack requirement bit
 Fragment	Setting for Fragment bit
 RetryMode	Setting for retry mode
 Ifs			Setting for IFS gap
 Rate		Setting for transmit rate
 Service		Setting for service
 Length		Frame length
 TxPreamble	Short or Long preamble when using CCK rates
 QueIdx - 0-3, according to 802.11e/d4.4 June/2003

 Return Value:
 None

 ========================================================================
 */VOID RTUSBWriteTxDescriptor( IN PRTMP_ADAPTER pAd, IN PTXD_STRUC pSourceTxD,
		IN UCHAR CipherAlg, IN UCHAR KeyTable, IN UCHAR KeyIdx, IN BOOLEAN Ack,
		IN BOOLEAN Fragment, IN BOOLEAN InsTimestamp, IN UCHAR RetryMode,
		IN UCHAR Ifs, IN UINT Rate, IN ULONG Length, IN UCHAR QueIdx, IN UCHAR PID,
		IN BOOLEAN bAfterRTSCTS) {
	UINT Residual;

	PTXD_STRUC pTxD;

#ifndef BIG_ENDIAN
	pTxD = pSourceTxD;
#else
	TXD_STRUC TxD;

	TxD = *pSourceTxD;
	pTxD = &TxD;
	RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
#endif

	pTxD->HostQId = QueIdx;
	pTxD->MoreFrag = Fragment;
	pTxD->ACK = Ack;
	pTxD->Timestamp = InsTimestamp;
	pTxD->RetryMd = RetryMode;
	pTxD->Ofdm = (Rate < RATE_FIRST_OFDM_RATE) ? 0 : 1;
	pTxD->IFS = Ifs;
	pTxD->PktId = PID;
	pTxD->Drop = 1; // 1:valid, 0:drop
	pTxD->HwSeq = 1; // (QueIdx == QID_MGMT)? 1:0;
	pTxD->BbpTxPower = DEFAULT_BBP_TX_POWER; // TODO: to be modified
	pTxD->DataByteCnt = Length;

	RTMPCckBbpTuning(pAd, Rate);

	// fill encryption related information, if required
	pTxD->CipherAlg = CipherAlg;
	if (CipherAlg != CIPHER_NONE) {
		pTxD->KeyTable = KeyTable;
		pTxD->KeyIndex = KeyIdx;
		pTxD->TkipMic = 1;
	}

	// In TKIP+fragmentation. TKIP MIC is already appended by driver. MAC needs not generate MIC
	if (CipherAlg == CIPHER_TKIP_NO_MIC) {
		pTxD->CipherAlg = CIPHER_TKIP;
		pTxD->TkipMic = 0; // tell MAC need not insert TKIP MIC
	}

	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED)) {
		if ((pAd->PortCfg.APEdcaParm.bValid) && (QueIdx <= QID_AC_VO)) {
			pTxD->Cwmin = pAd->PortCfg.APEdcaParm.Cwmin[QueIdx];
			pTxD->Cwmax = pAd->PortCfg.APEdcaParm.Cwmax[QueIdx];
			pTxD->Aifsn = pAd->PortCfg.APEdcaParm.Aifsn[QueIdx];
		} else {
			DBGPRINT(RT_DEBUG_ERROR," WMM in used but EDCA not valid ERROR !!\n)");
		}
	} else {
		if (bAfterRTSCTS) {
			// After RTS/CTS frame, data frame should use SIFS time.
			// To patch this code, add the following code.
			// Recommended by Jerry 2005/07/25 for WiFi testing with Proxim AP
			pTxD->Cwmin = 0;
			pTxD->Cwmax = 0;
			pTxD->Aifsn = 1;
			pTxD->IFS = IFS_BACKOFF;
		} else {
			pTxD->Cwmin = CW_MIN_IN_BITS;
			pTxD->Cwmax = CW_MAX_IN_BITS;
			pTxD->Aifsn = 2;
		}
	}

	// fill up PLCP SIGNAL field
	pTxD->PlcpSignal = RateIdToPlcpSignal[Rate];
	if (((Rate == RATE_2) || (Rate == RATE_5_5) || (Rate == RATE_11))
			&& (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_SHORT_PREAMBLE_INUSED))) {
		pTxD->PlcpSignal |= 0x0008;
	}

	// fill up PLCP SERVICE field, not used for OFDM rates
	pTxD->PlcpService = 4; // Service;

	// file up PLCP LENGTH_LOW and LENGTH_HIGH fields
	Length += LENGTH_CRC; // CRC length
	switch (CipherAlg) {
	case CIPHER_WEP64:
		Length += 8;
		break; // IV + ICV
	case CIPHER_WEP128:
		Length += 8;
		break; // IV + ICV
	case CIPHER_TKIP:
		Length += 20;
		break; // IV + EIV + MIC + ICV
	case CIPHER_AES:
		Length += 16;
		break; // IV + EIV + MIC
	case CIPHER_CKIP64:
		Length += 8;
		break; // IV + CMIC + ICV, but CMIC already inserted by driver
	case CIPHER_CKIP128:
		Length += 8;
		break; // IV + CMIC + ICV, but CMIC already inserted by driver
	case CIPHER_TKIP_NO_MIC:
		Length += 12;
		break; // IV + EIV + ICV
	default:
		break;
	}

	if (Rate < RATE_FIRST_OFDM_RATE) // 11b - RATE_1, RATE_2, RATE_5_5, RATE_11
	{
		if ((Rate == RATE_1) || (Rate == RATE_2)) {
			Length = Length * 8 / (Rate + 1);
		} else {
			Residual = ((Length * 16) % (11 * (1 + Rate - RATE_5_5)));
			Length = Length * 16 / (11 * (1 + Rate - RATE_5_5));
			if (Residual != 0) {
				Length++;
			}
			if ((Residual <= (3 * (1 + Rate - RATE_5_5))) && (Residual != 0)) {
				if (Rate == RATE_11) // Only 11Mbps require length extension bit
					pTxD->PlcpService |= 0x80; // 11b's PLCP Length extension bit
			}
		}

		pTxD->PlcpLengthHigh = Length >> 8; // 256;
		pTxD->PlcpLengthLow = Length % 256;
	} else // OFDM - RATE_6, RATE_9, RATE_12, RATE_18, RATE_24, RATE_36, RATE_48, RATE_54
	{
		pTxD->PlcpLengthHigh = Length >> 6; // 64;	// high 6-bit of total byte count
		pTxD->PlcpLengthLow = Length % 64; // low 6-bit of total byte count
	}

	pTxD->Burst = Fragment;
	pTxD->Burst2 = pTxD->Burst;

#ifdef BIG_ENDIAN
	RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
	WriteBackToDescriptor((PUCHAR)pSourceTxD, (PUCHAR)pTxD, FALSE, TYPE_TXD);
#endif
}

/*
 ========================================================================

 Routine	Description:
 To do the enqueue operation and extract the first item of waiting
 list. If a number of available shared memory segments could meet
 the request of extracted item, the extracted item will be fragmented
 into shared memory segments.

 Arguments:
 pAd			Pointer	to our adapter
 pQueue		Pointer to Waiting Queue

 Return Value:
 None

 Note:

 ========================================================================
 */VOID RTMPDeQueuePacket( IN PRTMP_ADAPTER pAd, IN UCHAR BulkOutPipeId) {
	struct sk_buff *pSkb;
	UCHAR FragmentRequired;
	NDIS_STATUS Status;
	UCHAR Count = 0;
	PQUEUE_HEADER pQueue;
	UCHAR QueIdx;
	unsigned long IrqFlags;

	NdisAcquireSpinLock(&pAd->DeQueueLock[BulkOutPipeId], IrqFlags);
	if (pAd->DeQueueRunning[BulkOutPipeId]) {
		NdisReleaseSpinLock(&pAd->DeQueueLock[BulkOutPipeId], IrqFlags);
		return;
	} else {
		pAd->DeQueueRunning[BulkOutPipeId] = TRUE;
		NdisReleaseSpinLock(&pAd->DeQueueLock[BulkOutPipeId], IrqFlags);
	}

	QueIdx = BulkOutPipeId;

	if (pAd->TxRingTotalNumber[BulkOutPipeId])
		DBGPRINT(RT_DEBUG_INFO,"--RTMPDeQueuePacket %d TxRingTotalNumber= %d !!--\n", BulkOutPipeId, (INT)pAd->TxRingTotalNumber[BulkOutPipeId]);

	// Make sure SendTxWait queue resource won't be used by other threads
	NdisAcquireSpinLock(&pAd->SendTxWaitQueueLock[BulkOutPipeId], IrqFlags);

	// Select Queue
	pQueue = &pAd->SendTxWaitQueue[BulkOutPipeId];

	// Check queue before dequeue
	while ((pQueue->Head != NULL) && (Count < MAX_TX_PROCESS)) {
		// Reset is in progress, stop immediately
		if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS)
				|| RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)
				|| RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS)) {
			DBGPRINT(RT_DEBUG_ERROR,"--RTMPDeQueuePacket %d reset-in-progress !!--\n", BulkOutPipeId);
			break;
		}

		// Dequeue the first entry from head of queue list
		pSkb = (struct sk_buff*) RemoveHeadQueue(pQueue);

		// RTS or CTS-to-self for B/G protection mode has been set already.
		// There is no need to re-do it here.
		// Total fragment required = number of fragment + RST if required
		FragmentRequired = RTMP_GET_PACKET_FRAGMENTS(pSkb)
				+ RTMP_GET_PACKET_RTS(pSkb);

		if ((RTUSBFreeDescriptorRequest(pAd, TX_RING, BulkOutPipeId,
				FragmentRequired) == NDIS_STATUS_SUCCESS)) {
			// Avaliable ring descriptors are enough for this frame
			// Call hard transmit
			// Nitro mode / Normal mode selection
			NdisReleaseSpinLock(&pAd->SendTxWaitQueueLock[BulkOutPipeId], IrqFlags);

			Status = RTUSBHardTransmit(pAd, pSkb, FragmentRequired, QueIdx);

			// Acquire the resource again, snice we may need to process it in this while-loop.
			NdisAcquireSpinLock(&pAd->SendTxWaitQueueLock[BulkOutPipeId], IrqFlags);

			if (Status == NDIS_STATUS_FAILURE) {
				// Packet failed due to various Ndis Packet error
				RTUSBFreeSkbBuffer(pSkb);
				break;
			} else if (Status == NDIS_STATUS_RESOURCES) {
				// Not enough free tx ring, it might happen due to free descriptor inquery might be not correct
				// It also might change to NDIS_STATUS_FAILURE to simply drop the frame
				// Put the frame back into head of queue
				InsertHeadQueue(pQueue, pSkb);
				break;
			}
			Count++;
		} else {
			DBGPRINT(RT_DEBUG_INFO,"--RTMPDeQueuePacket %d queue full !! TxRingTotalNumber= %d !! FragmentRequired=%d !!\n", BulkOutPipeId, (INT)pAd->TxRingTotalNumber[BulkOutPipeId], FragmentRequired);
			InsertHeadQueue(pQueue, pSkb);
			pAd->PrivateInfo.TxRingFullCnt++;

			break;
		}
	}

	// Release TxSwQueue0 resources
	NdisReleaseSpinLock(&pAd->SendTxWaitQueueLock[BulkOutPipeId], IrqFlags);

#ifdef BLOCK_NET_IF
	if ((pAd->blockQueueTab[QueIdx].SwTxQueueBlockFlag == TRUE)
			&& (pAd->TxSwQueue[QueIdx].Number < 1))
	{
		releaseNetIf(&pAd->blockQueueTab[QueIdx]);
	}
#endif // BLOCK_NET_IF //
	NdisAcquireSpinLock(&pAd->DeQueueLock[BulkOutPipeId], IrqFlags);
	pAd->DeQueueRunning[BulkOutPipeId] = FALSE;
	NdisReleaseSpinLock(&pAd->DeQueueLock[BulkOutPipeId], IrqFlags);

}

/*
 ========================================================================
 Description:
 This is the completion routine for the USB_RxPacket which submits
 a URB to USBD for a transmission.
 ========================================================================
 */VOID RTUSBRxPacket( IN unsigned long data) {
	purbb_t pUrb = (purbb_t) data;
	PRTMP_ADAPTER pAd;
	PRX_CONTEXT pRxContext;
	PRXD_STRUC pRxD;
#ifdef BIG_ENDIAN
	PRXD_STRUC pDestRxD;
	RXD_STRUC RxD;
#endif
	PHEADER_802_11 pHeader;
	PUCHAR pData;
	PUCHAR pDA, pSA;
	NDIS_STATUS Status;
	USHORT DataSize, Msdu2Size;
	UCHAR Header802_3[14];
	PCIPHER_KEY pWpaKey;
	//	  struct sk_buff	  *pSkb;
	BOOLEAN EAPOLFrame;
	struct net_device *net_dev;

	int success;
	PUCHAR pRemovedLLCSNAP;
	int idx;
	BOOLEAN CheckPktSanity;
	USHORT Payload1Size, Payload2Size;
	PUCHAR pData2;

	DBGPRINT_RAW(RT_DEBUG_INFO, "--->RTUSBRxPacket\n");

	pRxContext = (PRX_CONTEXT) pUrb->context;
	pAd = pRxContext->pAd;
	net_dev = pAd->net_dev;

	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
		return;

	do {
		DBGPRINT_RAW(RT_DEBUG_INFO, "BulkIn actual length(%d)\n", pRxContext->pUrb->actual_length);
		if (pRxContext->pUrb->actual_length >= sizeof(RXD_STRUC)
				+ LENGTH_802_11) {
			pData = pRxContext->TransferBuffer;
#ifndef BIG_ENDIAN
			pRxD = (PRXD_STRUC) pData;
#else
			pDestRxD = (PRXD_STRUC) pData;
			RxD = *pDestRxD;
			pRxD = &RxD;
			RTMPDescriptorEndianChange((PUCHAR)pRxD, TYPE_RXD);
#endif

			// Cast to 802.11 header for flags checking
			pHeader = (PHEADER_802_11) (pData + sizeof(RXD_STRUC));

#ifdef BIG_ENDIAN
			RTMPFrameEndianChange(pAd, (PUCHAR)pHeader, DIR_READ, FALSE);
#endif
			if (pRxD->DataByteCnt < 4)
				Status = NDIS_STATUS_FAILURE;
			else {
				// Increase Total receive byte counter after real data received no mater any error or not
				pAd->RalinkCounters.ReceivedByteCount
						+= (pRxD->DataByteCnt - 4);
				pAd->RalinkCounters.RxCount++;

				// Check for all RxD errors
				Status = RTMPCheckRxDescriptor(pAd, pHeader, pRxD);

			}
			if (Status == NDIS_STATUS_SUCCESS) {
				// Apply packet filtering rule based on microsoft requirements.
				Status = RTMPApplyPacketFilter(pAd, pRxD, pHeader);
			}

			// Add receive counters
			if (Status == NDIS_STATUS_SUCCESS) {
				// Increase 802.11 counters & general receive counters
				INC_COUNTER64(pAd->WlanCounters.ReceivedFragmentCount);
			} else {
				// Increase general counters
				pAd->Counters.RxErrors++;
			}

#ifdef RALINK_ATE
			CHAR RealRssi;
			if((!pRxD->U2M) && pAd->ate.Mode != ATE_STASTART)
			{
				RealRssi = ConvertToRssi(pAd, (UCHAR)pRxD->PlcpRssi, RSSI_NO_1);
				pAd->PortCfg.LastRssi = RealRssi + pAd->BbpRssiToDbmDelta;
				pAd->PortCfg.AvgRssiX8 = (pAd->PortCfg.AvgRssiX8 - pAd->PortCfg.AvgRssi) + pAd->PortCfg.LastRssi;
				pAd->PortCfg.AvgRssi = pAd->PortCfg.AvgRssiX8 >> 3;
				// for smart antenna
				//	            if ((pAd->RfIcType == RFIC_5325) || (pAd->RfIcType == RFIC_2529))
				//                {
				//                    pAd->PortCfg.LastRssi2  = ConvertToRssi(pAd, (UCHAR)pRxD->PlcpSignal, RSSI_NO_2) + pAd->BbpRssiToDbmDelta;
				//                }
			}
#endif	// RALINK_ATE
			// Check for retry bit, if this bit is on, search the cache with SA & sequence
			// as index, if matched, discard this frame, otherwise, update cache
			// This check only apply to unicast data & management frames
			if ((pRxD->U2M) && (Status == NDIS_STATUS_SUCCESS)
					&& (pHeader->FC.Type != BTYPE_CNTL)) {
				if (pHeader->FC.Retry) {
					if (RTMPSearchTupleCache(pAd, pHeader) == TRUE) {
						// Found retry frame in tuple cache, Discard this frame / fragment
						// Increase 802.11 counters
						INC_COUNTER64(pAd->WlanCounters.FrameDuplicateCount);
						DBGPRINT_RAW(RT_DEBUG_INFO, "duplicate frame %d\n", pHeader->Sequence);
						Status = NDIS_STATUS_FAILURE;
					} else {
						RTMPUpdateTupleCache(pAd, pHeader);
					}
				} else // Update Tuple Cache
				{
					RTMPUpdateTupleCache(pAd, pHeader);
				}
			}

			if ((pRxD->U2M) || ((pHeader->FC.SubType == SUBTYPE_BEACON)
					&& (MAC_ADDR_EQUAL(&pAd->PortCfg.Bssid, &pHeader->Addr2)))) {
				if ((pAd->Antenna.field.NumOfAntenna == 2)
						&& (pAd->Antenna.field.TxDefaultAntenna == 0)
						&& (pAd->Antenna.field.RxDefaultAntenna == 0)) {
					COLLECT_RX_ANTENNA_AVERAGE_RSSI(pAd, ConvertToRssi(pAd, (UCHAR)pRxD->PlcpRssi, RSSI_NO_1), 0); //Note: RSSI2 not used on RT73
					pAd->PortCfg.NumOfAvgRssiSample++;
				}
			}

			//
			// Do RxD release operation	for	all	failure	frames
			//
			if (Status == NDIS_STATUS_SUCCESS) {
				do {

					if (VIRTUAL_IF_NUM(pAd) == 0)
						break;

					// pData : Pointer skip	the RxD Descriptior and the first 24 bytes,	802.11 HEADER
					pData += LENGTH_802_11 + sizeof(RXD_STRUC);
					DataSize = (USHORT) pRxD->DataByteCnt - LENGTH_802_11;

					//
					// CASE I. receive a DATA frame
					//
					if (pHeader->FC.Type == BTYPE_DATA) {
						// before LINK UP, all DATA frames are rejected
						if (!OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED)) {
							DBGPRINT(RT_DEBUG_INFO,"RxDone- drop DATA frame before LINK UP(len=%d)\n",pRxD->DataByteCnt);
							break;
						}
						pAd->BulkInDataOneSecCount++;

						// remove the 2 extra QOS CNTL bytes
						if (pHeader->FC.SubType & 0x08) {
							pData += 2;
							DataSize -= 2;
						}

						// remove the 2 extra AGGREGATION bytes
						Msdu2Size = 0;
						if (pHeader->FC.Order) {
							Msdu2Size = *pData + (*(pData + 1) << 8);
							if ((Msdu2Size <= 1536) && (Msdu2Size < DataSize)) {
								pData += 2;
								DataSize -= 2;
							} else
								Msdu2Size = 0;
						}

						// Drop not my BSS frame
						//
						// Not drop EAPOL frame, since this have happen on the first time that we link up
						// And need some more time to set BSSID to asic
						// So pRxD->MyBss may be 0
						//
						if (RTMPEqualMemory(EAPOL, pData + 6, 2))
							EAPOLFrame = TRUE;
						else
							EAPOLFrame = FALSE;

						if ((pRxD->MyBss == 0) && (EAPOLFrame != TRUE))
							break; // give up this frame

						// Drop NULL (+CF-POLL) (+CF-ACK) data frame
						if ((pHeader->FC.SubType & 0x04) == 0x04) {
							DBGPRINT(RT_DEBUG_TRACE,"RxDone- drop NULL frame(subtype=%d)\n",pHeader->FC.SubType);
							break;
						}

						// prepare 802.3 header: DA=addr1; SA=addr3 in INFRA mode, DA=addr2 in ADHOC mode
						pDA = pHeader->Addr1;
						if (INFRA_ON(pAd))
							pSA = pHeader->Addr3;
						else
							pSA = pHeader->Addr2;

						if (pHeader->FC.Wep) // frame received in encrypted format
						{
							if (pRxD->CipherAlg == CIPHER_NONE) // unsupported cipher suite
							{
								break; // give up this frame
							} else if (pAd->SharedKey[pRxD->KeyIndex].KeyLen
									== 0) {
								break; // give up this frame since the keylen is invalid.
							}
						} else { // frame received in clear text
							// encryption in-use but receive a non-EAPOL clear text frame, drop it
							if (((pAd->PortCfg.WepStatus
									== Ndis802_11Encryption1Enabled)
									|| (pAd->PortCfg.WepStatus
											== Ndis802_11Encryption2Enabled)
									|| (pAd->PortCfg.WepStatus
											== Ndis802_11Encryption3Enabled))
									&& (pAd->PortCfg.PrivacyFilter
											== Ndis802_11PrivFilter8021xWEP)
									&& (!NdisEqualMemory(EAPOL_LLC_SNAP, pData, LENGTH_802_1_H))) {
								break; // give up this frame
							}
						}

						//
						// Case I.1  Process Broadcast & Multicast data frame
						//
						if (pRxD->Bcast || pRxD->Mcast) {
							INC_COUNTER64(pAd->WlanCounters.MulticastReceivedFrameCount);

							// Drop Mcast/Bcast frame with fragment bit on
							if (pHeader->FC.MoreFrag) {
								break; // give up this frame
							}

							// Filter out Bcast frame which AP relayed for us
							if (pHeader->FC.FrDs
									&& MAC_ADDR_EQUAL(pHeader->Addr3, pAd->CurrentAddress)) {
								break; // give up this frame
							}

							// build 802.3 header and decide if remove the 8-byte LLC/SNAP encapsulation
							CONVERT_TO_802_3(Header802_3, pDA, pSA, pData, DataSize, pRemovedLLCSNAP);
							REPORT_ETHERNET_FRAME_TO_LLC(pAd, Header802_3,
									pData, DataSize, net_dev);
							DBGPRINT(RT_DEBUG_TRACE, "!!! report BCAST DATA to LLC (len=%d) !!!\n", DataSize);
						}
						//
						// Case I.2  Process unicast-to-me DATA frame
						//
						else if (pRxD->U2M) {
							RECORD_LATEST_RX_DATA_RATE(pAd, pRxD);
							//#ifdef RALINK_WPA_SUPPLICANT_SUPPORT
							if (pAd->PortCfg.WPA_Supplicant == TRUE) {
								// All EAPoL frames have to pass to upper layer (ex. WPA_SUPPLICANT daemon)
								// TBD : process fragmented EAPol frames
								//2007/12/21:Carella add to fix fragmentation bugs in 802.1x(start)
								if (pHeader->Frag == 0) { // First or Only fragment
									if (pHeader->FC.MoreFrag == FALSE) { // One & The only fragment
										if (NdisEqualMemory(EAPOL_LLC_SNAP, pData, LENGTH_802_1_H)) {
											success = 0;

											// In 802.1x mode, if the received frame is EAP-SUCCESS packet, turn on the PortSecured variable
											if (pAd->PortCfg.IEEE8021X == TRUE
													&& (EAP_CODE_SUCCESS
															== RTMPCheckWPAframeForEapCode(
																	pAd, pData,
																	DataSize,
																	LENGTH_802_1_H))) {
												DBGPRINT_RAW(RT_DEBUG_TRACE, "Receive EAP-SUCCESS Packet\n");
												pAd->PortCfg.PortSecured
														= WPA_802_1X_PORT_SECURED;

												success = 1;
											}

											// build 802.3 header and decide if remove the 8-byte LLC/SNAP encapsulation
											CONVERT_TO_802_3(Header802_3, pDA, pSA, pData, DataSize, pRemovedLLCSNAP);
											REPORT_ETHERNET_FRAME_TO_LLC(pAd,
													Header802_3, pData,
													DataSize, net_dev);
											DBGPRINT_RAW(RT_DEBUG_TRACE, "!!! report EAPoL DATA to LLC (len=%d) !!!\n", DataSize);

											if (success) {
												// For static wep mode, need to set wep key to Asic again
												if (pAd->PortCfg.IEEE8021x_required_keys
														== 0) {
													idx
															= pAd->PortCfg.DefaultKeyId;

													DBGPRINT_RAW(RT_DEBUG_TRACE, "Set WEP key to Asic again =>\n");
													if (pAd->PortCfg.DesireSharedKey[idx].KeyLen
															!= 0) {
														pAd->SharedKey[idx].KeyLen
																= pAd->PortCfg.DesireSharedKey[idx].KeyLen;
														NdisMoveMemory(pAd->SharedKey[idx].Key, pAd->PortCfg.DesireSharedKey[idx].Key, pAd->SharedKey[idx].KeyLen);
														pAd->SharedKey[idx].CipherAlg
																= pAd->PortCfg.DesireSharedKey[idx].CipherAlg;
														AsicAddSharedKeyEntry(
																pAd,
																0,
																(UCHAR) idx,
																pAd->SharedKey[idx].CipherAlg,
																pAd->SharedKey[idx].Key,
																NULL, NULL);
													}
												}
											}
											break; //After reporting to LLC, must break
										}
									}//End of One & The only fragment
									else { // First fragment - record the 802.3 header and frame body
										if (NdisEqualMemory(EAPOL_LLC_SNAP, pData, LENGTH_802_1_H)) {
											NdisMoveMemory(&pAd->FragFrame.Buffer[LENGTH_802_3], pData, DataSize);
											NdisMoveMemory(pAd->FragFrame.Header802_3, Header802_3, LENGTH_802_3);
											pAd->FragFrame.RxSize = DataSize;
											pAd->FragFrame.Sequence
													= pHeader->Sequence;
											pAd->FragFrame.LastFrag
													= pHeader->Frag; // Should be 0
											break;
										}
									}
								}//End of First or Only fragment
								else {
									// Middle & End of fragment burst fragments
									// No LLC-SNAP header in except the first fragment frame
									// Here only handle the EAPOL middle or final fragment
									if (RTMPEqualMemory(
											SNAP_802_1H,
											(PUCHAR) &pAd->FragFrame.Buffer[LENGTH_802_3],
											6)) {
										if ((pHeader->Sequence
												!= pAd->FragFrame.Sequence)
												|| (pHeader->Frag
														!= (pAd->FragFrame.LastFrag
																+ 1))) { // Fragment is not the same sequence or out of fragment number order
											// Clear Fragment frame contents
											NdisZeroMemory(&pAd->FragFrame, sizeof(FRAGMENT_FRAME));
											break; // give up this frame
										} else if ((pAd->FragFrame.RxSize
												+ DataSize) > MAX_FRAME_SIZE) { // Fragment frame is too large, it exeeds the maximum frame size.
											// Clear Fragment frame contents
											NdisZeroMemory(&pAd->FragFrame, sizeof(FRAGMENT_FRAME));
											break; // give up this frame
										}

										// concatenate this fragment into the re-assembly buffer
										NdisMoveMemory(&pAd->FragFrame.Buffer[LENGTH_802_3 + pAd->FragFrame.RxSize], pData, DataSize);
										pAd->FragFrame.RxSize += DataSize;
										pAd->FragFrame.LastFrag = pHeader->Frag; // Update fragment number

										success = 0;
										// Last fragment
										if (pHeader->FC.MoreFrag == FALSE) {
											CheckPktSanity = TRUE;
											if (pAd->FragFrame.RxSize
													< (LENGTH_802_1_H
															+ LENGTH_EAPOL_H)) {
												CheckPktSanity = FALSE;
												DBGPRINT(RT_DEBUG_ERROR, "Total pkts size is too small.\n");
											} else if (!RTMPEqualMemory(
													SNAP_802_1H,
													(PUCHAR) &pAd->FragFrame.Buffer[LENGTH_802_3],
													6)) {
												CheckPktSanity = FALSE;
												DBGPRINT(RT_DEBUG_ERROR, "Can't find SNAP_802_1H parameter.\n");
											} else if (!RTMPEqualMemory(
													EAPOL,
													(PUCHAR) &pAd->FragFrame.Buffer[LENGTH_802_3
															+ 6], 2)) {
												CheckPktSanity = FALSE;
												DBGPRINT(RT_DEBUG_ERROR, "Can't find EAPOL parameter.\n");
											} else if (pAd->FragFrame.Buffer[LENGTH_802_3
													+ 9] > EAPOLASFAlert) {
												CheckPktSanity = FALSE;
												DBGPRINT(RT_DEBUG_ERROR, "Unknown EAP type(%d).\n",pAd->FragFrame.Buffer[LENGTH_802_3+9]);
											}

											if (CheckPktSanity == FALSE) {
												NdisZeroMemory(&pAd->FragFrame, sizeof(FRAGMENT_FRAME));
												break;
											}
											if (pAd->PortCfg.IEEE8021X == TRUE) {
												DBGPRINT_RAW(RT_DEBUG_TRACE, "Receive EAP-SUCCESS Packet\n");
												pAd->PortCfg.PortSecured
														= WPA_802_1X_PORT_SECURED;
												success = 1;
											}

											pData
													= &pAd->FragFrame.Buffer[LENGTH_802_3];
											CONVERT_TO_802_3(pAd->FragFrame.Header802_3, pDA, pSA, pData, pAd->FragFrame.RxSize, pRemovedLLCSNAP);
											REPORT_ETHERNET_FRAME_TO_LLC(pAd,
													pAd->FragFrame.Header802_3,
													pData,
													pAd->FragFrame.RxSize,
													net_dev);

											if (success) {
												// For static wep mode, need to set wep key to Asic again
												if (pAd->PortCfg.IEEE8021x_required_keys
														== 0) {
													idx
															= pAd->PortCfg.DefaultKeyId;
													DBGPRINT_RAW(RT_DEBUG_TRACE, "Set WEP key to Asic again =>\n");

													if (pAd->PortCfg.DesireSharedKey[idx].KeyLen
															!= 0) {
														pAd->SharedKey[idx].KeyLen
																= pAd->PortCfg.DesireSharedKey[idx].KeyLen;
														NdisMoveMemory(pAd->SharedKey[idx].Key, pAd->PortCfg.DesireSharedKey[idx].Key, pAd->SharedKey[idx].KeyLen);
														pAd->SharedKey[idx].CipherAlg
																= pAd->PortCfg.DesireSharedKey[idx].CipherAlg;
														AsicAddSharedKeyEntry(
																pAd,
																0,
																(UCHAR) idx,
																pAd->SharedKey[idx].CipherAlg,
																pAd->SharedKey[idx].Key,
																NULL, NULL);
													}
												}
											}

											// Clear Fragment frame contents
											NdisZeroMemory(&pAd->FragFrame, sizeof(FRAGMENT_FRAME));
										}//-- End of the Last fragment --//
										break; //Middle or Final fragment has been handled ok , so break out
									}
								}//-- End Middle & End of fragment burst fragments	--//
							}//-- End of WPA_Supplicant == TRUE --//
							else {
								//#else
								// Special DATA frame that has to pass to MLME
								//	 1. EAPOL handshaking frames when driver supplicant enabled, pass to MLME for special process
								if ((pHeader->Frag == 0)
										&& (pHeader->FC.MoreFrag == FALSE)) // add by johnli, fix WPAPSK/WPA2PSK bugs for receiving EAPoL fragmentation packets						{
								{
									if (NdisEqualMemory(EAPOL_LLC_SNAP, pData, LENGTH_802_1_H)
											&& (pAd->PortCfg.WpaState
													!= SS_NOTUSE)) {
										// edit by johnli, fix WPAPSK/WPA2PSK bugs for receiving EAPoL fragmentation packets
										/*
										 DataSize += LENGTH_802_11;
										 REPORT_MGMT_FRAME_TO_MLME(pAd, pHeader, DataSize, pRxD->PlcpRssi, pRxD->PlcpSignal);
										 */REPORT_MGMT_FRAME_TO_MLME(pAd, pHeader, pData, DataSize, pRxD->PlcpRssi, pRxD->PlcpSignal);
										// end johnli
										DBGPRINT_RAW(RT_DEBUG_TRACE, "!!! report EAPOL/AIRONET DATA to MLME (len=%d) !!!\n", DataSize);
										break; // end of processing this frame
									}
								}
								//#endif
							}
							//2007/12/21:Carella add to fix fragmentation bugs in 802.1x(End)
							if (pHeader->Frag == 0) // First or Only fragment
							{
								CONVERT_TO_802_3(Header802_3, pDA, pSA, pData, DataSize, pRemovedLLCSNAP);
								pAd->FragFrame.Flags &= 0xFFFFFFFE;

								// Firt Fragment & LLC/SNAP been removed. Keep the removed LLC/SNAP for later on
								// TKIP MIC verification.
								if (pHeader->FC.MoreFrag && pRemovedLLCSNAP) {
									NdisMoveMemory(pAd->FragFrame.Header802_11, pHeader, LENGTH_802_11); // add by johnli, fix WPAPSK/WPA2PSK bugs for receiving EAPoL fragmentation packets
									NdisMoveMemory(pAd->FragFrame.Header_LLC, pRemovedLLCSNAP, LENGTH_802_1_H);
									pAd->FragFrame.Flags |= 0x01;
								}

								// One & The only fragment
								if (pHeader->FC.MoreFrag == FALSE) {
									if ((pHeader->FC.Order == 1) && (Msdu2Size
											> 0)) // this is an aggregation
									{
										pAd->RalinkCounters.OneSecRxAggregationCount++;
										Payload1Size = DataSize - Msdu2Size;
										Payload2Size = Msdu2Size - LENGTH_802_3;

										REPORT_ETHERNET_FRAME_TO_LLC(pAd,
												Header802_3, pData,
												Payload1Size, net_dev);
										DBGPRINT(RT_DEBUG_INFO, "!!! report segregated MSDU1 to LLC (len=%d, proto=%02x:%02x) %02x:%02x:%02x:%02x-%02x:%02x:%02x:%02x\n",
												LENGTH_802_3+Payload1Size, Header802_3[12], Header802_3[13],
												*pData, *(pData+1),*(pData+2),*(pData+3),*(pData+4),*(pData+5),*(pData+6),*(pData+7));

										pData2 = pData + Payload1Size
												+ LENGTH_802_3;
										REPORT_ETHERNET_FRAME_TO_LLC(pAd,
												pData + Payload1Size, pData2,
												Payload2Size, net_dev);
										DBGPRINT_RAW(RT_DEBUG_INFO, "!!! report segregated MSDU2 to LLC (len=%d, proto=%02x:%02x) %02x:%02x:%02x:%02x-%02x:%02x:%02x:%02x\n",
												LENGTH_802_3+Payload2Size, *(pData2 -2), *(pData2 - 1),
												*pData2, *(pData2+1),*(pData2+2),*(pData2+3),*(pData2+4),*(pData2+5),*(pData2+6),*(pData2+7));
									} else {
										REPORT_ETHERNET_FRAME_TO_LLC(pAd,
												Header802_3, pData, DataSize,
												net_dev);
										DBGPRINT_RAW(RT_DEBUG_INFO, "!!! report DATA (no frag) to LLC (len=%d, proto=%02x:%02x) %02x:%02x:%02x:%02x-%02x:%02x:%02x:%02x\n",
												DataSize, Header802_3[12], Header802_3[13],
												*pData, *(pData+1),*(pData+2),*(pData+3),*(pData+4),*(pData+5),*(pData+6),*(pData+7));
									}
								}
								// First fragment - record the 802.3 header and frame body
								else {
									NdisMoveMemory(&pAd->FragFrame.Buffer[LENGTH_802_3], pData, DataSize);
									NdisMoveMemory(pAd->FragFrame.Header802_3, Header802_3, LENGTH_802_3);
									pAd->FragFrame.RxSize = DataSize;
									pAd->FragFrame.Sequence = pHeader->Sequence;
									pAd->FragFrame.LastFrag = pHeader->Frag; // Should be 0
								}
							} //First or Only fragment
							// Middle & End of fragment burst fragments
							else {
								// No LLC-SNAP header in except the first fragment frame
								if ((pHeader->Sequence
										!= pAd->FragFrame.Sequence)
										|| (pHeader->Frag
												!= (pAd->FragFrame.LastFrag + 1))) {
									// Fragment is not the same sequence or out of fragment number order
									// Clear Fragment frame contents
									NdisZeroMemory(&pAd->FragFrame, sizeof(FRAGMENT_FRAME));
									break;
								} else if ((pAd->FragFrame.RxSize + DataSize)
										> MAX_FRAME_SIZE) {
									// Fragment frame is too large, it exeeds the maximum frame size.
									// Clear Fragment frame contents
									NdisZeroMemory(&pAd->FragFrame, sizeof(FRAGMENT_FRAME));
									break; // give up this frame
								}

								// concatenate this fragment into the re-assembly buffer
								NdisMoveMemory(&pAd->FragFrame.Buffer[LENGTH_802_3 + pAd->FragFrame.RxSize], pData, DataSize);
								pAd->FragFrame.RxSize += DataSize;
								pAd->FragFrame.LastFrag = pHeader->Frag; // Update fragment number

								// Last fragment
								if (pHeader->FC.MoreFrag == FALSE) {
									// add by johnli, fix WPAPSK/WPA2PSK bugs for receiving EAPoL fragmentation packets
									if (pAd->FragFrame.Flags & 0x00000001) {
										// originally there's an LLC/SNAP field in the first fragment
										// but been removed in re-assembly buffer. here we have to include
										// this LLC/SNAP field upon calculating TKIP MIC
										// pData = pAd->FragFrame.Header_LLC;
										// Copy LLC data to the position in front of real data for MIC calculation
										NdisMoveMemory(&pAd->FragFrame.Buffer[LENGTH_802_3 - LENGTH_802_1_H],
												pAd->FragFrame.Header_LLC,
												LENGTH_802_1_H);
										pData
												= (PUCHAR) &pAd->FragFrame.Buffer[LENGTH_802_3
														- LENGTH_802_1_H];
										DataSize
												= (USHORT) pAd->FragFrame.RxSize
														+ LENGTH_802_1_H;
									} else {
										pData
												= (PUCHAR) &pAd->FragFrame.Buffer[LENGTH_802_3];
										DataSize
												= (USHORT) pAd->FragFrame.RxSize;
									}
									// end johnli

									// For TKIP frame, calculate the MIC value
									if (pRxD->CipherAlg == CIPHER_TKIP) {
										pWpaKey = &pAd->SharedKey[0]; // pRxD->KeyIndex -> 0

										// Minus MIC length
										pAd->FragFrame.RxSize -= 8;
										DataSize -= 8; // add by johnli, fix WPAPSK/WPA2PSK bugs for receiving EAPoL fragmentation packets

										/* remove by johnli, fix WPAPSK/WPA2PSK bugs for receiving EAPoL fragmentation packets
										 if (pAd->FragFrame.Flags & 0x00000001)
										 {
										 // originally there's an LLC/SNAP field in the first fragment
										 // but been removed in re-assembly buffer. here we have to include
										 // this LLC/SNAP field upon calculating TKIP MIC
										 // pData = pAd->FragFrame.Header_LLC;
										 // Copy LLC data to the position in front of real data for MIC calculation
										 NdisMoveMemory(&pAd->FragFrame.Buffer[LENGTH_802_3 - LENGTH_802_1_H],
										 pAd->FragFrame.Header_LLC,
										 LENGTH_802_1_H);
										 pData = (PUCHAR) &pAd->FragFrame.Buffer[LENGTH_802_3 - LENGTH_802_1_H];
										 DataSize = (USHORT)pAd->FragFrame.RxSize + LENGTH_802_1_H;
										 }
										 else
										 {
										 pData = (PUCHAR) &pAd->FragFrame.Buffer[LENGTH_802_3];
										 DataSize = (USHORT)pAd->FragFrame.RxSize;
										 }
										 */

										if (RTMPTkipCompareMICValue(pAd, pData,
												pDA, pSA, pWpaKey->RxMic,
												DataSize) == FALSE) {
											DBGPRINT_RAW(RT_DEBUG_ERROR,"Rx MIC Value error 2\n");
											RTMPReportMicError(pAd, pWpaKey);
											break; // give up this frame
										}
									}

									// edit by johnli, fix WPAPSK/WPA2PSK bugs for receiving EAPoL fragmentation packets
									/*
									 pData = &pAd->FragFrame.Buffer[LENGTH_802_3];
									 REPORT_ETHERNET_FRAME_TO_LLC(pAd, pAd->FragFrame.Header802_3, pData, pAd->FragFrame.RxSize, net_dev);
									 DBGPRINT(RT_DEBUG_TRACE, "!!! report DATA (fragmented) to LLC (len=%d) !!!\n", pAd->FragFrame.RxSize);
									 */
									// Special DATA frame that has to pass to MLME
									//	 1. EAPOL handshaking frames when driver supplicant enabled, pass to MLME for special process
									if ((pAd->FragFrame.Flags & 0x00000001)
											&& NdisEqualMemory(EAPOL_LLC_SNAP, pData, LENGTH_802_1_H)
											&& (pAd->PortCfg.WpaState
													!= SS_NOTUSE)) {
										REPORT_MGMT_FRAME_TO_MLME(pAd, pAd->FragFrame.Header802_11, pData, DataSize, pRxD->PlcpRssi, pRxD->PlcpSignal); DBGPRINT_RAW(RT_DEBUG_TRACE, "!!! report EAPOL/AIRONET DATA to MLME (len=%d) !!!\n", DataSize);
									} else {
										pData
												= &pAd->FragFrame.Buffer[LENGTH_802_3];
										REPORT_ETHERNET_FRAME_TO_LLC(pAd,
												pAd->FragFrame.Header802_3,
												pData, pAd->FragFrame.RxSize,
												net_dev);
										DBGPRINT(RT_DEBUG_TRACE, "!!! report DATA (fragmented) to LLC (len=%d) !!!\n", pAd->FragFrame.RxSize);
									}
									// end johnli
								}
							}
						}
					} // FC.Type == BTYPE_DATA
					//
					// CASE II. receive a MGMT frame
					//
					else if (pHeader->FC.Type == BTYPE_MGMT) {
						// edit by johnli, fix WPAPSK/WPA2PSK bugs for receiving EAPoL fragmentation packets
						//					REPORT_MGMT_FRAME_TO_MLME(pAd, pHeader, pRxD->DataByteCnt, pRxD->PlcpRssi, pRxD->PlcpSignal);
						REPORT_MGMT_FRAME_TO_MLME(pAd, pHeader, pData, DataSize, pRxD->PlcpRssi, pRxD->PlcpSignal);
						// end johnli
						break; // end of processing this frame
					}
					//
					// CASE III. receive a CNTL frame
					//
					else if (pHeader->FC.Type == BTYPE_CNTL)
						break; // give up this frame
					//
					// CASE IV. receive a frame of invalid type
					//
					else
						break; // give up this frame
				} while (FALSE); // ************* exit point *********

			}//if (Status == NDIS_STATUS_SUCCESS)

			else if (Status == NDIS_STATUS_RESET) {
				RTUSBEnqueueInternalCmd(pAd, RT_OID_USB_RESET_BULK_IN);
				return;
			}

#ifdef BIG_ENDIAN
			RTMPFrameEndianChange(pAd, (PUCHAR)pHeader, DIR_WRITE, TRUE);
			RTMPDescriptorEndianChange((PUCHAR)pRxD, TYPE_RXD);
			WriteBackToDescriptor((PUCHAR)pDestRxD, (PUCHAR)pRxD, FALSE, TYPE_RXD);
#endif
		}//if (pRxContext->pUrb->actual_length >= sizeof(RXD_STRUC)+ LENGTH_802_11)


		pRxContext->InUse = FALSE;
		//iverson patch usb 1.1 or 2.0 2007 1109
		if (pAd->BulkOutMaxPacketSize == 512) {
			if ((!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS))
					&& (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BULKIN_RESET))
					&& (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF))
					&& (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS)) &&
#ifdef RALINK_ATE
					(!(pAd->ate.Mode != ATE_STASTART)) &&
#endif
					(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))) {
				RTUSBBulkReceive(pAd);
			}
		} else {
			DBGPRINT_RAW(RT_DEBUG_INFO, "In USB 1.1 Mode \n");
		}
		//iversonend
#ifdef RALINK_ATE
		//If the driver is in ATE mode and Rx frame is set into here.
		if((pAd->ate.Mode != ATE_STASTART) && (pAd->ContinBulkIn == TRUE))
		{
			RTUSBBulkReceive(pAd);
		}

#endif

	} while (0);

	DBGPRINT_RAW(RT_DEBUG_INFO, "<---RTUSBRxPacket Complete\n");
}

/*
 ========================================================================

 Routine Description:

 Arguments:

 Return Value:

 Note:

 ========================================================================
 */VOID RTUSBDequeueMLMEPacket( IN PRTMP_ADAPTER pAd) {
	PMGMT_STRUC pMgmt;
	unsigned long IrqFlags;
	unsigned long IrqFlags2;

	DBGPRINT(RT_DEBUG_INFO, "RTUSBDequeueMLMEPacket\n");

	NdisAcquireSpinLock(&pAd->DeMGMTQueueLock, IrqFlags2);
	if (pAd->DeMGMTQueueRunning) {
		DBGPRINT(RT_DEBUG_ERROR, "<---RTUSBDequeueMLMEPacket : DeMGMTQueueRunning\n");
		NdisReleaseSpinLock(&pAd->DeMGMTQueueLock, IrqFlags2);
		return;
	} else {
		pAd->DeMGMTQueueRunning = TRUE;
		NdisReleaseSpinLock(&pAd->DeMGMTQueueLock, IrqFlags2);
	}

	NdisAcquireSpinLock(&pAd->MLMEWaitQueueLock, IrqFlags);
	while ((pAd->PopMgmtIndex != pAd->PushMgmtIndex) || (atomic_read(
			&pAd->MgmtQueueSize) > 0)) {
		pMgmt = &pAd->MgmtRing[pAd->PopMgmtIndex];

		if (RTUSBFreeDescriptorRequest(pAd, PRIO_RING, 0, 1)
				== NDIS_STATUS_SUCCESS) {
			atomic_dec(&pAd->MgmtQueueSize);
			pAd->PopMgmtIndex = (pAd->PopMgmtIndex + 1) % MGMT_RING_SIZE;
			NdisReleaseSpinLock(&pAd->MLMEWaitQueueLock, IrqFlags);

			RTUSBMlmeHardTransmit(pAd, pMgmt);

			MlmeFreeMemory(pAd, pMgmt->pBuffer);
			pMgmt->pBuffer = NULL;
			pMgmt->Valid = FALSE;

			NdisAcquireSpinLock(&pAd->MLMEWaitQueueLock, IrqFlags);
		} else {
			DBGPRINT(RT_DEBUG_TRACE, "not enough space in PrioRing[pAdapter->MgmtQueueSize=%d]\n", atomic_read(&pAd->MgmtQueueSize)); DBGPRINT(RT_DEBUG_TRACE, "RTUSBDequeueMLMEPacket::PrioRingFirstIndex = %d, PrioRingTxCnt = %d, PopMgmtIndex = %d, PushMgmtIndex = %d, NextMLMEIndex = %d\n",
					pAd->PrioRingFirstIndex, pAd->PrioRingTxCnt,
					pAd->PopMgmtIndex, pAd->PushMgmtIndex, pAd->NextMLMEIndex);
			break;
		}
	}
	NdisReleaseSpinLock(&pAd->MLMEWaitQueueLock, IrqFlags);

	NdisAcquireSpinLock(&pAd->DeMGMTQueueLock, IrqFlags2);
	pAd->DeMGMTQueueRunning = FALSE;
	NdisReleaseSpinLock(&pAd->DeMGMTQueueLock, IrqFlags2);
}

/*
 ========================================================================

 Routine Description:

 Arguments:

 Return Value:

 Note:

 ========================================================================
 */VOID RTUSBCleanUpMLMEWaitQueue( IN PRTMP_ADAPTER pAd) {
	PMGMT_STRUC pMgmt;
	unsigned long IrqFlags;

	DBGPRINT(RT_DEBUG_TRACE, "--->CleanUpMLMEWaitQueue\n");

	NdisAcquireSpinLock(&pAd->MLMEWaitQueueLock, IrqFlags);
	while (pAd->PopMgmtIndex != pAd->PushMgmtIndex) {
		pMgmt = (PMGMT_STRUC) &pAd->MgmtRing[pAd->PopMgmtIndex];
		MlmeFreeMemory(pAd, pMgmt->pBuffer);
		pMgmt->pBuffer = NULL;
		pMgmt->Valid = FALSE;
		atomic_dec(&pAd->MgmtQueueSize);

		pAd->PopMgmtIndex++;
		if (pAd->PopMgmtIndex >= MGMT_RING_SIZE) {
			pAd->PopMgmtIndex = 0;
		}
	}
	NdisReleaseSpinLock(&pAd->MLMEWaitQueueLock, IrqFlags);

	DBGPRINT(RT_DEBUG_TRACE, "<---CleanUpMLMEWaitQueue\n");
}

/*
 ========================================================================

 Routine	Description:
 Suspend MSDU transmission

 Arguments:
 pAd		Pointer	to our adapter

 Return Value:
 None

 Note:

 ========================================================================
 */VOID RTUSBSuspendMsduTransmission( IN PRTMP_ADAPTER pAd) {
	DBGPRINT(RT_DEBUG_TRACE,"SCANNING, suspend MSDU transmission ...\n");

	//
	// Before BSS_SCAN_IN_PROGRESS, we need to keep Current R17 value and
	// use Lowbound as R17 value on ScanNextChannel(...)
	//
	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
		RTUSBReadBBPRegister(pAd, 17, &pAd->BbpTuning.R17CurrentValue);

	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS);
}

/*
 ========================================================================

 Routine	Description:
 Resume MSDU transmission

 Arguments:
 pAd		Pointer	to our adapter

 Return Value:
 None

 Note:

 ========================================================================
 */VOID RTUSBResumeMsduTransmission( IN PRTMP_ADAPTER pAd) {
	INT Index;

	DBGPRINT(RT_DEBUG_ERROR,"SCAN done, resume MSDU transmission ...\n");
	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS);

	//
	// After finish BSS_SCAN_IN_PROGRESS, we need to restore Current R17 value
	//
	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
		RTUSBWriteBBPRegister(pAd, 17, pAd->BbpTuning.R17CurrentValue);

	if ((!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS))
			&& (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS))
			&& (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF))) {
		// Dequeue all Tx software queue, if have been queued.
		for (Index = 0; Index < 4; Index++) {
			if (pAd->SendTxWaitQueue[Index].Number > 0) {
				RTMPDeQueuePacket(pAd, Index);
			}
		}
	}

	// Kick bulk out
	RTUSBKickBulkOut(pAd);
}

/*
 ========================================================================

 Routine	Description:
 API for MLME to transmit management frame to AP (BSS Mode)
 or station (IBSS Mode)

 Arguments:
 pAd			Pointer	to our adapter
 Buffer		Pointer to	memory of outgoing frame
 Length		Size of outgoing management frame

 Return Value:
 NDIS_STATUS_FAILURE
 NDIS_STATUS_PENDING
 NDIS_STATUS_SUCCESS

 Note:

 ========================================================================
 */VOID MiniportMMRequest( IN PRTMP_ADAPTER pAd, IN UCHAR QueIdx,
		IN PVOID pBuffer, IN ULONG Length) {
	unsigned long IrqFlags;

	DBGPRINT_RAW(RT_DEBUG_INFO, "---> MiniportMMRequest\n");

	if (pBuffer) {
		PMGMT_STRUC pMgmt;

		// Check management ring free avaliability
		NdisAcquireSpinLock(&pAd->MLMEWaitQueueLock, IrqFlags);
		pMgmt = (PMGMT_STRUC) &pAd->MgmtRing[pAd->PushMgmtIndex];
		// This management cell has been occupied
		if (pMgmt->Valid == TRUE) {
			NdisReleaseSpinLock(&pAd->MLMEWaitQueueLock, IrqFlags);
			MlmeFreeMemory(pAd, pBuffer);
			pAd->RalinkCounters.MgmtRingFullCount++;
			DBGPRINT_RAW(RT_DEBUG_WARN, "MiniportMMRequest (error:: MgmtRing full)\n");
		}
		// Insert this request into software managemnet ring
		else {
			pMgmt->pBuffer = pBuffer;
			pMgmt->Length = Length;
			pMgmt->Valid = TRUE;
			pAd->PushMgmtIndex++;
			atomic_inc(&pAd->MgmtQueueSize);
			if (pAd->PushMgmtIndex >= MGMT_RING_SIZE) {
				pAd->PushMgmtIndex = 0;
			}
			NdisReleaseSpinLock(&pAd->MLMEWaitQueueLock, IrqFlags);
		}
	} else
		DBGPRINT(RT_DEBUG_WARN, "MiniportMMRequest (error:: NULL msg)\n");

	RTUSBDequeueMLMEPacket(pAd);

	// If pAd->PrioRingTxCnt is larger than 0, this means that prio_ring have something to transmit.
	// Then call KickBulkOut to transmit it
	if (pAd->PrioRingTxCnt > 0) {
		if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_DOZE))
			AsicForceWakeup(pAd);
		RTUSBKickBulkOut(pAd);
	}

	DBGPRINT_RAW(RT_DEBUG_INFO, "<--- MiniportMMRequest\n");
}

/*
 ========================================================================

 Routine	Description:
 Search tuple cache for receive duplicate frame from unicast frames.

 Arguments:
 pAd				Pointer	to our adapter
 pHeader			802.11 header of receiving frame

 Return Value:
 TRUE			found matched tuple cache
 FALSE			no matched found

 Note:

 ========================================================================
 */BOOLEAN RTMPSearchTupleCache( IN PRTMP_ADAPTER pAd, IN PHEADER_802_11 pHeader) {
	INT Index;

	for (Index = 0; Index < MAX_CLIENT; Index++) {
		if (pAd->TupleCache[Index].Valid == FALSE)
			continue;

		if (MAC_ADDR_EQUAL(pAd->TupleCache[Index].MacAddress, pHeader->Addr2)
				&& (pAd->TupleCache[Index].Sequence == pHeader->Sequence)
				&& (pAd->TupleCache[Index].Frag == pHeader->Frag)) {
			//			DBGPRINT(RT_DEBUG_TRACE,"DUPCHECK - duplicate frame hit entry %d\n", Index);
			return (TRUE);
		}
	}
	return (FALSE);
}

/*
 ========================================================================

 Routine	Description:
 Update tuple cache for new received unicast frames.

 Arguments:
 pAd				Pointer	to our adapter
 pHeader			802.11 header of receiving frame

 Return Value:
 None

 Note:

 ========================================================================
 */VOID RTMPUpdateTupleCache( IN PRTMP_ADAPTER pAd, IN PHEADER_802_11 pHeader) {
	UCHAR Index;

	for (Index = 0; Index < MAX_CLIENT; Index++) {
		if (pAd->TupleCache[Index].Valid == FALSE) {
			// Add new entry
			COPY_MAC_ADDR(pAd->TupleCache[Index].MacAddress, pHeader->Addr2);
			pAd->TupleCache[Index].Sequence = pHeader->Sequence;
			pAd->TupleCache[Index].Frag = pHeader->Frag;
			pAd->TupleCache[Index].Valid = TRUE;
			pAd->TupleCacheLastUpdateIndex = Index;
			DBGPRINT(RT_DEBUG_INFO,"DUPCHECK - Add Entry %d, MAC=%02x:%02x:%02x:%02x:%02x:%02x\n", Index,
					pAd->TupleCache[Index].MacAddress[0], pAd->TupleCache[Index].MacAddress[1],
					pAd->TupleCache[Index].MacAddress[2], pAd->TupleCache[Index].MacAddress[3],
					pAd->TupleCache[Index].MacAddress[4], pAd->TupleCache[Index].MacAddress[5]);
			return;
		} else if (MAC_ADDR_EQUAL(pAd->TupleCache[Index].MacAddress, pHeader->Addr2)) {
			// Update old entry
			pAd->TupleCache[Index].Sequence = pHeader->Sequence;
			pAd->TupleCache[Index].Frag = pHeader->Frag;
			return;
		}
	}

	// tuple cache full, replace the first inserted one (even though it may not be
	// least referenced one)
	if (Index == MAX_CLIENT) {
		pAd->TupleCacheLastUpdateIndex++;
		if (pAd->TupleCacheLastUpdateIndex >= MAX_CLIENT)
			pAd->TupleCacheLastUpdateIndex = 0;
		Index = pAd->TupleCacheLastUpdateIndex;

		// replace with new entry
		COPY_MAC_ADDR(pAd->TupleCache[Index].MacAddress, pHeader->Addr2);
		pAd->TupleCache[Index].Sequence = pHeader->Sequence;
		pAd->TupleCache[Index].Frag = pHeader->Frag;
		pAd->TupleCache[Index].Valid = TRUE;
		DBGPRINT(RT_DEBUG_INFO,"DUPCHECK - replace Entry %d, MAC=%02x:%02x:%02x:%02x:%02x:%02x\n", Index,
				pAd->TupleCache[Index].MacAddress[0], pAd->TupleCache[Index].MacAddress[1],
				pAd->TupleCache[Index].MacAddress[2], pAd->TupleCache[Index].MacAddress[3],
				pAd->TupleCache[Index].MacAddress[4], pAd->TupleCache[Index].MacAddress[5]);
	}
}

/*
 ========================================================================

 Routine	Description:
 Apply packet filter policy, return NDIS_STATUS_FAILURE if this frame
 should be dropped.

 Arguments:
 pAd		Pointer	to our adapter
 pRxD			Pointer	to the Rx descriptor
 pHeader			Pointer to the 802.11 frame header

 Return Value:
 NDIS_STATUS_SUCCESS		Accept frame
 NDIS_STATUS_FAILURE		Drop Frame

 Note:
 Maganement frame should bypass this filtering rule.

 ========================================================================
 */NDIS_STATUS RTMPApplyPacketFilter( IN PRTMP_ADAPTER pAd, IN PRXD_STRUC pRxD,
		IN PHEADER_802_11 pHeader) {
	UCHAR i;

	// 0. Management frame should bypass all these filtering rules.
	if (pHeader->FC.Type == BTYPE_MGMT)
		return (NDIS_STATUS_SUCCESS);

	// 0.1	Drop all Rx frames if MIC countermeasures kicks in
	if (pAd->PortCfg.MicErrCnt >= 2) {
		DBGPRINT(RT_DEBUG_TRACE,"Rx dropped by MIC countermeasure\n");
		return (NDIS_STATUS_FAILURE);
	}

	// 1. Drop unicast to me packet if NDIS_PACKET_TYPE_DIRECTED is FALSE
	if (pRxD->U2M) {
		if (pAd->bAcceptDirect == FALSE) {
			DBGPRINT(RT_DEBUG_TRACE,"Rx U2M dropped by RX_FILTER\n");
			return (NDIS_STATUS_FAILURE);
		}
	}

	// 2. Drop broadcast packet if NDIS_PACKET_TYPE_BROADCAST is FALSE
	else if (pRxD->Bcast) {
		if (pAd->bAcceptBroadcast == FALSE) {
			DBGPRINT(RT_DEBUG_TRACE,"Rx BCAST dropped by RX_FILTER\n");
			return (NDIS_STATUS_FAILURE);
		}
	}

	// 3. Drop (non-Broadcast) multicast packet if NDIS_PACKET_TYPE_ALL_MULTICAST is false
	//	  and NDIS_PACKET_TYPE_MULTICAST is false.
	//	  If NDIS_PACKET_TYPE_MULTICAST is true, but NDIS_PACKET_TYPE_ALL_MULTICAST is false.
	//	  We have to deal with multicast table lookup & drop not matched packets.
	else if (pRxD->Mcast) {
		if (pAd->bAcceptAllMulticast == FALSE) {
			if (pAd->bAcceptMulticast == FALSE) {
				DBGPRINT(RT_DEBUG_INFO,"Rx MCAST dropped by RX_FILTER\n");
				return (NDIS_STATUS_FAILURE);
			} else {
				// Selected accept multicast packet based on multicast table
				for (i = 0; i < pAd->NumberOfMcastAddresses; i++) {
					if (MAC_ADDR_EQUAL(pHeader->Addr1, pAd->McastTable[i]))
						break; // Matched
				}

				// Not matched
				if (i == pAd->NumberOfMcastAddresses) {
					DBGPRINT(RT_DEBUG_INFO,"Rx MCAST %02x:%02x:%02x:%02x:%02x:%02x dropped by RX_FILTER\n",
							pHeader->Addr1[0], pHeader->Addr1[1], pHeader->Addr1[2],
							pHeader->Addr1[3], pHeader->Addr1[4], pHeader->Addr1[5]);
					return (NDIS_STATUS_FAILURE);
				} else {
					DBGPRINT(RT_DEBUG_INFO,"Accept multicast %02x:%02x:%02x:%02x:%02x:%02x\n",
							pHeader->Addr1[0], pHeader->Addr1[1], pHeader->Addr1[2],
							pHeader->Addr1[3], pHeader->Addr1[4], pHeader->Addr1[5]);
				}
			}
		}
	}

	// 4. Not U2M, not Mcast, not Bcast, must be unicast to other DA.
	//	  Since we did not implement promiscuous mode, just drop this kind of packet for now.
	else
		return (NDIS_STATUS_FAILURE);

	return (NDIS_STATUS_SUCCESS);
}

/*
 ========================================================================

 Routine	Description:
 Check Rx descriptor, return NDIS_STATUS_FAILURE if any error dound

 Arguments:
 pRxD		Pointer	to the Rx descriptor

 Return Value:
 NDIS_STATUS_SUCCESS		No err
 NDIS_STATUS_FAILURE		Error

 Note:

 ========================================================================
 */NDIS_STATUS RTMPCheckRxDescriptor( IN PRTMP_ADAPTER pAd,
		IN PHEADER_802_11 pHeader, IN PRXD_STRUC pRxD) {
	PCIPHER_KEY pWpaKey;

	// Phy errors & CRC errors
	if (/*(pRxD->PhyErr) ||*/(pRxD->Crc)) {
		DBGPRINT_RAW(RT_DEBUG_ERROR, "pRxD->Crc error\n")
		return (NDIS_STATUS_FAILURE);
	}

	// Drop ToDs promiscous frame, it is opened due to CCX 2 channel load statistics
	if (pHeader->FC.ToDs)
		return (NDIS_STATUS_FAILURE);

	/*
	 // Paul 04-03 for OFDM Rx length issue
	 if (pRxD->DataByteCnt > MAX_AGGREGATION_SIZE)
	 {
	 DBGPRINT_RAW(RT_DEBUG_ERROR, "received packet too long\n");
	 return (NDIS_STATUS_FAILURE);
	 }
	 */
	// do not support AGGREGATION
	if (pRxD->DataByteCnt > 1600) {
		//DBGPRINT_RAW(RT_DEBUG_ERROR, "received packet too long\n");
		return (NDIS_STATUS_FAILURE);
	}

	// Drop not U2M frames, cant's drop here because we will drop beacon in this case
	// I am kind of doubting the U2M bit operation
	// if (pRxD->U2M == 0)
	//	return(NDIS_STATUS_FAILURE);

	// drop decyption fail frame
	if (pRxD->CipherErr) {
		DBGPRINT_RAW(RT_DEBUG_TRACE,"ERROR: CRC ok but CipherErr %d (len = %d, Mcast=%d, Cipher=%s, KeyId=%d)\n",
				pRxD->CipherErr,
				pRxD->DataByteCnt,
				pRxD->Mcast | pRxD->Bcast,
				CipherName[pRxD->CipherAlg],
				pRxD->KeyIndex);

		//
		// MIC Error
		//
		if (pRxD->CipherErr == 2) {
			pWpaKey = &pAd->SharedKey[pRxD->KeyIndex];
			RTMPReportMicError(pAd, pWpaKey);
			DBGPRINT_RAW(RT_DEBUG_ERROR,"Rx MIC Value error\n");
		}

		if ((pRxD->CipherAlg == CIPHER_AES) && (pHeader->Sequence
				== pAd->FragFrame.Sequence)) {
			//
			// Acceptable since the First FragFrame no CipherErr problem.
			//
			return (NDIS_STATUS_FAILURE);
		}

		return (NDIS_STATUS_FAILURE);
	}

	return (NDIS_STATUS_SUCCESS);
}

#ifdef RALINK_WPA_SUPPLICANT_SUPPORT
static void ralink_michael_mic_failure(struct net_device *dev, PCIPHER_KEY pWpaKey)
{
	union iwreq_data wrqu;
	char buf[128];

	/* TODO: needed parameters: count, keyid, key type, TSC */

	//Check for Group or Pairwise MIC error
	if (pWpaKey->Type == PAIRWISE_KEY)
	sprintf(buf, "MLME-MICHAELMICFAILURE.indication unicast");
	else
	sprintf(buf, "MLME-MICHAELMICFAILURE.indication broadcast");
	memset(&wrqu, 0, sizeof(wrqu));
	wrqu.data.length = strlen(buf);
	//send mic error event to wpa_supplicant
	wireless_send_event(dev, IWEVCUSTOM, &wrqu, buf);
}
#endif

/*
 ========================================================================

 Routine	Description:
 Process MIC error indication and record MIC error timer.

 Arguments:
 pAd		Pointer	to our adapter
 pWpaKey			Pointer	to the WPA key structure

 Return Value:
 None

 Note:

 ========================================================================
 */VOID RTMPReportMicError( IN PRTMP_ADAPTER pAd, IN PCIPHER_KEY pWpaKey) {
	ULONG Now;
	UCHAR unicastKey = (pWpaKey->Type == PAIRWISE_KEY ? 1 : 0); //BensonLiu 07-11-22 add for countermeature
	struct {
		NDIS_802_11_STATUS_INDICATION Status;
		NDIS_802_11_AUTHENTICATION_REQUEST Request;
	} Report;

	DBGPRINT_RAW(RT_DEBUG_TRACE,"@@@@@@@@@@@@@ snowcarey mic error 1 @@@@@@@@@@@@@@@\n");
#ifdef RALINK_WPA_SUPPLICANT_SUPPORT
	if (pAd->PortCfg.WPA_Supplicant == TRUE) {
		//report mic error to wpa_supplicant
		ralink_michael_mic_failure(pAd->net_dev, pWpaKey);
	}
#endif

	// 0. Set Status to indicate auth error
	Report.Status.StatusType = Ndis802_11StatusType_Authentication;

	// 1. Check for Group or Pairwise MIC error
	if (pWpaKey->Type == PAIRWISE_KEY)
		Report.Request.Flags = NDIS_802_11_AUTH_REQUEST_PAIRWISE_ERROR;
	else
		Report.Request.Flags = NDIS_802_11_AUTH_REQUEST_GROUP_ERROR;

	// 2. Copy AP MAC address
	COPY_MAC_ADDR(Report.Request.Bssid, pWpaKey->BssId);

	// 3. Calculate length
	Report.Request.Length = sizeof(NDIS_802_11_AUTHENTICATION_REQUEST);

	// 4. Record Last MIC error time and count
	Now = jiffies;
	if (pAd->PortCfg.MicErrCnt == 0) {
		pAd->PortCfg.MicErrCnt++;
		pAd->PortCfg.LastMicErrorTime = Now;
	} else if (pAd->PortCfg.MicErrCnt == 1) {
		//if ((pAd->PortCfg.LastMicErrorTime + (60 * 1000)) > Now)
		if ((pAd->PortCfg.LastMicErrorTime + (60 * HZ)) < Now) //BensonLiu 07-11-22 add for countermeature
		{
			// Update Last MIC error time, this did not violate two MIC errors within 60 seconds
			pAd->PortCfg.LastMicErrorTime = Now;
		} else {
			pAd->PortCfg.LastMicErrorTime = Now;
			// Violate MIC error counts, MIC countermeasures kicks in
			pAd->PortCfg.MicErrCnt++;
			// We shall block all reception
			// We shall clean all Tx ring and disassoicate from AP after next EAPOL frame
			RTUSBRejectPendingPackets(pAd);
			RTUSBCleanUpDataBulkOutQueue(pAd);
		}
	} else {
		// MIC error count >= 2
		// This should not happen
		;
	}

	//BensonLiu 07-11-22 start for countermeature
	DBGPRINT_RAW(RT_DEBUG_TRACE,"@@@@@@@@@@@@@ snowcarey mic error 1 @@@@@@@@@@@@@@@\n");
	MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE,
			OID_802_11_MIC_FAILURE_REPORT_FRAME, 2, &unicastKey);
	RTUSBMlmeUp(pAd);

	if (pAd->PortCfg.MicErrCnt == 2)
		RTMPSetTimer(&pAd->PortCfg.WpaDisassocAndBlockAssocTimer, 100);
	//BensonLiu 07-11-22 end for countermeature
}

/* 
 ==========================================================================
 Description:
 Send out a NULL frame to AP. The prpose is to inform AP this client
 current PSM bit.
 NOTE:
 This routine should only be used in infrastructure mode.
 ==========================================================================
 */VOID RTMPSendNullFrame( IN PRTMP_ADAPTER pAd, IN UCHAR TxRate) {
	PTX_CONTEXT pNullContext;
	PTXD_STRUC pTxD;
#ifdef BIG_ENDIAN
	PTXD_STRUC pDestTxD;
	TXD_STRUC TxD;
#endif
	UCHAR QueIdx = QID_AC_VO;
	PHEADER_802_11 pHdr80211;
	ULONG TransferBufferLength;

#ifdef RALINK_ATE
	if(pAd->ate.Mode != ATE_STASTART)
	{
		return;
	}
#endif

	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED)) {
		QueIdx = QID_AC_VO;
	} else {
		QueIdx = QID_AC_BE;
	}

	if ((RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS))
			|| (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BULKOUT_RESET))
			|| (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS))
			|| (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))) {
		return;
	}

	// WPA 802.1x secured port control
	if (((pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA)
			|| (pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPAPSK)
			|| (pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA2)
			|| (pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA2PSK)
#ifdef RALINK_WPA_SUPPLICANT_SUPPORT
       || (pAd->PortCfg.IEEE8021X == TRUE)		
#endif 		
       )
			&& (pAd->PortCfg.PortSecured == WPA_802_1X_PORT_NOT_SECURED)) {
		return;
	}

	pNullContext = &(pAd->NullContext);
	if (pNullContext->InUse == FALSE) {
		// Set the in use bit
		pNullContext->InUse = TRUE;

		// Fill Null frame body and TxD
#ifndef BIG_ENDIAN
		pTxD = (PTXD_STRUC) &pNullContext->TransferBuffer->TxDesc;
#else
		pDestTxD = (PTXD_STRUC) &pNullContext->TransferBuffer->TxDesc;
		TxD = *pDestTxD;
		pTxD = &TxD;
		RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
#endif
		NdisZeroMemory(pTxD, sizeof(TXD_STRUC));

		pHdr80211
				= (PHEADER_802_11) &pAd->NullContext.TransferBuffer->u.NullFrame;
		MgtMacHeaderInit(pAd, pHdr80211, SUBTYPE_NULL_FUNC, 1,
				pAd->PortCfg.Bssid, pAd->PortCfg.Bssid);
		pHdr80211->Duration = RTMPCalcDuration(pAd, TxRate, 14);
		pHdr80211->FC.Type = BTYPE_DATA;
		pHdr80211->FC.PwrMgmt = (pAd->PortCfg.Psm == PWR_SAVE);

#ifdef BIG_ENDIAN
		RTMPFrameEndianChange(pAd, (PUCHAR)pHdr80211, DIR_WRITE, FALSE);
		RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
		*pDestTxD = TxD;
		pTxD = pDestTxD;
#endif
		RTUSBWriteTxDescriptor(pAd, pTxD, CIPHER_NONE, 0, 0, FALSE, FALSE,
				FALSE, SHORT_RETRY, IFS_BACKOFF, TxRate, sizeof(HEADER_802_11),
				QueIdx, PID_MGMT_FRAME, FALSE);

		DBGPRINT(RT_DEBUG_TRACE, "SYNC - send NULL Frame @%d Mbps...\n", RateIdToMbps[TxRate]);
	}

	// Build our URB for USBD
	TransferBufferLength = sizeof(TXD_STRUC) + sizeof(HEADER_802_11);
	if ((TransferBufferLength % 2) == 1)
		TransferBufferLength++;
	if ((TransferBufferLength % pAd->BulkOutMaxPacketSize) == 0)
		TransferBufferLength += 2;

	// Fill out frame length information for global Bulk out arbitor
	pNullContext->BulkOutSize = TransferBufferLength;
	RTUSB_SET_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_NULL);

	// Kick bulk out
	RTUSBKickBulkOut(pAd);
}

VOID RTMPSendRTSCTSFrame( IN PRTMP_ADAPTER pAd, IN PUCHAR pDA,
		IN ULONG NextMpduSize, IN UCHAR TxRate, IN UCHAR RTSRate,
		IN USHORT AckDuration, IN UCHAR QueIdx, IN UCHAR FrameGap, IN UCHAR Type) {
	PTX_CONTEXT pTxContext;
	PTXD_STRUC pTxD;
#ifdef BIG_ENDIAN
	PTXD_STRUC pDestTxD;
	TXD_STRUC TxD;
#endif	
	PRTS_FRAME pRtsFrame;
	PUCHAR pBuf;
	ULONG Length = 0;
	ULONG TransferBufferLength = 0;
	unsigned long IrqFlags;//BensonLiu modify

	if ((Type != SUBTYPE_RTS) && (Type != SUBTYPE_CTS)) {
		DBGPRINT(RT_DEBUG_WARN, "Making RTS/CTS Frame failed, type not matched!\n");
		return;
	} else if ((Type == SUBTYPE_RTS) && ((*pDA) & 0x01)) {
		if ((*pDA) & 0x01) {
			// should not use RTS/CTS to protect MCAST frame since no one will reply CTS
			DBGPRINT(RT_DEBUG_INFO,"Not use RTS Frame to proect MCAST frame\n");
			return;
		}
	}

	NdisAcquireSpinLock(&pAd->TxRingLock, IrqFlags);//BensonLiu modify

	pTxContext = &pAd->TxContext[QueIdx][pAd->NextTxIndex[QueIdx]];
	if (pTxContext->InUse == FALSE) {
		pTxContext->InUse = TRUE;
		pTxContext->LastOne = FALSE;
		pAd->NextTxIndex[QueIdx]++;
		if (pAd->NextTxIndex[QueIdx] >= TX_RING_SIZE) {
			pAd->NextTxIndex[QueIdx] = 0;
		}

#ifndef BIG_ENDIAN
		pTxD = (PTXD_STRUC) &pTxContext->TransferBuffer->TxDesc;
#else
		pDestTxD = (PTXD_STRUC) &pTxContext->TransferBuffer->TxDesc;
		TxD = *pDestTxD;
		pTxD = &TxD;
		RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
#endif

		pRtsFrame = (PRTS_FRAME) &pTxContext->TransferBuffer->u.RTSFrame;
		pBuf = (PUCHAR) pRtsFrame;

		NdisZeroMemory(pRtsFrame, sizeof(RTS_FRAME));
		pRtsFrame->FC.Type = BTYPE_CNTL;
		// CTS-to-self's duration = SIFS + MPDU
		pRtsFrame->Duration = (2 * pAd->PortCfg.Dsifs) + RTMPCalcDuration(pAd,
				TxRate, NextMpduSize) + AckDuration;// SIFS + Data + SIFS + ACK

		// Write Tx descriptor
		// Don't kick tx start until all frames are prepared
		// RTS has to set more fragment bit for fragment burst
		// RTS did not encrypt
		if (Type == SUBTYPE_RTS) {
			DBGPRINT(RT_DEBUG_INFO,"Making RTS Frame\n");

			pRtsFrame->FC.SubType = SUBTYPE_RTS;
			COPY_MAC_ADDR(pRtsFrame->Addr1, pDA);
			COPY_MAC_ADDR(pRtsFrame->Addr2, pAd->CurrentAddress);

			// RTS's duration need to include and extra (SIFS + CTS) time
			pRtsFrame->Duration += (pAd->PortCfg.Dsifs + RTMPCalcDuration(pAd,
					RTSRate, 14)); // SIFS + CTS-Duration

			rts_cts_frame_duration = pRtsFrame->Duration;

			Length = sizeof(RTS_FRAME);

#ifdef BIG_ENDIAN
			RTMPFrameEndianChange(pAd, (PUCHAR)pRtsFrame, DIR_WRITE, FALSE);
			RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
			*pDestTxD = TxD;
			pTxD = pDestTxD;
#endif
			RTUSBWriteTxDescriptor(pAd, pTxD, CIPHER_NONE, 0, 0, TRUE, TRUE,
					FALSE, SHORT_RETRY, FrameGap, RTSRate, Length, QueIdx, 0,
					FALSE);
		} else if (Type == SUBTYPE_CTS) {
			DBGPRINT(RT_DEBUG_INFO,"Making CTS-to-self Frame\n");
			pRtsFrame->FC.SubType = SUBTYPE_CTS;
			COPY_MAC_ADDR(pRtsFrame->Addr1, pAd->CurrentAddress);

			Length = 10; //CTS frame length.

#ifdef BIG_ENDIAN
			RTMPFrameEndianChange(pAd, (PUCHAR)pRtsFrame, DIR_WRITE, FALSE);
			RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
			*pDestTxD = TxD;
			pTxD = pDestTxD;
#endif			
			RTUSBWriteTxDescriptor(pAd, pTxD, CIPHER_NONE, 0, 0, FALSE, TRUE,
					FALSE, SHORT_RETRY, FrameGap, RTSRate, Length, QueIdx, 0,
					FALSE);
		}

		// Build our URB for USBD
		TransferBufferLength = sizeof(TXD_STRUC) + Length;
		if ((TransferBufferLength % 2) == 1)
			TransferBufferLength++;
		if ((TransferBufferLength % pAd->BulkOutMaxPacketSize) == 0)
			TransferBufferLength += 2;

		// Fill out frame length information for global Bulk out arbitor
		pTxContext->BulkOutSize = TransferBufferLength;
		pTxContext->bWaitingBulkOut = TRUE;

		pAd->TxRingTotalNumber[QueIdx]++; // sync. to TxCount
		pTxContext->LastOne = TRUE;
		atomic_inc(&pAd->TxCount);
		NdisReleaseSpinLock(&pAd->TxRingLock, IrqFlags);//BensonLiu modify
		RTUSB_SET_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_NORMAL << QueIdx);
	}
}

/*
 ========================================================================

 Routine	Description:
 Calculates the duration which is required to transmit out frames
 with given size and specified rate.

 Arguments:
 pAd				Pointer	to our adapter
 Rate			Transmit rate
 Size			Frame size in units of byte

 Return Value:
 Duration number in units of usec

 Note:

 ========================================================================
 */USHORT RTMPCalcDuration( IN PRTMP_ADAPTER pAd, IN UCHAR Rate, IN ULONG Size) {
	ULONG Duration = 0;

	if (Rate < RATE_FIRST_OFDM_RATE) // CCK
	{
		if ((Rate > RATE_1)
				&& (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_SHORT_PREAMBLE_INUSED)))
			Duration = 96; // 72+24 preamble+plcp
		else
			Duration = 192; // 144+48 preamble+plcp

		Duration += (USHORT)((Size << 4) / RateIdTo500Kbps[Rate]);
		if ((Size << 4) % RateIdTo500Kbps[Rate])
			Duration++;
	} else // OFDM rates
	{
		Duration = 20 + 6; // 16+4 preamble+plcp + Signal Extension
		Duration += 4 * (USHORT)((11 + Size * 4) / RateIdTo500Kbps[Rate]);
		if ((11 + Size * 4) % RateIdTo500Kbps[Rate])
			Duration += 4;
	}

	return (USHORT) Duration;

}

/*
 ========================================================================

 Routine	Description:
 Check the out going frame, if this is an DHCP or ARP datagram
 will be duplicate another frame at low data rate transmit.

 Arguments:
 pAd			Pointer	to our adapter
 pSkb		Pointer to outgoing skb buffer

 Return Value:
 TRUE		To be transmitted at Low data rate transmit. (1Mbps/6Mbps)
 FALSE		Do nothing.

 Note:

 MAC header + IP Header + UDP Header
 14 Bytes	  20 Bytes

 UDP Header
 00|01|02|03|04|05|06|07|08|09|10|11|12|13|14|15|
 Source Port
 16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|
 Destination Port

 port 0x43 means Bootstrap Protocol, server.
 Port 0x44 means Bootstrap Protocol, client.

 ========================================================================
 */BOOLEAN RTMPCheckDHCPFrame( IN PRTMP_ADAPTER pAd, IN struct sk_buff *pSkb) {
	PUCHAR pSrc;
	ULONG SrcLen = 0;

	pSrc = (PVOID) pSkb->data;
	SrcLen = pSkb->len;

	// Check ARP packet
	if (SrcLen >= 13) {
		if ((pSrc[12] == 0x08) && (pSrc[13] == 0x06)) {
			DBGPRINT(RT_DEBUG_INFO,"RTMPCheckDHCPFrame - ARP packet\n");
			return TRUE;
		}
	}

	// Check foe DHCP & BOOTP protocol
	if (SrcLen >= 37) {
		//if ((pSrc[35] == 0x43) && (pSrc[37] == 0x44))
		if ((pSrc[35] == 0x44) && (pSrc[37] == 0x43))//[Richie20090714]DHCP Tx using high data rate
		{
			DBGPRINT(RT_DEBUG_INFO,"RTMPCheckDHCPFrame - DHCP packet\n");
			return TRUE;
		}
	}

	return FALSE;
}

#ifdef RALINK_ATE
//Setup Null Frame format.
VOID ATE_RTMPSendNullFrame(
		IN PRTMP_ADAPTER pAd)
{
	PTXD_STRUC pTxD;
	HEADER_802_11 Header_802_11;

	PTX_CONTEXT pNullContext;
	UCHAR QueIdx =QID_AC_VO;
	ULONG TransferBufferLength;
	//unsigned long	IrqFlags;

	QueIdx =QID_AC_BE;

	pNullContext = &(pAd->NullContext);
	if (pNullContext->InUse == FALSE)
	{
		// Set the in use bit
		pNullContext->InUse = TRUE;

		//Create Tx packet
		NdisZeroMemory(&Header_802_11, sizeof(HEADER_802_11));
		NdisMoveMemory(&Header_802_11.Addr1, pAd->ate.Addr1, ETH_LENGTH_OF_ADDRESS);
		NdisMoveMemory(&Header_802_11.Addr2, pAd->ate.Addr2, ETH_LENGTH_OF_ADDRESS);
		NdisMoveMemory(&Header_802_11.Addr3, pAd->ate.Addr3, ETH_LENGTH_OF_ADDRESS);
		Header_802_11.Sequence = pAd->Sequence;
		Header_802_11.FC.Type = BTYPE_DATA;
		Header_802_11.FC.PwrMgmt = 0;
		Header_802_11.Frag = 0;
		Header_802_11.FC.MoreFrag = 0;
		Header_802_11.Duration = 0;

		pTxD = (PTXD_STRUC) &pNullContext->TransferBuffer->TxDesc;
		NdisZeroMemory(pTxD, sizeof(PTXD_STRUC));
		NdisMoveMemory(&pNullContext->TransferBuffer->u.NullFrame, &Header_802_11, sizeof(Header_802_11));

		//Note
		RTUSBWriteTxDescriptor(pAd, pTxD, CIPHER_NONE, 0,0, FALSE, FALSE, FALSE, SHORT_RETRY, IFS_BACKOFF, (UCHAR)pAd->ate.TxRate, pAd->ate.TxLength, QID_AC_BE, PID_NULL_AT_HIGH_RATE, FALSE);
	}

	// Build our URB for USBD
	TransferBufferLength = (pAd->ate.TxLength-(sizeof(TXD_STRUC) + sizeof(HEADER_802_11))+(sizeof(TXD_STRUC) + sizeof(HEADER_802_11)));

	if ((TransferBufferLength % 4) == 1)
	TransferBufferLength += 3;
	else if ((TransferBufferLength % 4) == 2)
	TransferBufferLength += 2;
	else if ((TransferBufferLength % 4) == 3)
	TransferBufferLength += 1;

	if ((TransferBufferLength % pAd->BulkOutMaxPacketSize) == 0)
	TransferBufferLength += 4;

	// Fill out frame length information for global Bulk out arbitor
	pNullContext->BulkOutSize = TransferBufferLength;

}
#endif
