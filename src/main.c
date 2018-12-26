#include <simplelink.h>

#include "helpers.h"
#include "sl_common.h"




#include "windows.h"

#define CLI_Write(X) DEBUG(X)
#define Delay(X) Sleep(X)
#define MAX_PACKET_SIZE (1472)












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

union
{
  _u8 BsdBuf[BUF_SIZE];
  _u32 demobuf[BUF_SIZE/4];
} uBuf;


const _u8 RawData_Ping[] = {
                   0x88,   /* version , type sub type
                   	   1000      - subtype 8 QoS Data
                   	   .... 10   - type 2 Data frame
                   	   .... ..00 - version 0
                   */
                   0x02,   /* Frame control flag
                   	   0000      ...
                   	   .... 0    Retry
                   	   .... .0   More fragments
                   	   .... ..1  from DS = true : from DS to STA
                   	   .... ...0 to DS = false  :
                   	   	   ->
                   	   	   Addr1=DA, TA=BSSID, Addr3=SA,Addr4=N/A

                  */
                   0x2C, 0x00, /* duration */
                   0x00, 0x23, 0x75, 0x55,0x55, 0x55,   /* destination MAC */
                   0x00, 0x22, 0x75, 0x55,0x55, 0x55,   /* bssid MAC*/
                   0x00, 0x22, 0x75, 0x55,0x55, 0x55,   /* source MAC */
                   0x80, 0x42, /* Seq control:
                   4    2    8    0
                   .... .... .... 0000 - Fragment Number, 0 if the fragment is A-MSDU
                   0100 0010 1000 .... - Sequence Number: 1064
                   */
				   0x00, 0x00, /* QoS Control, as soon as subtype with QoS (b7)
				   .... .... .... 0000 = TID: 0
				   .... .... .... .000 = Priority: Best Effort (0)
				   .... .... ...0 ....
				   .... .... .00. ....
 				   .... .... 0... .... = Payload Type MSDU
				   0000 0000 .... ....
				   */
				   /* LLC */
                   0xAA, // DSAP
				   0xAA, // SSAP
				   0x03, // Control field
				   0x00, 0x00, 0x00, // Organization Code: Encapsulated Ethernet
				   0x08, 0x00, // Type: IPv4
                   /*---- ip header start -----*/
                   0x45, 0x00, 0x00, 0x54, 0x96, 0xA1, 0x00, 0x00, 0x40, 0x01,
                   0x57, 0xFA,                          /* checksum */
                   0xc0, 0xa8, 0x01, 0x64,              /* src ip */
                   0xc0, 0xa8, 0x01, 0x65,              /* dest ip  */
                   /* payload - ping/icmp */
                   0x08, 0x00, 0xA5, 0x51,
                   0x5E, 0x18, 0x00, 0x00, 0x41, 0x08, 0xBB, 0x8D, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x00};
/*
 * GLOBAL VARIABLES -- End
 */

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

/*
 * Application's entry point
 */
int main(int argc, char** argv)
{
    _i32 retVal = -1;

    retVal = initializeAppVariables();
    ASSERT_ON_ERROR(retVal);


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
        CLI_Write(" Failed to configure the device in its default state \n\r");

        LOOP_FOREVER();
    }

    CLI_Write(" Device is configured in default state \n\r");

    /*
     * Assumption is that the device is configured in station mode already
     * and it is in its default state
     */
    /* Initializing the CC3100 device */
    retVal = sl_Start(0, 0, 0);
    if ((retVal < 0) ||
        (ROLE_STA != retVal) )
    {
        CLI_Write(" Failed to start the device \n\r");
        LOOP_FOREVER();
    }

    CLI_Write(" Device started as STATION \n\r");

    CLI_Write(" Sending raw data over wlan PHY \n\r");

    /* Transmits raw packets over the configured channel. There should be
     * minimum 50 ms delay between each packet */
    retVal = RxEvaluation(CHANNEL);
    if(retVal < 0)
        CLI_Write(" Error in sending raw data over wlan PHY \n\r");
    else
        CLI_Write(" Raw data sent over wlan PHY successfully\n\r");


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

    _u32 rate = RATE;
    Status = sl_SetSockOpt(SockID, SL_SOL_PHY_OPT, SL_SO_PHY_RATE, &rate, sizeof(rate));
    assert(Status == 0);

    _u32 preamble = PREAMBLE;
    Status = sl_SetSockOpt(SockID, SL_SOL_PHY_OPT, SL_SO_PHY_PREAMBLE, &preamble, sizeof(preamble));
    assert(Status == 0);

    Len = sizeof(RawData_Ping);

    _u8 buffer[MAX_PACKET_SIZE];

    SlTransceiverRxOverHead_t *rxHeader = buffer;
    while (TRUE)
    {
    	Status = sl_Recv(SockID, buffer, MAX_PACKET_SIZE, 0);

    	if (Status < 0) {
    		DEBUG("Broken socket. Exiting...");
    		sl_Close(SockID);
    		return -1;
    	}

    	DEBUG("[%lu]Recv: %d bytes; ch %u; rate: %u; rssi %d", rxHeader->timestamp, Status, rxHeader->channel, rxHeader->rate, rxHeader->rssi);

    }

    Status = sl_Close(SockID);
    ASSERT_ON_ERROR(Status);

    return SUCCESS;
}


static _i32 initializeAppVariables()
{
    g_Status = 0;
    pal_Memset(uBuf.BsdBuf, 0, sizeof(uBuf));

    return SUCCESS;
}

