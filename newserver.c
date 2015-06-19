/*
 * main.c - TCP socket sample application
 *
 * Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/

/*
 * Application Name     -   TCP socket
 * Application Overview -   This is a sample application demonstrating how to open
 *                          and use a standard TCP socket with CC3100.
 * Application Details  -   http://processors.wiki.ti.com/index.php/CC31xx_TCP_Socket_Application
 *                          doc\examples\tcp_socket.pdf
 */


#include "simplelink.h"
#include "sl_common.h"
#include <stdio.h>
#include <string.h>

#define APPLICATION_VERSION "1.2.0"

#define SL_STOP_TIMEOUT        0xFF

#define STATUS_BIT_PING_DONE  31

#define HOST_NAME "www.lifeoncloud.com"

/*
 * Values for below macros shall be modified for setting the 'Ping' properties
 */
#define PING_INTERVAL   1000    /* In msecs */
#define PING_TIMEOUT    3000    /* In msecs */
#define PING_PKT_SIZE   32      /* In bytes */
#define NO_OF_ATTEMPTS  5

/* IP addressed of server side socket. Should be in long format,
 * E.g: 0xc0a8010a == 192.168.1.10 */
//Default - 0xc0a80166, 5001

#define IP_ADDR         0x689a545b		/* IP address of www.lifeoncloud.com */
#define AP_PORT_NUM     11500      		/* Port number to be used for TCP server in AP mode */
#define ST_PORT_NUM		9734	   		/* Port number to be used for TCP server in STATION mode */
#define UDP_PORT_NUM	5001	   		/* Port number to be used for UDP server */

//socjket definitions
#define BUF_SIZE        1400
#define NO_OF_PACKETS   1000

//device login credentials
#define LOGIN "teena"
#define PSWD "2.2.91"

//UDP socket message
#define IP_REQ "type:scan"


/* directives for AP mode */
/* Use bit 32: Lower bits of status variable are used for NWP events
 *      1 in a 'status_variable', the device has completed the ping operation
 *      0 in a 'status_variable', the device has not completed the ping operation
 */

#define CONFIG_IP       SL_IPV4_VAL(192,168,0,1)    /* Static IP to be configured */
#define CONFIG_MASK     SL_IPV4_VAL(255,255,255,0)  /* Subnet Mask for the station */
#define CONFIG_GATEWAY  SL_IPV4_VAL(192,168,0,1)    /* Default Gateway address */
#define CONFIG_DNS      SL_IPV4_VAL(192,168,0,1)    /* DNS Server Address */

#define DHCP_START_IP    SL_IPV4_VAL(192,168,0,100) /* DHCP start IP address */
#define DHCP_END_IP      SL_IPV4_VAL(192,168,0,200) /* DHCP End IP address */

#define IP_LEASE_TIME        3600

#define IS_PING_DONE(status_variable)	GET_STATUS_BIT(status_variable, STATUS_BIT_PING_DONE)


/* Application specific status/error codes fro AP mode included - LAN_CONNECTION_FAILED, INTERNET_CONNECTION_FAILED */
typedef enum{
    DEVICE_NOT_IN_STATION_MODE = -0x7D0,        /* Choosing this number to avoid overlap w/ host-driver's error codes */
	LAN_CONNECTION_FAILED = DEVICE_NOT_IN_STATION_MODE - 1,
	INTERNET_CONNECTION_FAILED = LAN_CONNECTION_FAILED - 1,
	TCP_SEND_ERROR = INTERNET_CONNECTION_FAILED - 1,
    TCP_RECV_ERROR = TCP_SEND_ERROR -1,
	BSD_UDP_CLIENT_FAILED = TCP_RECV_ERROR - 1,

    STATUS_CODE_MAX = -0xBB8
}e_AppStatusCodes;


/*
 * GLOBAL VARIABLES -- Start
 */

union
{
    _u8 BsdBuf[BUF_SIZE];
    _u32 demobuf[BUF_SIZE/4];
} uBuf;

_u32	g_Status = 0;
_u32	g_PingPacketsRecv = 0;
_u32	g_GatewayIP = 0;
_u32  	g_StationIP = 0;

_u8		ssid[24] = {'\0'}; 		//SSID of the AP
_u8 	pswd[24] = {'\0'}; 		//{SWD pf the AP
//_u8 	username[24] = {'\0'};
//_u8 	password[24] = {'\0'};
char	server[64] = {'\0'}; 	//name of server

/* To display the IP address and MAC address of the device*/
SlNetCfgIpV4Args_t 	ipV4 = {0};
char	IPaddress[32] = {'\0'};
char 	MACaddress[32] = {'\0'};
char 	deviceINFO[64] = {'\0'};
_u8		len = sizeof(SlNetCfgIpV4Args_t);
_u8		dhcpIsOn = 0;
_u8 	macAddrLen = SL_MAC_ADDR_LEN;
_u8 	macAddr[SL_MAC_ADDR_LEN];

/*
 * GLOBAL VARIABLES -- End
 */


/*
 * STATIC FUNCTION DEFINITIONS -- Start
 */
static _i32 configureSimpleLinkToDefaultState();
static _i32 establishConnectionWithAP();
static _i32 checkLanConnection();
static _i32 checkInternetConnection();
static _i32 APmodeServer(_u16 Port);
static _i32 StationModeServer(_u16 Port);
//static _i32 UdpServer(_u16 Port);
//static _i32 BsdTcpClient(_u16 Port);
static _i32 initializeAppVariables();
static void displayBanner();
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
            CLR_STATUS_BIT(g_Status, STATUS_BIT_IP_ACQUIRED);

            pEventData = &pWlanEvent->EventData.STAandP2PModeDisconnected;

            /* If the user has initiated 'Disconnect' request, 'reason_code' is
             * SL_USER_INITIATED_DISCONNECTION */
            if(SL_USER_INITIATED_DISCONNECTION == pEventData->reason_code)
            {
                CLI_Write(" Device disconnected from the AP on application's request \n\r");
            }
            else
            {
                CLI_Write(" Device disconnected from the AP on an ERROR..!! \n\r");
            }
        }
        break;

        /* the following 2 cases are for AP mode */
        case SL_WLAN_STA_CONNECTED_EVENT:
        {
            SET_STATUS_BIT(g_Status, STATUS_BIT_STA_CONNECTED);
        }
        break;

        case SL_WLAN_STA_DISCONNECTED_EVENT:
        {
            CLR_STATUS_BIT(g_Status, STATUS_BIT_STA_CONNECTED);
            CLR_STATUS_BIT(g_Status, STATUS_BIT_IP_LEASED);
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
        	SlIpV4AcquiredAsync_t *pEventData = NULL; //for AP mode

            SET_STATUS_BIT(g_Status, STATUS_BIT_IP_ACQUIRED);

            //for AP mode
            pEventData = &pNetAppEvent->EventData.ipAcquiredV4;
            g_GatewayIP = pEventData->gateway;

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

        /* the following case is for AP mode*/
        case SL_NETAPP_IP_LEASED_EVENT:
        {
            g_StationIP = pNetAppEvent->EventData.ipLeased.ip_address;
            SET_STATUS_BIT(g_Status, STATUS_BIT_IP_LEASED);
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

/*! FOR AP mode
    \brief This function handles ping report events

    \param[in]      pPingReport holds the ping report statistics

    \return         None

    \note

    \warning
*/

static void SimpleLinkPingReport(SlPingReport_t *pPingReport)
{
    SET_STATUS_BIT(g_Status, STATUS_BIT_PING_DONE);

    if(pPingReport == NULL)
    {
        CLI_Write((_u8 *)" [PING REPORT] NULL Pointer Error\r\n");
        return;
    }

    g_PingPacketsRecv = pPingReport->PacketsReceived;
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
                    CLI_Write(" [SOCK EVENT] Close socket operation, failed to transmit all queued packets\n\r");
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
	_u8 SecType = 0;
	_i32 mode = ROLE_STA;

	_i32 retVal = -1;

    retVal = initializeAppVariables();
    ASSERT_ON_ERROR(retVal);

    /* Stop WDT and initialize the system-clock of the MCU
       These functions needs to be implemented in PAL */
    stopWDT();
    initClk();

    /* Configure command line interface */
    CLI_Configure();

    displayBanner();

    /* To configure the device in AP mode */
    //CLR_STATUS_BIT(g_Status, STATUS_BIT_PING_DONE);
    //g_PingPacketsRecv = 0;

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
        if (DEVICE_NOT_IN_STATION_MODE == retVal)
        {
            CLI_Write((_u8 *)" Failed to configure the device in its default state \n\r");
        }

        LOOP_FOREVER();
    }

    //CLI_Write(" Device is configured in default state \n\r");

    /*
     * Assumption is that the device is configured in station mode already
     * and it is in its default state
     */

    // Starting in AP mode
    mode = sl_Start(0, 0, 0);
        if (ROLE_AP == mode)
        {
            // If the device is in AP mode, we need to wait for this event before doing anything
            while(!IS_IP_ACQUIRED(g_Status)) { _SlNonOsMainLoopTask(); }
        }
        else
        {
            // Configure CC3100 to start in AP mode
            retVal = sl_WlanSetMode(ROLE_AP);
            if(retVal < 0)
                LOOP_FOREVER();

            // Configure the SSID of the CC3100
            retVal = sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_SSID,
                    pal_Strlen(SSID_AP_MODE), (_u8 *)SSID_AP_MODE);
            if(retVal < 0)
                LOOP_FOREVER();

            SecType = SEC_TYPE_AP_MODE;
            // Configure the Security parameter the AP mode
            retVal = sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_SECURITY_TYPE, 1,
                    (_u8 *)&SecType);
            if(retVal < 0)
                LOOP_FOREVER();

            retVal = sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_PASSWORD, pal_Strlen(PASSWORD_AP_MODE),
                    (_u8 *)PASSWORD_AP_MODE);
            if(retVal < 0)
                LOOP_FOREVER();

            retVal = sl_Stop(SL_STOP_TIMEOUT);
            if(retVal < 0)
                LOOP_FOREVER();

            CLR_STATUS_BIT(g_Status, STATUS_BIT_IP_ACQUIRED);

            mode = sl_Start(0, 0, 0);
            if (ROLE_AP == mode)
            {
                // If the device is in AP mode, we need to wait for this event before doing anything
                while(!IS_IP_ACQUIRED(g_Status)) { _SlNonOsMainLoopTask(); }
            }
            else
            {
                CLI_Write((_u8 *)" Device couldn't be configured in AP mode \n\r");
                LOOP_FOREVER();
            }
        }

        CLI_Write((_u8 *)" Device started as Access Point - ");
        CLI_Write((_u8 *)SSID_AP_MODE);
        CLI_Write((_u8 *)"\r\n");

    /*printf("IP %d.%d.%d.%d MASK %d.%d.%d.%d GW %d.%d.%d.%d DNS %d.%d.%d.%d\n",
            SL_IPV4_BYTE(ipV4.ipV4,3),SL_IPV4_BYTE(ipV4.ipV4,2),SL_IPV4_BYTE(ipV4.ipV4,1),SL_IPV4_BYTE(ipV4.ipV4,0),
            SL_IPV4_BYTE(ipV4.ipV4Mask,3),SL_IPV4_BYTE(ipV4.ipV4Mask,2),SL_IPV4_BYTE(ipV4.ipV4Mask,1),SL_IPV4_BYTE(ipV4.ipV4Mask,0),
            SL_IPV4_BYTE(ipV4.ipV4Gateway,3),SL_IPV4_BYTE(ipV4.ipV4Gateway,2),SL_IPV4_BYTE(ipV4.ipV4Gateway,1),SL_IPV4_BYTE(ipV4.ipV4Gateway,0),
            SL_IPV4_BYTE(ipV4.ipV4DnsServer,3),SL_IPV4_BYTE(ipV4.ipV4DnsServer,2),SL_IPV4_BYTE(ipV4.ipV4DnsServer,1),SL_IPV4_BYTE(ipV4.ipV4DnsServer,0));*/

    // Display the IP address
    //sl_NetCfgGet(SL_IPV4_STA_P2P_CL_GET_INFO,&dhcpIsOn,&len,(_u8 *)&ipV4);
    sl_NetCfgGet(SL_IPV4_AP_P2P_GO_GET_INFO,&dhcpIsOn,&len,(_u8 *)&ipV4);
    snprintf (IPaddress, 32, " IP address: %d.%d.%d.%d", (_u8)SL_IPV4_BYTE(ipV4.ipV4,3),(_u8)SL_IPV4_BYTE(ipV4.ipV4,2),(_u8)SL_IPV4_BYTE(ipV4.ipV4,1),(_u8)SL_IPV4_BYTE(ipV4.ipV4,0));
    CLI_Write((_u8*)IPaddress);
    CLI_Write((_u8*)"\r\n");

    // Display the MAC address
    sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &macAddrLen, macAddr);
    snprintf (MACaddress, 32, " MAC address: %x:%x:%x:%x:%x:%x", macAddr[0],macAddr[1],macAddr[2],macAddr[3],macAddr[4],macAddr[5]);
    CLI_Write((_u8*)MACaddress);
    CLI_Write((_u8*)"\r\n");

    /* Sockets */

    //Before proceeding, please make sure to have a server waiting on PORT_NUM
    //retVal = BsdTcpClient(PORT_NUM);
    //if(retVal < 0)
    //  CLI_Write(" Failed to establishing connection with TCP server \n\r");
    //else
    //  CLI_Write(" Connection with TCP server established successfully \n\r");


    //This function will create a server socket on the device that
    //can acept client requests and process them.
    retVal = APmodeServer(AP_PORT_NUM);

    if(retVal < 0)
    	CLI_Write((_u8 *)" AP mode SERVER ERROR \n\r");

    CLI_Write((_u8 *)" Configuring the device as STATION... \n\r");

    /* STATION MODE */

    retVal = configureSimpleLinkToDefaultState();
    if(retVal < 0)
    {
    	if (DEVICE_NOT_IN_STATION_MODE == retVal)
    		CLI_Write((_u8 *)" Failed to configure the device in its default state \n\r");

    	LOOP_FOREVER();
    }

    // Starting in Station mode Initializing the CC3100 device
    retVal = sl_Start(0, 0, 0);
    if ((retVal < 0) ||
        (ROLE_STA != retVal) )
    {
        CLI_Write((_u8 *)" Failed to start the device \n\r");
        LOOP_FOREVER();
    }

    CLI_Write((_u8 *)" Device started as STATION \n\r");

    // Connecting to WLAN AP - Set with static parameters defined at the top
    // After this call we will be connected and have IP address
    retVal = establishConnectionWithAP();
    if(retVal < 0)
    {
        CLI_Write((_u8 *)" Failed to establish connection w/ an AP \n\r");
        LOOP_FOREVER();
    }

    CLI_Write((_u8 *)" Connection established w/ AP and IP is acquired \n\r");

    // Display the IP address acquired after changing to STATION
    sl_NetCfgGet(SL_IPV4_STA_P2P_CL_GET_INFO,&dhcpIsOn,&len,(_u8 *)&ipV4);
    snprintf (IPaddress, 32, " IP address: %d.%d.%d.%d", (_u8)SL_IPV4_BYTE(ipV4.ipV4,3),(_u8)SL_IPV4_BYTE(ipV4.ipV4,2),(_u8)SL_IPV4_BYTE(ipV4.ipV4,1),(_u8)SL_IPV4_BYTE(ipV4.ipV4,0));
    CLI_Write((_u8*)IPaddress);
    CLI_Write((_u8*)"\r\n");

    /* Concatenate the IP address and MAC address to send to the app */
    snprintf (deviceINFO, 64, "%s %s", IPaddress, MACaddress);

    // Server in the STATION Mode
    CLI_Write((_u8 *)" Starting TCP server on the device in Station mode...\r\n");
    retVal = StationModeServer(ST_PORT_NUM);

    if(retVal < 0)
    	CLI_Write((_u8 *)" STATION mode SERVER ERROR \n\r");

    CLI_Write((_u8 *)" Pinging the gateway...\n\r");

    /*// UDP socket to send IP address to the app
    CLI_Write((_u8 *)" Starting UDP server on the device in Station mode.\r\n");
    retVal = UdpServer(UDP_PORT_NUM);

    if(retVal < 0)
    	CLI_Write((_u8 *)" UDP SERVER ERROR \n\r");
    else
    	CLI_Write((_u8 *)" I've reached here.\r\n");*/

    retVal = checkLanConnection();
    if(retVal < 0)
    {
        CLI_Write((_u8 *)" Device couldn't connect to LAN \n\r");
        LOOP_FOREVER();
    }

    CLI_Write((_u8 *)" Device successfully connected to the LAN\r\n");

    CLI_Write((_u8 *)" Pinging the server...\n\r");
    retVal = checkInternetConnection();
    if(retVal < 0)
    {
        CLI_Write((_u8 *)" Device couldn't connect to the internet \n\r");
        LOOP_FOREVER();
    }

    CLI_Write((_u8 *)" Device successfully connected to the internet\r\n");


    //Stop the CC3100 device
    retVal = sl_Stop(SL_STOP_TIMEOUT);
    if(retVal < 0)
    {
    	LOOP_FOREVER();
    }

    return 0;
}

/*!
    \brief This function configure the SimpleLink device in its default state. It:
           - Sets the mode to STATION
           - Configures connection policy to Auto and AutoSmartConfig
           - Deletes all the stored profiles
           - Enables DHCP
           - Disables Scan policy
           - Sets Tx power to maximum
           - Sets power policy to normal
           - Unregisters mDNS services
           - Remove all filters

    \param[in]      none

    \return         On success, zero is returned. On error, negative is returned
*/
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

    /* If the device is not in station-mode, try configuring it in staion-mode */
    if (ROLE_STA != mode)
    {
        if (ROLE_AP == mode)
        {
            /* If the device is in AP mode, we need to wait for this event before doing anything */
            while(!IS_IP_ACQUIRED(g_Status)) { _SlNonOsMainLoopTask(); }
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
        while(IS_CONNECTED(g_Status)) { _SlNonOsMainLoopTask(); }
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

    retVal = initializeAppVariables();
    ASSERT_ON_ERROR(retVal);

    return retVal; /* Success */
}

/*!
    \brief Connecting to a WLAN Access point

    This function connects to the required AP (SSID_NAME).
    The function will return once we are connected and have acquired IP address

    \param[in]  None

    \return     0 on success, negative error-code on error

    \note

    \warning    If the WLAN connection fails or we don't acquire an IP address,
                We will be stuck in this function forever.*/

static _i32 establishConnectionWithAP()
{
	/* replacing PASSKEY with pswd and SSID_NAME with ssid */

    SlSecParams_t secParams = {0};
    _i32 retVal = 0;

    //secParams.Key = PASSKEY;
    secParams.Key = (_i8 *)pswd;
    //secParams.KeyLen = pal_Strlen(PASSKEY);
    secParams.KeyLen = (_u8)pal_Strlen(pswd);
    secParams.Type = SEC_TYPE;

    retVal = sl_WlanConnect((_i8 *)ssid, pal_Strlen(ssid), 0, &secParams, 0);
    //retVal = sl_WlanConnect(SSID_NAME, pal_Strlen(SSID_NAME), 0, &secParams, 0);
    ASSERT_ON_ERROR(retVal);

    // Wait
    while((!IS_CONNECTED(g_Status)) || (!IS_IP_ACQUIRED(g_Status)))
    {
    	_SlNonOsMainLoopTask();
    }

    return SUCCESS;
}



/*!
    \brief Opening a client side socket and sending data

    This function opens a TCP socket and tries to connect to a Server IP_ADDR
    waiting on port PORT_NUM. If the socket connection is successful then the
    function will send 1000 TCP packets to the server.

    \param[in]      port number on which the server will be listening on

    \return         0 on success, -1 on Error.

    \note

    \warning        A server must be waiting with an open TCP socket on the
                    right port number before calling this function.
                    Otherwise the socket creation will fail.
*/

/* Client side commented out for the time-being.
 * static _i32 BsdTcpClient(_u16 Port)
{
    SlSockAddrIn_t  Addr;

    _u16          idx = 0;
    _u16          AddrSize = 0;
    _i16          SockID = 0;
    _i16          Status = 0;
    _u16          LoopCount = 0;

    for (idx=0 ; idx<BUF_SIZE ; idx++)
    {
        uBuf.BsdBuf[idx] = (_u8)(idx % 10);
    }

    Addr.sin_family = SL_AF_INET;
    Addr.sin_port = sl_Htons((_u16)Port);
    Addr.sin_addr.s_addr = sl_Htonl((_u32)IP_ADDR);
    AddrSize = sizeof(SlSockAddrIn_t);

    SockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, 0);
    if( SockID < 0 )
    {
        CLI_Write(" [TCP Client] Create socket Error \n\r");
        ASSERT_ON_ERROR(SockID);
    }

    Status = sl_Connect(SockID, ( SlSockAddr_t *)&Addr, AddrSize);
    if( Status < 0 )
    {
        sl_Close(SockID);
        CLI_Write(" [TCP Client]  TCP connection Error \n\r");
        ASSERT_ON_ERROR(Status);
    }

    while (LoopCount < NO_OF_PACKETS)
    {
        Status = sl_Send(SockID, uBuf.BsdBuf, BUF_SIZE, 0 );
        if( Status <= 0 )
        {
            CLI_Write(" [TCP Client] Data send Error \n\r");
            Status = sl_Close(SockID);
            ASSERT_ON_ERROR(TCP_SEND_ERROR);
        }

        LoopCount++;
    }

    Status = sl_Close(SockID);
    ASSERT_ON_ERROR(Status);

    return SUCCESS;
}*/

/*!
    \brief Opening a server side socket and receiving data

    This function opens a TCP socket in Listen mode and waits for an incoming
    TCP connection.

    \param[in]      port number on which the server will be listening on

    \return         0 on success, -1 on Error.

    \note           This function will wait for an incoming connection till one
                    is established

    \warning
*/
static _i32 APmodeServer(_u16 Port)
{
    SlSockAddrIn_t  Addr;
    SlSockAddrIn_t  LocalAddr;

    _u16	AddrSize = 0;
    _i16    SockID = 0;
    _i32    Status = 0;
    _i16    newSockID = 0;
    _i16    recvSize = 64;
    _u32 	len = 0;
    _i32 	state = 0;
    char 	data[64] = {'\0'};
    char* 	Msg;
    _u16	char_position = 0;
    _u16	idx = 0;

    LocalAddr.sin_family = SL_AF_INET;
    LocalAddr.sin_port = sl_Htons((_u16)Port);
    LocalAddr.sin_addr.s_addr = sl_Htonl(SL_INADDR_ANY);

    AddrSize = sizeof(SlSockAddrIn_t);

    // Open a socket
    SockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, 0);
    if( SockID < 0 )
    {
        CLI_Write(" [TCP Server] Create socket Error \n\r");
        ASSERT_ON_ERROR(SockID);
    }

    // Bind the socket to the port 11500
    Status = sl_Bind(SockID, (SlSockAddr_t *)&LocalAddr, AddrSize);
    if( Status < 0 )
    {
        sl_Close(SockID);
        CLI_Write(" [TCP Server] Bind socket Error \n\r");
        ASSERT_ON_ERROR(Status);
    }

    // Listen on the socket
    Status = sl_Listen(SockID, 0);
    if( Status < 0 )
    {
        sl_Close(SockID);
        CLI_Write(" [TCP Server] Listen Error \n\r");
        ASSERT_ON_ERROR(Status);
    }

    while(state != 2)
    {
    	// If a client is not already connected, accept a connection
    	if(!newSockID)
    	{
    		newSockID = sl_Accept(SockID, ( struct SlSockAddr_t *)&Addr, (SlSocklen_t*)&AddrSize);
    		if( newSockID < 0 )
    		{
    		    sl_Close(SockID);
    		    CLI_Write(" [TCP Server] Accept connection Error \n\r");
    		    ASSERT_ON_ERROR(newSockID);
    		}

    		else
    		{
    			// Send a message to the client asking for user name
    			Msg = "What is your user name?\r\n";
	    		Status = sl_Send(newSockID, Msg, pal_Strlen(Msg), 0 );
	    		if(Status <= 0)
	    		{
	    			CLI_Write(" [TCP Server] Write message Error.n\r");
	    			ASSERT_ON_ERROR(Status);
	    		}

	    		// Receive the response from the client
	    		else
	    		{
	    			Status = sl_Recv(newSockID, data, recvSize, 0);
	    			if( Status <= 0 )
	    			{
	    			    sl_Close(newSockID);
	    			    CLI_Write(" [TCP Server] Data recv Error \n\r");
	    			    ASSERT_ON_ERROR(TCP_RECV_ERROR);
	    			}
	    			// If username was received, check if it is valid
	    			else
	    			{
	    				len = strlen(data) - 2;

    			    	if(!strncmp(LOGIN, data, len))
    			    	{
    			    		Msg = "What is the password?\r\n";
    			    	}
    			    	else
    			    	{
    			    		Msg = "Invalid user name";
    			    	}

    			    	// Reply to the client with "Valid/Invalid user name"
    			    	Status = sl_Send(newSockID, Msg, pal_Strlen(Msg), 0 );
    			    	if(Status <= 0)
    			        {
    			    		CLI_Write(" [TCP Server] Write message Error.n\r");
    			    		ASSERT_ON_ERROR(Status);
    			    	}
    			    	// Receive the password from the client
    			    	else
    			    	{
    			    		Status = sl_Recv(newSockID, data, recvSize, 0);
    			    		if( Status <= 0 )
    			    		{
    			    			  sl_Close(newSockID);
    			    			  CLI_Write(" [TCP Server] Data recv Error \n\r");
    			    			  ASSERT_ON_ERROR(TCP_RECV_ERROR);
    			    		}
    			    		// If password was received, check if it is correct
    			    		else
    			    		{
    			    			len = strlen(data) - 2;
   		    			    	if(!strncmp(PSWD, data, len))
   		    			    	{
   		    			    		Msg = "Send Router SSID as <SSID>;\r\n";
   		    			    	}
   		    			    	else
   		    			    	{
   		    			    		Msg = " Incorrect password.";
   		    			    	}

   		    			    	// If password matched, ask for SSID
   		    			    	Status = sl_Send(newSockID, Msg, pal_Strlen(Msg), 0 );
   		    			    	if(Status <= 0)
   		    			        {
   		    			    		CLI_Write(" [TCP Server] Write message Error.n\r");
   		    			    		ASSERT_ON_ERROR(Status);
   		    			    	}
   		    			    	else
   		    			    	{
   		    			    		Status = sl_Recv(newSockID, data, recvSize, 0);
   		    			    		if( Status <= 0 )
   		    			    		{
   		    			    			  sl_Close(newSockID);
   		    			    		      CLI_Write(" [TCP Server] Data recv Error \n\r");
   		    			    		      ASSERT_ON_ERROR(TCP_RECV_ERROR);
   		    			    		}
   		    			    		else
   		    			    		{
   		    			    		      // Save the SSID
   		    			    		      while(data[char_position] != ';')
   	   			    		    	      {
   	   			    		    		      ssid[idx] = data[char_position];
   	   			    		    		      char_position++;
   	   			    		    		      idx++;
   	   			    		    	      }
   		    			    		      ssid[idx] = '\0';

       			    		    	      idx = 0;
       			    		    	      char_position = 0;

       			    		    	      Msg = "Send Router PASSKEY as <PASSKEY>;\r\n";
       			    		    	      Status = sl_Send(newSockID, Msg, pal_Strlen(Msg), 0 );
       			    		    	      if(Status <= 0)
       			    		    	      {
       			    		    	    	  CLI_Write(" [TCP Server] Write message Error.n\r");
       			    		    	    	  ASSERT_ON_ERROR(Status);
       			    		    	      }
       			    		    	      else
       			    		    	      {
       			    		    	    	Status = sl_Recv(newSockID, data, recvSize, 0);
       			    		    	    	if( Status <= 0 )
       			    		    	    	{
       			    		    	    		sl_Close(newSockID);
       			    		    	    		CLI_Write(" [TCP Server] Data recv Error \n\r");
       			    		    	    		ASSERT_ON_ERROR(TCP_RECV_ERROR);
       			    		    	    	}
       			    		    	    	else
       			    		    	    	{
       			    		    	    		// save the PASSKEY
       			    		    	    		while(data[char_position] != ';')
       			    		    	    		{
       			    		    	    			pswd[idx] = data[char_position];
       			    		    	    			char_position++;
       			    		    	    			idx++;
       			    		    	    		}

       			    		    	    		pswd[idx] = '\0';

       			    		    	    		Status = sl_Send(newSockID, MACaddress, pal_Strlen(MACaddress), 0 );
       			    		    	    		if(Status <= 0)
       			    		    	    		{
       			    		    	    			CLI_Write(" [TCP Server] Write message Error.n\r");
       			    		    	    			ASSERT_ON_ERROR(Status);
       			    		    	    		}
       			    		    	    	}
       			    		    	    }
   		    			    		}
   		    			    	}
    			    		}
    			    	}
    			    }
	    		}

	    		Status = sl_Close(newSockID);
	    		ASSERT_ON_ERROR(Status);
	    		// Free the client socket, make it re-assignable
	    	    newSockID = 0;
	    	    state = 2;
	    	}
    	}
    }

    Status = sl_Close(SockID);
    ASSERT_ON_ERROR(Status);

    return SUCCESS;
}

/* Server when the device is in STATION mode */
static _i32 StationModeServer(_u16 Port)
{
    SlSockAddrIn_t  Addr;
    SlSockAddrIn_t  LocalAddr;

    _u16          AddrSize = 0;
    _i16          SockID = 0;
    _i32          Status = 0;
    _i16          newSockID = 0;
    _i16          recvSize = 64;
//    _u32 		  len = 0;
    _i32 		  state = 0;
    char 		  data[64] = {'\0'};
    char* 		  Msg;
//    _u16		  char_position = 0;
    _u16		  idx = 0;

    LocalAddr.sin_family = SL_AF_INET;
    LocalAddr.sin_port = sl_Htons((_u16)Port);
    LocalAddr.sin_addr.s_addr = sl_Htonl(SL_INADDR_ANY);

    AddrSize = sizeof(SlSockAddrIn_t);

    // Open a socket
    SockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, 0);
    if( SockID < 0 )
    {
        CLI_Write(" [Station Mode Server] Create socket Error \n\r");
        ASSERT_ON_ERROR(SockID);
    }

    // Bind the socket to a port
    Status = sl_Bind(SockID, (SlSockAddr_t *)&LocalAddr, AddrSize);
    if( Status < 0 )
    {
        sl_Close(SockID);
        CLI_Write(" [Station Mode Server] Bind socket Error \n\r");
        ASSERT_ON_ERROR(Status);
    }

    // Listen on the socket
    Status = sl_Listen(SockID, 0);
    if( Status < 0 )
    {
        sl_Close(SockID);
        CLI_Write(" [Station Mode Server] Listen Error \n\r");
        ASSERT_ON_ERROR(Status);
    }

    while(state != 2)
    {
    	// If a client is not already connected, accept a connection
    	if(!newSockID)
    	{
    		CLI_Write((_u8 *)" Accepting a connection on port number - 9734\r\n");

    		newSockID = sl_Accept(SockID, ( struct SlSockAddr_t *)&Addr, (SlSocklen_t*)&AddrSize);
    		if( newSockID < 0 )
    		{
    		    sl_Close(SockID);
    		    CLI_Write(" [Station Mode Server] Accept connection Error \n\r");
    		    ASSERT_ON_ERROR(newSockID);
    		}

    		else
    		{
	    		// Send a message to the client asking for the server name
    			Msg = "Send name of server as <name>;\r\n";
	    		Status = sl_Send(newSockID, Msg, pal_Strlen(Msg), 0 );
	    		if(Status <= 0)
	    		{
	    			CLI_Write(" [Station Mode Server] Write message Error.n\r");
	    			ASSERT_ON_ERROR(Status);
	    		}

	    		/* receive the response from the client */
	    		else
	    		{
	    			Status = sl_Recv(newSockID, data, recvSize, 0);
	    			if( Status <= 0 )
	    			{
	    			    sl_Close(newSockID);
	    			    CLI_Write(" [Station Mode Server] Data recv Error \n\r");
	    			    ASSERT_ON_ERROR(TCP_RECV_ERROR);
	    			}
	    			/* Receive the server name from the client */
	    			else
	    			{
	    				// Save the SSID
	    				while(data[idx] != ';')
	    				{
	    				server[idx] = data[idx];
	    				//char_position++;
	    				idx++;
	    				}
	    				server[idx] = '\0';

	    				Msg = " Received the server name";
    			    	Status = sl_Send(newSockID, Msg, pal_Strlen(Msg), 0 );
    			    	if(Status <= 0)
    			        {
    			    		CLI_Write(" [Station Mode Server] Write message Error.n\r");
    			    		ASSERT_ON_ERROR(Status);
    			    	}
    			    	else
    			    	{
    			    		CLI_Write((_u8 *)" Login complete\n\r");
    			    	}
	    			}
	    		}

	    		Status = sl_Close(newSockID);
	    		ASSERT_ON_ERROR(Status);
	    		// Free the client socket, make it re-assignable
	    	    newSockID = 0;
	    	    state = 2;
	    	}
    	}
    }

    Status = sl_Close(SockID);
    ASSERT_ON_ERROR(Status);

    return SUCCESS;
}

/* This functions opens a UDP socket on port 5001,
 * compares the received message against the IP_REQ message
 * and sends back a reply */

/*
static _i32 UdpServer(_u16 Port)
{
    SlSockAddrIn_t  myAddr;
    SlSockAddrIn_t  remoteAddr;

    _u16          AddrSize = 0;
    _i16          SockID = 0;
    _i32          Status = 0;
    char 		  data[64] = {'\0'};
    char* 		  reply;
    _u16		  char_position = 0;
    _u16		  idx = 0;
    char 		  msg[24] = {'\0'};
    _u16          LoopCount = 0;
    _u16          recvSize = 0;


    myAddr.sin_family = SL_AF_INET;
    myAddr.sin_port = sl_Htons((_u16)Port);
    myAddr.sin_addr.s_addr = 0;//sl_Htonl(SL_INADDR_ANY);

    AddrSize = sizeof(SlSockAddrIn_t);

    // Open a socket
    SockID = sl_Socket(SL_AF_INET,SL_SOCK_DGRAM, 0);
    if( SockID < 0 )
    {
        CLI_Write(" [UDP Server] Create socket Error \n\r");
        ASSERT_ON_ERROR(SockID);
    }

    // Bind the socket to a port
    Status = sl_Bind(SockID, (SlSockAddr_t *)&myAddr, AddrSize);
    if( Status < 0 )
    {
    	CLI_Write(" [UDP Server] Bind socket Error \n\r");
    	Status = sl_Close(SockID);
        ASSERT_ON_ERROR(Status);
    }

    CLI_Write(" UDP socket is ready.\n\r");

    // now loop, receiving data and printing what we received
    while (LoopCount < 1)
    {
    	recvSize = 64;

    	//do
    	//{
    		Status = sl_RecvFrom(SockID, data, recvSize, 0, (SlSockAddr_t *)&remoteAddr, (SlSocklen_t*)&AddrSize );
    		if(Status < 0)
    		{
    			sl_Close(SockID);
    			ASSERT_ON_ERROR(Status);
   			}
    		else
    		// Receive the request and send a reply
    		{
    			data[Status] = 0;

    			while(data[char_position] != ';')
    			{
    				msg[idx] = data[char_position];
    				char_position++;
    				idx++;
    			}

    			msg[idx] = '\0';

    			if(!strcmp(IP_REQ, msg))
    			{
    				Status = sl_SendTo(SockID, deviceINFO, strlen(deviceINFO), 0, (SlSockAddr_t *)&remoteAddr, AddrSize);
    			}
    			else
    			{
    				reply = "Wrong request";
    				Status = sl_SendTo(SockID, reply, strlen(reply), 0, (SlSockAddr_t *)&remoteAddr, AddrSize);
    			}

    			if( Status <= 0 )
    			{
    				Status = sl_Close(SockID);
    				ASSERT_ON_ERROR(BSD_UDP_CLIENT_FAILED);
    			}

    		}

    		//recvSize -= Status;

    	//}while(recvSize > 0);

    	LoopCount++;
    }

    Status = sl_Close(SockID);
    ASSERT_ON_ERROR(Status);

    CLI_Write(" Returning\n\r");
    return SUCCESS;
}*/


/*!
    \brief This function checks the LAN connection by pinging the AP's gateway

    \param[in]  None

    \return     0 on success, negative error-code on error
*/
static _i32 checkLanConnection()
{
    SlPingStartCommand_t pingParams = {0};
    SlPingReport_t pingReport = {0};

    _i32 retVal = -1;

    CLR_STATUS_BIT(g_Status, STATUS_BIT_PING_DONE);
    g_PingPacketsRecv = 0;

    /* Set the ping parameters */
    pingParams.PingIntervalTime = PING_INTERVAL;
    pingParams.PingSize = PING_PKT_SIZE;
    pingParams.PingRequestTimeout = PING_TIMEOUT;
    pingParams.TotalNumberOfAttempts = NO_OF_ATTEMPTS;
    pingParams.Flags = 0;
    pingParams.Ip = g_GatewayIP;

    /* Check for LAN connection */
    retVal = sl_NetAppPingStart( (SlPingStartCommand_t*)&pingParams, SL_AF_INET,
                                 (SlPingReport_t*)&pingReport, SimpleLinkPingReport);
    ASSERT_ON_ERROR(retVal);

    /* Wait */
    while(!IS_PING_DONE(g_Status))
    {
    	_SlNonOsMainLoopTask();
    }

    if(0 == g_PingPacketsRecv)
    {
        /* Problem with LAN connection */
    	CLI_Write((_u8*) "LAN CONNECTION FAILED");
        ASSERT_ON_ERROR(LAN_CONNECTION_FAILED);
    }

    //CLI_Write((_u8*) " Packets received\r\n");

    /* LAN connection is successful */
    return SUCCESS;
}

/*!
    \brief This function checks the internet connection by pinging
           the external-host (HOST_NAME)

    \param[in]  None

    \return     0 on success, negative error-code on error
*/
static _i32 checkInternetConnection()
{
    SlPingStartCommand_t pingParams = {0};
    SlPingReport_t pingReport = {0};

    _u32 ipAddr = 0;

    _i32 retVal = -1;

    char IP[32] = {};
    _u8 bytes[4] = {};

    CLR_STATUS_BIT(g_Status, STATUS_BIT_PING_DONE);
    g_PingPacketsRecv = 0;

    /* Set the ping parameters */
    pingParams.PingIntervalTime = PING_INTERVAL;
    pingParams.PingSize = PING_PKT_SIZE;
    pingParams.PingRequestTimeout = PING_TIMEOUT;
    pingParams.TotalNumberOfAttempts = NO_OF_ATTEMPTS;
    pingParams.Flags = 0;
    pingParams.Ip = g_GatewayIP;

    /* Check for Internet connection */
    //retVal = sl_NetAppDnsGetHostByName((_i8 *)HOST_NAME, pal_Strlen(HOST_NAME), &ipAddr, SL_AF_INET);
    retVal = sl_NetAppDnsGetHostByName((_i8 *)server, pal_Strlen(server), &ipAddr, SL_AF_INET);
    ASSERT_ON_ERROR(retVal);

    /* Replace the ping address to match HOST_NAME's IP address */
    pingParams.Ip = ipAddr;

    /* Try to ping HOST_NAME */
    retVal = sl_NetAppPingStart( (SlPingStartCommand_t*)&pingParams, SL_AF_INET,
                                 (SlPingReport_t*)&pingReport, SimpleLinkPingReport);
    ASSERT_ON_ERROR(retVal);

    /* Wait */
    while(!IS_PING_DONE(g_Status)) { _SlNonOsMainLoopTask(); }

    if (0 == g_PingPacketsRecv)
    {
        /* Problem with internet connection*/
        ASSERT_ON_ERROR(INTERNET_CONNECTION_FAILED);
    }

    //my_changes
    bytes[0] = pingParams.Ip & 0xFF;
    bytes[1] = (pingParams.Ip >> 8) & 0xFF;
    bytes[2] = (pingParams.Ip >> 16) & 0xFF;
    bytes[3] = (pingParams.Ip >> 24) & 0xFF;
    snprintf(IP, 32, "%d.%d.%d.%d", bytes[3], bytes[2], bytes[1], bytes[0]);
    CLI_Write((_u8 *)" IP address is ");
    CLI_Write((_u8 *)IP);
    CLI_Write((_u8 *)"\r\n");
    CLI_Write((_u8 *)" Host name is - ");
    CLI_Write((_u8 *)server);
    CLI_Write((_u8 *)"\r\n");

    /* Internet connection is successful */
    return SUCCESS;
}

/*!
    \brief This function initializes the application variables

    \param[in]  None

    \return     0 on success, negative error-code on error
*/
static _i32 initializeAppVariables()
{
    g_Status = 0;
    pal_Memset(uBuf.BsdBuf, 0, sizeof(uBuf));

    //for AP mode
    g_PingPacketsRecv = 0;
    g_StationIP = 0;
    g_GatewayIP = 0;

    return SUCCESS;
}

/*!
    \brief This function displays the application's banner

    \param      None

    \return     None
*/
static void displayBanner()
{
    CLI_Write("\n\r\n\r");
    CLI_Write(" AP mode and TCP socket application - Version ");
    CLI_Write(APPLICATION_VERSION);
    CLI_Write("\n\r*******************************************************************************\n\r");
}
