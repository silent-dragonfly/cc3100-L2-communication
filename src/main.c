#include <simplelink.h>

#include "helpers.h"
#include "sl_common.h"



#include "ieee80211.h"
#include "windows.h"

#define CLI_Write(X) DEBUG(X)
#define Delay(X) Sleep(X)
#define MAX_PACKET_SIZE (1472)





static _i32 main_pinger();
static _i32 main_ponger();






#define APPLICATION_VERSION "1.3.0"

#define SL_STOP_TIMEOUT        0xFF

/* Power level tone valid range 0-15 */
#define POWER_LEVEL_TONE    1
/* Preamble value 0- short, 1- long */
#define PREAMBLE            1

#define RATE                RATE_11M

/* Channel (1-13) used during the tx and rx*/
#define CHANNEL             10

#define BUF_SIZE 1400
#define NO_OF_PACKETS 100

/* Application specific status/error codes */
typedef enum{
    DEVICE_NOT_IN_STATION_MODE = -0x7D0,        /* Choosing this number to avoid overlap w/ host-driver's error codes */

    STATUS_CODE_MAX = -0xBB8
}e_AppStatusCodes;

/*
 * GLOBAL VARIABLES -- Start
 */
_u8 g_Status = 0;
const int END_COUNT = 100;
const int TRY_RECIVE_MAX = 100;
const ROLE_OFFSET = 25;
const COUNTER_OFFSET = 26;

DataQoSFrame_FromDSToSTA FRAME = {
   		.FrameControl = (struct FrameControl){
   				.Type = TYPE_DATA,
   				.Subtype = DATA_SUBTYPE_QOS,
   				.FromDS = 1
   		},
   		.Duration = 0x002C,
   	    .DestinationMAC = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA},
   	    .BSSIDMAC = {0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB},
   		.SourceMAC = {0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB},
   		.SequenceControl = (struct SequenceControl){
   			.FragmentNumber = 0,
   			.SequenceNumber = 0x428,
   		},
   		.QoSControl = (struct QoSControl){0},
		.Data = {
				/* LLC */
			   0xAA, // DSAP
			   0xAA, // SSAP
			   0x03, // Control field
			   0x00, 0x00, 0x00, // Organization Code: Encapsulated Ethernet
			   0x12, 0x34,},
   };

uint8_t BUFFER[MAX_PACKET_SIZE];

/*
 * STATIC FUNCTION DEFINITIONS -- Start
 */
static _i32 configureSimpleLinkToDefaultState();
static _i32 initializeAppVariables();
static _i32 RxEvaluation(_i16 channel);
/*
 * STATIC FUNCTION DEFINITIONS -- End
 */

/*
 * ASYNCHRONOUS EVENT HANDLERS -- Start
 */
/*!
    \brief This function handles WLAN events

    \param[in]      pWlanEvent is the event passed to the handler

    \return         None

    \note

    \warning
*/
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent)
{
    if(pWlanEvent == NULL)
    {
        CLI_Write(" [WLAN EVENT] NULL Pointer Error \n\r");
        return;
    }

    switch(pWlanEvent->Event)
    {
        case SL_WLAN_CONNECT_EVENT:
        {
            SET_STATUS_BIT(g_Status, STATUS_BIT_CONNECTION);

            /*
             * Information about the connected AP (like name, MAC etc) will be
             * available in 'slWlanConnectAsyncResponse_t' - Applications
             * can use it if required
             *
             * slWlanConnectAsyncResponse_t *pEventData = NULL;
             * pEventData = &pWlanEvent->EventData.STAandP2PModeWlanConnected;
             *
             */
        }
        break;

        case SL_WLAN_DISCONNECT_EVENT:
        {
            slWlanConnectAsyncResponse_t*  pEventData = NULL;

            CLR_STATUS_BIT(g_Status, STATUS_BIT_CONNECTION);

            pEventData = &pWlanEvent->EventData.STAandP2PModeDisconnected;

            /* If the user has initiated 'Disconnect' request, 'reason_code' is
             * SL_USER_INITIATED_DISCONNECTION */
            if(SL_WLAN_DISCONNECT_USER_INITIATED_DISCONNECTION == pEventData->reason_code)
            {
                CLI_Write(" Device disconnected from the AP on application's request \n\r");
            }
            else
            {
                CLI_Write(" Device disconnected from the AP on an ERROR..!! \n\r");
            }
        }
        break;

        default:
        {
            CLI_Write(" [WLAN EVENT] Unexpected event \n\r");
        }
        break;
    }
}

/*!
    \brief This function handles events for IP address acquisition via DHCP
           indication

    \param[in]      pNetAppEvent is the event passed to the handler

    \return         None

    \note

    \warning
*/
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{
    if(pNetAppEvent == NULL)
    {
        CLI_Write(" [NETAPP EVENT] NULL Pointer Error \n\r");
        return;
    }

    switch(pNetAppEvent->Event)
    {
        case SL_NETAPP_IPV4_IPACQUIRED_EVENT:
        {
            SET_STATUS_BIT(g_Status, STATUS_BIT_IP_ACQUIRED);

            /*
             * Information about the connection (like IP, gateway address etc)
             * will be available in 'SlIpV4AcquiredAsync_t'
             * Applications can use it if required
             *
             * SlIpV4AcquiredAsync_t *pEventData = NULL;
             * pEventData = &pNetAppEvent->EventData.ipAcquiredV4;
             *
             */
        }
        break;

        default:
        {
            CLI_Write(" [NETAPP EVENT] Unexpected event \n\r");
        }
        break;
    }
}

/*!
    \brief This function handles callback for the HTTP server events

    \param[in]      pHttpEvent - Contains the relevant event information
    \param[in]      pHttpResponse - Should be filled by the user with the
                    relevant response information

    \return         None

    \note

    \warning
*/
void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pHttpEvent,
                                  SlHttpServerResponse_t *pHttpResponse)
{
    /* Unused in this application */
    CLI_Write(" [HTTP EVENT] Unexpected event \n\r");
}

/*!
    \brief This function handles general error events indication

    \param[in]      pDevEvent is the event passed to the handler

    \return         None
*/
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
    /*
     * Most of the general errors are not FATAL are are to be handled
     * appropriately by the application
     */
    CLI_Write(" [GENERAL EVENT] \n\r");
}

/*!
    \brief This function handles socket events indication

    \param[in]      pSock is the event passed to the handler

    \return         None
*/
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
    if(pSock == NULL)
    {
        CLI_Write(" [SOCK EVENT] NULL Pointer Error \n\r");
        return;
    }

    switch( pSock->Event )
    {
        case SL_SOCKET_TX_FAILED_EVENT:
            /*
             * TX Failed
             *
             * Information about the socket descriptor and status will be
             * available in 'SlSockEventData_t' - Applications can use it if
             * required
             *
            * SlSockEventData_u *pEventData = NULL;
            * pEventData = & pSock->socketAsyncEvent;
             */
            switch( pSock->socketAsyncEvent.SockTxFailData.status )
            {
                case SL_ECLOSE:
                    CLI_Write(" [SOCK EVENT] Close socket operation failed to transmit all queued packets\n\r");
                    break;
                default:
                    CLI_Write(" [SOCK EVENT] Unexpected event \n\r");
                    break;
            }
            break;

        default:
            CLI_Write(" [SOCK EVENT] Unexpected event \n\r");
            break;
    }
}
/*
 * ASYNCHRONOUS EVENT HANDLERS -- End
 */


typedef enum AppRole {
	APP_PINGER = 0x55,
	APP_PONGER = 0xDD
} AppRole;

int main(int argc, char** argv)
{
	AppRole role;

 	if (argc != 2) {
		DEBUG("USAGE: cc3100-L2-communication [PINGER|PONGER]");
		return -1;
	}

	if (strcmp(argv[1], "PINGER") == 0) {
		role = APP_PINGER;
	} else if (strcmp(argv[1], "PONGER") == 0) {
		role = APP_PONGER;
	} else {
		DEBUG("[ERROR] Wrong role: %s", argv[1]);
		return -1;
	}

    _i32 retVal = -1;

    /*
     * Following function configures the device to default state by cleaning
     * the persistent settings stored in NVMEM (viz. connection profiles &
     * policies, power policy etc)
     *
     * Applications may choose to skip this step if the developer is sure
     * that the device is in its default state at start of application
     *
     * Note that all profiles and persistent settings that were done on the
     * device will be lost
     */
    retVal = configureSimpleLinkToDefaultState();
    if(retVal < 0)
    {
        DEBUG(" Failed to configure the device in its default state");

        LOOP_FOREVER();
    }
    DEBUG(" Device is configured in default state");

    retVal = initializeAppVariables();
    ASSERT_ON_ERROR(retVal);

    /*
     * Assumption is that the device is configured in station mode already
     * and it is in its default state
     */
    /* Initializing the CC3100 device */
    retVal = sl_Start(0, 0, 0);
    if ((retVal < 0) || (ROLE_STA != retVal) )
    {
        DEBUG(" Failed to start the device");
        LOOP_FOREVER();
    }
    DEBUG(" Device started as STATION");

    DEBUG("Start L1 Ping-Pong");

    if (role == APP_PINGER) {
    	retVal = main_pinger();
    } else if (role == APP_PONGER) {
    	retVal = main_ponger();
    } else {
    	assert(0);
    }

    if(retVal < 0) {
    	DEBUG("[ERROR] Bad exit from main function");
    }

    /* Stop the CC3100 device */
    retVal = sl_Stop(SL_STOP_TIMEOUT);
    if(retVal < 0)
        LOOP_FOREVER();

    return 0;
}

static _i32 configureSimpleLinkToDefaultState()
{
    SlVersionFull   ver = {0};
    _WlanRxFilterOperationCommandBuff_t  RxFilterIdMask = {0};

    _u8           val = 1;
    _u8           configOpt = 0;
    _u8           configLen = 0;
    _u8           power = 0;

    _i32          retVal = -1;
    _i32          mode = -1;

    mode = sl_Start(0, 0, 0);
    ASSERT_ON_ERROR(mode);

    /* If the device is not in station-mode, try configuring it in station-mode */
    if (ROLE_STA != mode)
    {
        if (ROLE_AP == mode)
        {
            /* If the device is in AP mode, we need to wait for this event before doing anything */
            while(!IS_IP_ACQUIRED(g_Status)) { Delay(100); }
        }

        /* Switch to STA role and restart */
        retVal = sl_WlanSetMode(ROLE_STA);
        ASSERT_ON_ERROR(retVal);

        retVal = sl_Stop(SL_STOP_TIMEOUT);
        ASSERT_ON_ERROR(retVal);

        retVal = sl_Start(0, 0, 0);
        ASSERT_ON_ERROR(retVal);

        /* Check if the device is in station again */
        if (ROLE_STA != retVal)
        {
            /* We don't want to proceed if the device is not coming up in station-mode */
            ASSERT_ON_ERROR(DEVICE_NOT_IN_STATION_MODE);
        }
    }

    /* Get the device's version-information */
    configOpt = SL_DEVICE_GENERAL_VERSION;
    configLen = sizeof(ver);
    retVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &configOpt, &configLen, (_u8 *)(&ver));
    ASSERT_ON_ERROR(retVal);

    /* Set connection policy to Auto + SmartConfig (Device's default connection policy) */
    retVal = sl_WlanPolicySet(SL_POLICY_CONNECTION, SL_CONNECTION_POLICY(1, 0, 0, 0, 1), NULL, 0);
    ASSERT_ON_ERROR(retVal);

    /* Remove all profiles */
    retVal = sl_WlanProfileDel(0xFF);
    ASSERT_ON_ERROR(retVal);

    /*
     * Device in station-mode. Disconnect previous connection if any
     * The function returns 0 if 'Disconnected done', negative number if already disconnected
     * Wait for 'disconnection' event if 0 is returned, Ignore other return-codes
     */
    retVal = sl_WlanDisconnect();
    if(0 == retVal)
    {
        /* Wait */
        while(IS_CONNECTED(g_Status)) { Delay(100); }
    }

    /* Enable DHCP client*/
    retVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE,1,1,&val);
    ASSERT_ON_ERROR(retVal);

    /* Disable scan */
    configOpt = SL_SCAN_POLICY(0);
    retVal = sl_WlanPolicySet(SL_POLICY_SCAN , configOpt, NULL, 0);
    ASSERT_ON_ERROR(retVal);

    /* Set Tx power level for station mode
       Number between 0-15, as dB offset from max power - 0 will set maximum power */
    power = 0;
    retVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, (_u8 *)&power);
    ASSERT_ON_ERROR(retVal);

    /* Set PM policy to normal */
    retVal = sl_WlanPolicySet(SL_POLICY_PM , SL_NORMAL_POLICY, NULL, 0);
    ASSERT_ON_ERROR(retVal);

    /* Unregister mDNS services */
    retVal = sl_NetAppMDNSUnRegisterService(0, 0);
    ASSERT_ON_ERROR(retVal);

    /* Remove  all 64 filters (8*8) */
    pal_Memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
    retVal = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *)&RxFilterIdMask,
                       sizeof(_WlanRxFilterOperationCommandBuff_t));
    ASSERT_ON_ERROR(retVal);

    retVal = sl_Stop(SL_STOP_TIMEOUT);
    ASSERT_ON_ERROR(retVal);

    return retVal; /* Success */
}

/*!
    \brief Entering raw Transmitter\Receiver mode in order to send raw data
           over the WLAN PHY

    This function shows how to send raw data, in this case ping packets over
    the air in transmitter mode.

    \param[in]      Channel number on which the data will be sent

    \return         0 on success, Negative on Error.

    \note

    \warning        We must be disconnected from WLAN AP in order to succeed
                    changing to transmitter mode
*/

static _i32 main_pinger()
{
	_i16 SockID = sl_Socket(SL_AF_RF, SL_SOCK_RAW, CHANNEL);
	ASSERT_ON_ERROR(SockID);

	_i32 Status = 0;

	for (int counter = 0;  counter < END_COUNT; ) {

		FRAME.Data[ROLE_OFFSET] = APP_PINGER;
		FRAME.Data[COUNTER_OFFSET] = counter;

		Status = sl_Send(SockID, (_u8*)&FRAME, sizeof(FRAME),
		                 SL_RAW_RF_TX_PARAMS(CHANNEL, RATE, POWER_LEVEL_TONE, PREAMBLE));
		if(Status < 0)
		{
			DEBUG("[ERROR] while sending packet");
			sl_Close(SockID);
			return -1;
		}

		DEBUG("[PINGER] send %d", counter);

		BOOLEAN RESP_RECEIVED = FALSE;
		for (int rx_counter = 0; rx_counter < TRY_RECIVE_MAX; rx_counter++) {

			Status = sl_Recv(SockID, BUFFER, MAX_PACKET_SIZE, 0);
			SlTransceiverRxOverHead_t *rxHeader = BUFFER;
			DataQoSFrame_FromDSToSTA *recv_fr = (DataQoSFrame_FromDSToSTA*)(BUFFER + sizeof(SlTransceiverRxOverHead_t));
			FrameControl *fc = &recv_fr->FrameControl;

			if(Status < 0)
			{
				DEBUG("[ERROR] while receiving packet");
				sl_Close(SockID);
				return -1;
			}

			if (fc->ProtocolVersion != 0) {
				DEBUG("[MAGIC] Protocol version is not a zero!!!!!!!!");
				continue;
			}

			if (fc->Type != TYPE_DATA) {
				continue;
			}

			if (fc->Subtype != DATA_SUBTYPE_QOS) {
				continue;
			}

			if (memcmp(FRAME.DestinationMAC, recv_fr->DestinationMAC, 6) != 0) {
				continue;
			}

			if (recv_fr->Data[ROLE_OFFSET] != APP_PONGER) {
				DEBUG("[PINGER] Unexpected role of sender: %d", recv_fr->Data[ROLE_OFFSET]);
				continue;
			}

			if (recv_fr->Data[COUNTER_OFFSET] != counter) {
				DEBUG("[PINGER] Unexpected counter in response: %d", recv_fr->Data[COUNTER_OFFSET]);
				continue;
			}

			DEBUG("[%lu]Recv: %d | %d bytes; ch %u; rate: %u; rssi %d",
					recv_fr->Data[COUNTER_OFFSET], rxHeader->timestamp, Status, rxHeader->channel, rxHeader->rate, rxHeader->rssi);

			counter++;
			RESP_RECEIVED = TRUE;
			break;
		}

		if (!RESP_RECEIVED) {
			DEBUG("[WARNING] not receive answer from PONGER, retry");
		}
	}

	DEBUG("[PINGER] end of procedure. Exiting...");

	Status = sl_Close(SockID);
	ASSERT_ON_ERROR(Status);

	return SUCCESS;
}

static _i32 main_ponger()
{
	int recv_counter = -1;

	_i16 SockID = sl_Socket(SL_AF_RF, SL_SOCK_RAW, CHANNEL);
	ASSERT_ON_ERROR(SockID);

	_i32 Status = 0;

	// while we are not received the last package from pinger
	while (recv_counter != (END_COUNT - 1)) {

		BOOLEAN RESP_RECEIVED = FALSE;
		for (int rx_counter = 0; rx_counter < TRY_RECIVE_MAX; rx_counter++) {

			Status = sl_Recv(SockID, BUFFER, MAX_PACKET_SIZE, 0);
			SlTransceiverRxOverHead_t *rxHeader = BUFFER;
			DataQoSFrame_FromDSToSTA *recv_fr = (DataQoSFrame_FromDSToSTA*)(BUFFER + sizeof(SlTransceiverRxOverHead_t));
			FrameControl *fc = &recv_fr->FrameControl;

			if(Status < 0)
			{
				DEBUG("[ERROR] while receiving packet");
				sl_Close(SockID);
				return -1;
			}

			if (fc->ProtocolVersion != 0) {
				DEBUG("[MAGIC] Protocol version is not a zero!!!!!!!!");
				continue;
			}

			if (fc->Type != TYPE_DATA) {
				continue;
			}

			if (fc->Subtype != DATA_SUBTYPE_QOS) {
				continue;
			}

			if (memcmp(FRAME.DestinationMAC, recv_fr->DestinationMAC, 6) != 0) {
				continue;
			}

			if (recv_fr->Data[ROLE_OFFSET] != APP_PINGER) {
				DEBUG("[PONGER] Unexpected role of sender: %d", recv_fr->Data[ROLE_OFFSET]);
				continue;
			}

			DEBUG("[%lu] Recv: %d | %d bytes; ch %u; rate: %u; rssi %d",
					recv_fr->Data[COUNTER_OFFSET], rxHeader->timestamp, Status, rxHeader->channel, rxHeader->rate, rxHeader->rssi);

			recv_counter = recv_fr->Data[COUNTER_OFFSET];
			RESP_RECEIVED = TRUE;
			break;
		}

		if (!RESP_RECEIVED) {
			DEBUG("[WARNING] not receive answer from PINGER. recv_counter %d", recv_counter);
		}

		// There is nothing to re-send, we are awaiting the first packet from PINGER
		if (recv_counter < 0) {
			continue;
		}

		FRAME.Data[ROLE_OFFSET] = APP_PONGER;
		FRAME.Data[COUNTER_OFFSET] = recv_counter;

		Status = sl_Send(SockID, (_u8*)&FRAME, sizeof(FRAME),
						 SL_RAW_RF_TX_PARAMS(CHANNEL, RATE, POWER_LEVEL_TONE, PREAMBLE));
		if(Status < 0)
		{
			DEBUG("[ERROR] while sending packet");
			sl_Close(SockID);
			return -1;
		}

		DEBUG("[PONGER] send %d", recv_counter);
	}

	DEBUG("[PONGER] Sending finishing 20 packets to give chance PINGER to finish");
	for (int i = 0; i < 20; i++) {
		Status = sl_Send(SockID, (_u8*)&FRAME, sizeof(FRAME),
								 SL_RAW_RF_TX_PARAMS(CHANNEL, RATE, POWER_LEVEL_TONE, PREAMBLE));
		if(Status < 0)
		{
			DEBUG("[ERROR] while sending packet");
			sl_Close(SockID);
			return -1;
		}
	}

	DEBUG("[PONGER] end of procedure. Exiting...");

	Status = sl_Close(SockID);
	ASSERT_ON_ERROR(Status);

	return SUCCESS;
}

static _i32 RxEvaluation(_i16 channel)
{
    _i16 SockID = 0;
    _i32 Status = 0;
    _u16 Len = 0;
    _i16 count = 0;

    /*
     * Disconnect previous connection if any
     * The function returns 0 if 'Disconnected done', negative number if already disconnected
     * Wait for 'disconnection' event if 0 is returned, Ignore other return-codes
     */
    Status = sl_WlanDisconnect();
    if(0 == Status)
    {
        /* Wait */
        while(IS_CONNECTED(g_Status)) { Delay(300); }
    }

    /* make sure device is disconnected & auto mode is off */
    SockID = sl_Socket(SL_AF_RF, SL_SOCK_RAW, channel);
    ASSERT_ON_ERROR(SockID);



//    SlTransceiverRxOverHead_t *rxHeader = buffer;

//    while (TRUE)
//    {
//    	Status = sl_Recv(SockID, buffer, MAX_PACKET_SIZE, 0);
//
//    	if (Status < 0) {
//    		DEBUG("Broken socket. Exiting...");
//    		sl_Close(SockID);
//    		return -1;
//    	}
//
//    	if (fc->ProtocolVersion != 0) {
//    		DEBUG("[MAGIC] Protocol version is not a zero!!!!!!!!");
//    		continue;
//    	}
//
//    	if (fc->Type != TYPE_DATA) {
//    		continue;
//    	}
//
//    	if (fc->Subtype != DATA_SUBTYPE_QOS) {
//    		continue;
//    	}
//
//    	if (memcmp(&(RawData_Ping[DEST_MAC_OFFSET]),
//    			   &(buffer[sizeof(SlTransceiverRxOverHead_t) + DEST_MAC_OFFSET]),
//				   6) != 0) {
//    		continue;
//    	}
//
//    	DEBUG("[%lu]Recv: %d bytes; ch %u; rate: %u; rssi %d", rxHeader->timestamp, Status, rxHeader->channel, rxHeader->rate, rxHeader->rssi);
//
//    }

    Status = sl_Close(SockID);
    ASSERT_ON_ERROR(Status);

    return SUCCESS;
}


static _i32 initializeAppVariables()
{
    g_Status = 0;

    return SUCCESS;
}

