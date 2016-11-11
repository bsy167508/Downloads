/*
 *  ======== client.c ========
 *
 * TCP/IP Network Client example ported to use BIOS6 OS.
 *
 * Copyright (C) 2007, 2011 Texas Instruments Incorporated - http://www.ti.com/
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

#include <stdio.h>
#include <ti/ndk/inc/netmain.h>
#include <ti/ndk/inc/_stack.h>
#include <ti/ndk/inc/tools/console.h>
#include <ti/ndk/inc/tools/servers.h>

#include "client.h"

/* BIOS6 include */
#include <ti/sysbios/BIOS.h>

/* Platform utilities include */
#include "ti/platform/platform.h"

/* Resource manager for QMSS, PA, CPPI */
#include "ti/platform/resource_mgr.h"

// When USE_OLD_SERVERS set to zero, server daemon is used
#define USE_OLD_SERVERS 0

//---------------------------------------------------------------------------
// Title String
//
char *VerStr = "\nTCP/IP Stack Example Client\n";

// Our NETCTRL callback functions
static void   NetworkOpen();
static void   NetworkClose();
static void   NetworkIPAddr( IPN IPAddr, uint IfIdx, uint fAdd );

// Fun reporting function
static void   ServiceReport( uint Item, uint Status, uint Report, HANDLE hCfgEntry );

/* Platform Information - we will read it form the Platform Library */
platform_info	gPlatformInfo;


//---------------------------------------------------------------------------
// Configuration
//

char *HostName    = "tidsp";
char *LocalIPAddr = "192.168.2.100";    
char *LocalIPMask = "255.255.255.0";    // Not used when using DHCP
char *GatewayIP   = "192.168.2.101";    // Not used when using DHCP
char *DomainName  = "demo.net";         // Not used when using DHCP
char *DNSServer   = "0.0.0.0";          // Used when set to anything but zero


// Simulator EMAC Switch does not handle ALE_LEARN mode, so please configure the
// MAC address of the PC where you want to launch the webpages and initiate PING to NDK */

Uint8 clientMACAddress [6] = {0x00, 0x15, 0xE9, 0x85, 0x8A, 0x0A}; /* MAC address for my PC */

UINT8 DHCP_OPTIONS[] = { DHCPOPT_SERVER_IDENTIFIER, DHCPOPT_ROUTER };

#undef    	TEST_RAW_NC
#undef    	TEST_RAW_SEND
#undef    	TEST_RAW_RECV

#define     PACKET_SIZE     1000
#define     PACKET_COUNT    1

#ifdef      TEST_RAW_SEND
static HANDLE hSendRaw = 0;
static void SendRawEth();
#endif

#ifdef      TEST_RAW_RECV
static HANDLE hRecvRaw = 0;
static void RecvRawEth();
#endif

/*************************************************************************
 *  @b EVM_init()
 * 
 *  @n
 * 		
 *  Initializes the platform hardware. This routine is configured to start in 
 * 	the evm.cfg configuration file. It is the first routine that BIOS 
 * 	calls and is executed before Main is called. If you are debugging within
 *  CCS the default option in your target configuration file may be to execute 
 *  all code up until Main as the image loads. To debug this you should disable
 *  that option. 
 *
 *  @param[in]  None
 * 
 *  @retval
 *      None
 ************************************************************************/
void EVM_init()
{
	platform_init_flags  	sFlags;
	platform_init_config 	sConfig;
	/* Status of the call to initialize the platform */
	int32_t pform_status;

	/* 
	 * You can choose what to initialize on the platform by setting the following 
	 * flags. Things like the DDR, PLL, etc should have been set by the boot loader.
	*/
	memset( (void *) &sFlags,  0, sizeof(platform_init_flags));
	memset( (void *) &sConfig, 0, sizeof(platform_init_config));

	sFlags.pll  = 0;	/* PLLs for clocking  	*/
	sFlags.ddr  = 0;   	/* External memory 		*/
    sFlags.tcsl = 1;	/* Time stamp counter 	*/
    sFlags.phy  = 1;	/* Ethernet 			*/
  	sFlags.ecc  = 0;	/* Memory ECC 			*/

    sConfig.pllm = 0;	/* Use libraries default clock divisor */

	pform_status = platform_init(&sFlags, &sConfig);

	/* If we initialized the platform okay */
	if (pform_status != Platform_EOK) {
		/* Initialization of the platform failed... die */
		while (1) {
			(void) platform_led(1, PLATFORM_LED_ON, PLATFORM_USER_LED_CLASS);
			(void) platform_delay(50000);
			(void) platform_led(1, PLATFORM_LED_OFF, PLATFORM_USER_LED_CLASS);
			(void) platform_delay(50000);
		}
	}
}

//---------------------------------------------------------------------
// Main Entry Point
//---------------------------------------------------------------------
int main()
{
    EVM_init();

    /* Start the BIOS 6 Scheduler */
	BIOS_start ();
}

//
// Main Thread
//
//
int StackTest()
{
    int               rc;
    int				  i;
    HANDLE            hCfg;
    CI_SERVICE_TELNET telnet;
    CI_SERVICE_HTTP   http;
    QMSS_CFG_T      qmss_cfg;
    CPPI_CFG_T      cppi_cfg;

	/* Get information about the platform so we can use it in various places */
	memset( (void *) &gPlatformInfo, 0, sizeof(platform_info));
	(void) platform_get_info(&gPlatformInfo);

	(void) platform_uart_init();
	(void) platform_uart_set_baudrate(115200);
	(void) platform_write_configure(PLATFORM_WRITE_ALL);

	/* Clear the state of the User LEDs to OFF */
	for (i=0; i < gPlatformInfo.led[PLATFORM_USER_LED_CLASS].count; i++) {
		(void) platform_led(i, PLATFORM_LED_OFF, PLATFORM_USER_LED_CLASS);
	}

    /* Initialize the components required to run this application:
     *  (1) QMSS
     *  (2) CPPI
     *  (3) Packet Accelerator
     */
    /* Initialize QMSS */
    if (platform_get_coreid() == 0)
    {
        qmss_cfg.master_core        = 1;
    }
    else
    {
        qmss_cfg.master_core        = 0;
    }
    qmss_cfg.max_num_desc       = MAX_NUM_DESC;
    qmss_cfg.desc_size          = MAX_DESC_SIZE;
    qmss_cfg.mem_region         = Qmss_MemRegion_MEMORY_REGION0;
    if (res_mgr_init_qmss (&qmss_cfg) != 0)
    {
        platform_write ("Failed to initialize the QMSS subsystem \n");
        goto main_exit;
    }
    else
    {
    	platform_write ("QMSS successfully initialized \n");
    }

    /* Initialize CPPI */
    if (platform_get_coreid() == 0)
    {
        cppi_cfg.master_core        = 1;
    }
    else
    {
        cppi_cfg.master_core        = 0;
    }
    cppi_cfg.dma_num            = Cppi_CpDma_PASS_CPDMA;
    cppi_cfg.num_tx_queues      = NUM_PA_TX_QUEUES;
    cppi_cfg.num_rx_channels    = NUM_PA_RX_CHANNELS;
    if (res_mgr_init_cppi (&cppi_cfg) != 0)
    {
        platform_write ("Failed to initialize CPPI subsystem \n");
        goto main_exit;
    }
    else
    {
    	platform_write ("CPPI successfully initialized \n");
    }


    if (res_mgr_init_pass()!= 0) {
        platform_write ("Failed to initialize the Packet Accelerator \n");
        goto main_exit;
    }
    else
    {
    	platform_write ("PA successfully initialized \n");
    }

    //
    // THIS MUST BE THE ABSOLUTE FIRST THING DONE IN AN APPLICATION before
    //  using the stack!!
    //
    rc = NC_SystemOpen( NC_PRIORITY_LOW, NC_OPMODE_INTERRUPT );
    if( rc )
    {
        platform_write("NC_SystemOpen Failed (%d)\n",rc);
        for(;;);
    }

    // Print out our banner
    platform_write(VerStr);

    //
    // Create and build the system configuration from scratch.
    //

    // Create a new configuration
    hCfg = CfgNew();
    if( !hCfg )
    {
        platform_write("Unable to create configuration\n");
        goto main_exit;
    }

    // We better validate the length of the supplied names
    if( strlen( DomainName ) >= CFG_DOMAIN_MAX ||
        strlen( HostName ) >= CFG_HOSTNAME_MAX )
    {
        platform_write("Names too long\n");
        goto main_exit;
    }

    // Add our global hostname to hCfg (to be claimed in all connected domains)
    CfgAddEntry( hCfg, CFGTAG_SYSINFO, CFGITEM_DHCP_HOSTNAME, 0,
                 strlen(HostName), (UINT8 *)HostName, 0 );

    // If the IP address is specified, manually configure IP and Gateway
    if (!platform_get_switch_state(1))
    {
        CI_IPNET NA;
        CI_ROUTE RT;
        IPN      IPTmp;

        // Setup manual IP address
        bzero( &NA, sizeof(NA) );
        NA.IPAddr  = inet_addr(LocalIPAddr);
        NA.IPMask  = inet_addr(LocalIPMask);
        strcpy( NA.Domain, DomainName );
        NA.NetType = 0;

        // Add the address to interface 0
        CfgAddEntry( hCfg, CFGTAG_IPNET, 0, 0,
                           sizeof(CI_IPNET), (UINT8 *)&NA, 0 );

        // Add the default gateway. Since it is the default, the
        // destination address and mask are both zero (we go ahead
        // and show the assignment for clarity).
        bzero( &RT, sizeof(RT) );
        RT.IPDestAddr = 0;
        RT.IPDestMask = 0;
        RT.IPGateAddr = inet_addr(GatewayIP);

        // Add the route
        CfgAddEntry( hCfg, CFGTAG_ROUTE, 0, 0,
                           sizeof(CI_ROUTE), (UINT8 *)&RT, 0 );

        // Manually add the DNS server when specified
        IPTmp = inet_addr(DNSServer);
        if( IPTmp )
            CfgAddEntry( hCfg, CFGTAG_SYSINFO, CFGITEM_DHCP_DOMAINNAMESERVER,
                         0, sizeof(IPTmp), (UINT8 *)&IPTmp, 0 );
    }
    // Else we specify DHCP
    else
    {
        CI_SERVICE_DHCPC dhcpc;

        platform_write("Configuring DHCP client\n");

        // Specify DHCP Service on IF-1
        bzero( &dhcpc, sizeof(dhcpc) );
        dhcpc.cisargs.Mode   = CIS_FLG_IFIDXVALID;
        dhcpc.cisargs.IfIdx  = 0;  // emac0
        dhcpc.cisargs.pCbSrv = &ServiceReport;
        dhcpc.param.pOptions = DHCP_OPTIONS;
        dhcpc.param.len = 2;

        CfgAddEntry( hCfg, CFGTAG_SERVICE, CFGITEM_SERVICE_DHCPCLIENT, 0,
                     sizeof(dhcpc), (UINT8 *)&dhcpc, 0 );
    }

    // Specify TELNET service for our Console example
    bzero( &telnet, sizeof(telnet) );
    telnet.cisargs.IPAddr = INADDR_ANY;
    telnet.cisargs.pCbSrv = &ServiceReport;
    telnet.param.MaxCon   = 2;
    telnet.param.Callback = &ConsoleOpen;
    CfgAddEntry( hCfg, CFGTAG_SERVICE, CFGITEM_SERVICE_TELNET, 0,
                 sizeof(telnet), (UINT8 *)&telnet, 0 );

    // Create RAM based WEB files for HTTP
    AddWebFiles();

    // HTTP Authentication
    {
        CI_ACCT CA;

        // Name our authentication group for HTTP (Max size = 31)
        // This is the authentication "realm" name returned by the HTTP
        // server when authentication is required on group 1.
        CfgAddEntry( hCfg, CFGTAG_SYSINFO, CFGITEM_SYSINFO_REALM1,
                     0, 30, (UINT8 *)"DSP_CLIENT_DEMO_AUTHENTICATE1", 0 );

        // Create a sample user account who is a member of realm 1.
        // The username and password are just "username" and "password"
        strcpy( CA.Username, "username" );
        strcpy( CA.Password, "password" );
        CA.Flags = CFG_ACCTFLG_CH1;  // Make a member of realm 1
        rc = CfgAddEntry( hCfg, CFGTAG_ACCT, CFGITEM_ACCT_REALM,
                          0, sizeof(CI_ACCT), (UINT8 *)&CA, 0 );
    }

    // Specify HTTP service
    bzero( &http, sizeof(http) );
    http.cisargs.IPAddr = INADDR_ANY;
    http.cisargs.pCbSrv = &ServiceReport;
    CfgAddEntry( hCfg, CFGTAG_SERVICE, CFGITEM_SERVICE_HTTP, 0,
                 sizeof(http), (UINT8 *)&http, 0 );

    //
    // Configure IPStack/OS Options
    //

    // We don't want to see debug messages less than WARNINGS
    rc = DBG_INFO;
    CfgAddEntry( hCfg, CFGTAG_OS, CFGITEM_OS_DBGPRINTLEVEL,
                 CFG_ADDMODE_UNIQUE, sizeof(uint), (UINT8 *)&rc, 0 );

    //
    // This code sets up the TCP and UDP buffer sizes
    // (Note 8192 is actually the default. This code is here to
    // illustrate how the buffer and limit sizes are configured.)
    //

    // TCP Transmit buffer size
    rc = 8192;
    CfgAddEntry( hCfg, CFGTAG_IP, CFGITEM_IP_SOCKTCPTXBUF,
                 CFG_ADDMODE_UNIQUE, sizeof(uint), (UINT8 *)&rc, 0 );

    // TCP Receive buffer size (copy mode)
    rc = 8192;
    CfgAddEntry( hCfg, CFGTAG_IP, CFGITEM_IP_SOCKTCPRXBUF,
                 CFG_ADDMODE_UNIQUE, sizeof(uint), (UINT8 *)&rc, 0 );

    // TCP Receive limit (non-copy mode)
    rc = 8192;
    CfgAddEntry( hCfg, CFGTAG_IP, CFGITEM_IP_SOCKTCPRXLIMIT,
                 CFG_ADDMODE_UNIQUE, sizeof(uint), (UINT8 *)&rc, 0 );

    // UDP Receive limit
    rc = 8192;
    CfgAddEntry( hCfg, CFGTAG_IP, CFGITEM_IP_SOCKUDPRXLIMIT,
                 CFG_ADDMODE_UNIQUE, sizeof(uint), (UINT8 *)&rc, 0 );

#if 0
    // TCP Keep Idle (10 seconds)
    rc = 100;
    //   This is the time a connection is idle before TCP will probe
    CfgAddEntry( hCfg, CFGTAG_IP, CFGITEM_IP_TCPKEEPIDLE,
                 CFG_ADDMODE_UNIQUE, sizeof(uint), (UINT8 *)&rc, 0 );

    // TCP Keep Interval (1 second)
    //   This is the time between TCP KEEP probes
    rc = 10;
    CfgAddEntry( hCfg, CFGTAG_IP, CFGITEM_IP_TCPKEEPINTVL,
                 CFG_ADDMODE_UNIQUE, sizeof(uint), (UINT8 *)&rc, 0 );

    // TCP Max Keep Idle (5 seconds)
    //   This is the TCP KEEP will probe before dropping the connection
    rc = 50;
    CfgAddEntry( hCfg, CFGTAG_IP, CFGITEM_IP_TCPKEEPMAXIDLE,
                 CFG_ADDMODE_UNIQUE, sizeof(uint), (UINT8 *)&rc, 0 );
#endif

    //
    // Boot the system using this configuration
    //
    // We keep booting until the function returns 0. This allows
    // us to have a "reboot" command.
    //
    do
    {
        rc = NC_NetStart( hCfg, NetworkOpen, NetworkClose, NetworkIPAddr );
    } while( rc > 0 );

    // Free the WEB files
    RemoveWebFiles();

    // Delete Configuration
    CfgFree( hCfg );

    // Close the OS
main_exit:
    NC_SystemClose();
    return(0);
}




//
// System Task Code [ Server Daemon Servers ]
//
static HANDLE hEcho=0,hEchoUdp=0,hData=0,hNull=0,hOob=0;

#ifdef _INCLUDE_IPv6_CODE
static HANDLE hEcho6=0, hEchoUdp6=0, hTelnet6=0, hOob6=0, hWeb6=0;
#endif

//
// NetworkOpen
//
// This function is called after the configuration has booted
//
static void NetworkOpen()
{
    // Create our local servers
    hEcho = DaemonNew( SOCK_STREAMNC, 0, 7, dtask_tcp_echo,
                       OS_TASKPRINORM, OS_TASKSTKNORM, 0, 3 );
    hEchoUdp = DaemonNew( SOCK_DGRAM, 0, 7, dtask_udp_echo,
                          OS_TASKPRINORM, OS_TASKSTKNORM, 0, 1 );
    hData = DaemonNew( SOCK_STREAM, 0, 1000, dtask_tcp_datasrv,
                       OS_TASKPRINORM, OS_TASKSTKNORM, 0, 3 );
    hNull = DaemonNew( SOCK_STREAMNC, 0, 1001, dtask_tcp_nullsrv,
                       OS_TASKPRINORM, OS_TASKSTKNORM, 0, 3 );
    hOob  = DaemonNew( SOCK_STREAMNC, 0, 999, dtask_tcp_oobsrv,
                       OS_TASKPRINORM, OS_TASKSTKNORM, 0, 3 );

    // Create the IPv6 Local Servers.
#ifdef _INCLUDE_IPv6_CODE
    hEcho6    = Daemon6New (SOCK_STREAM, IPV6_UNSPECIFIED_ADDRESS, 7, dtask_tcp_echo6,
                            OS_TASKPRINORM, OS_TASKSTKNORM, 0, 3 );
    hEchoUdp6 = Daemon6New (SOCK_DGRAM, IPV6_UNSPECIFIED_ADDRESS, 7, dtask_udp_echo6,
                            OS_TASKPRINORM, OS_TASKSTKNORM, 0, 1 );
    hTelnet6  = Daemon6New (SOCK_STREAM, IPV6_UNSPECIFIED_ADDRESS, 23, 
                            (int(*)(SOCKET,UINT32))telnetClientProcess, OS_TASKPRINORM, OS_TASKSTKLOW,
                            (UINT32)ConsoleOpen, 2 );
    hOob6     = Daemon6New (SOCK_STREAM, IPV6_UNSPECIFIED_ADDRESS, 999, dtask_tcp_oobsrv,
                            OS_TASKPRINORM, OS_TASKSTKNORM, 0, 3 );
    hWeb6     = Daemon6New (SOCK_STREAM, IPV6_UNSPECIFIED_ADDRESS, HTTPPORT, httpClientProcess,
                            OS_TASKPRINORM, OS_TASKSTKHIGH, 0, 4);
#endif
}

//
// NetworkClose
//
// This function is called when the network is shutting down,
// or when it no longer has any IP addresses assigned to it.
//
static void NetworkClose()
{
    DaemonFree( hOob );
    DaemonFree( hNull );
    DaemonFree( hData );
    DaemonFree( hEchoUdp );
    DaemonFree( hEcho );

#ifdef _INCLUDE_IPv6_CODE
    Daemon6Free (hEcho6);
    Daemon6Free (hEchoUdp6);
    Daemon6Free (hTelnet6);
    Daemon6Free (hOob6);
    Daemon6Free (hWeb6);
#endif

#ifdef  TEST_RAW_SEND
    TaskDestroy (hSendRaw);
#endif
#ifdef  TEST_RAW_RECV
    TaskDestroy (hRecvRaw);
#endif

    // Kill any active console
    ConsoleClose();
}


//
// NetworkIPAddr
//
// This function is called whenever an IP address binding is
// added or removed from the system.
//
static void NetworkIPAddr( IPN IPAddr, uint IfIdx, uint fAdd )
{
    static uint fAddGroups = 0;
    IPN IPTmp;

    if( fAdd )
        platform_write("Network Added: ");
    else
        platform_write("Network Removed: ");

    // Print a message
    IPTmp = ntohl( IPAddr );
    platform_write("If-%d:%d.%d.%d.%d\n", IfIdx,
            (UINT8)(IPTmp>>24)&0xFF, (UINT8)(IPTmp>>16)&0xFF,
            (UINT8)(IPTmp>>8)&0xFF, (UINT8)IPTmp&0xFF );

    // This is a good time to join any multicast group we require
    if( fAdd && !fAddGroups )
    {
        fAddGroups = 1;
//              IGMPJoinHostGroup( inet_addr("224.1.2.3"), IfIdx );
    }

    /* Create a Task to send/receive Raw ethernet traffic */
#ifdef  TEST_RAW_SEND
    hSendRaw = TaskCreate( SendRawEth, "TxRawEthTsk", OS_TASKPRINORM, 0x1400, 0, 0, 0 );
#endif
#ifdef  TEST_RAW_RECV
    hRecvRaw = TaskCreate( RecvRawEth, "PerformRawRX", OS_TASKPRIHIGH, 0x1400, 0, 0, 0 );
#endif    
}

//
// DHCP_reset()
//
// Code to reset DHCP client by removing it from the active config,
// and then reinstalling it.
//
// Called with:
// IfIdx    set to the interface (1-n) that is using DHCP.
// fOwnTask set when called on a new task thread (via TaskCreate()).
//
void DHCP_reset( uint IfIdx, uint fOwnTask )
{
    CI_SERVICE_DHCPC dhcpc;
    HANDLE h;
    int    rc,tmp;
    uint   idx;

    // If we were called from a newly created task thread, allow
    // the entity that created us to complete
    if( fOwnTask )
        TaskSleep(500);

    // Find DHCP on the supplied interface
    for(idx=1; ; idx++)
    {
        // Find a DHCP entry
        rc = CfgGetEntry( 0, CFGTAG_SERVICE, CFGITEM_SERVICE_DHCPCLIENT,
                          idx, &h );
        if( rc != 1 )
            goto RESET_EXIT;

        // Get DHCP entry data
        tmp = sizeof(dhcpc);
        rc = CfgEntryGetData( h, &tmp, (UINT8 *)&dhcpc );

        // If not the right entry, continue
        if( (rc<=0) || dhcpc.cisargs.IfIdx != IfIdx )
        {
            CfgEntryDeRef(h);
            h = 0;
            continue;
        }

        // This is the entry we want!

        // Remove the current DHCP service
        CfgRemoveEntry( 0, h );

        // Specify DHCP Service on specified IF
        bzero( &dhcpc, sizeof(dhcpc) );
        dhcpc.cisargs.Mode   = CIS_FLG_IFIDXVALID;
        dhcpc.cisargs.IfIdx  = IfIdx;
        dhcpc.cisargs.pCbSrv = &ServiceReport;
        CfgAddEntry( 0, CFGTAG_SERVICE, CFGITEM_SERVICE_DHCPCLIENT, 0,
                     sizeof(dhcpc), (UINT8 *)&dhcpc, 0 );
        break;
    }

RESET_EXIT:
    // If we are a function, return, otherwise, call TaskExit()
    if( fOwnTask )
        TaskExit();
}


void CheckDHCPOptions();

//
// Service Status Reports
//
// Here's a quick example of using service status updates
//
static char *TaskName[]  = { "Telnet","HTTP","NAT","DHCPS","DHCPC","DNS" };
static char *ReportStr[] = { "","Running","Updated","Complete","Fault" };
static char *StatusStr[] = { "Disabled","Waiting","IPTerm","Failed","Enabled" };
static void ServiceReport( uint Item, uint Status, uint Report, HANDLE h )
{
    platform_write( "Service Status: %-9s: %-9s: %-9s: %03d\n",
            TaskName[Item-1], StatusStr[Status],
            ReportStr[Report/256], Report&0xFF );

    //
    // Example of adding to the DHCP configuration space
    //
    // When using the DHCP client, the client has full control over access
    // to the first 256 entries in the CFGTAG_SYSINFO space.
    //
    // Note that the DHCP client will erase all CFGTAG_SYSINFO tags except
    // CFGITEM_DHCP_HOSTNAME. If the application needs to keep manual
    // entries in the DHCP tag range, then the code to maintain them should
    // be placed here.
    //
    // Here, we want to manually add a DNS server to the configuration, but
    // we can only do it once DHCP has finished its programming.
    //
    if( Item == CFGITEM_SERVICE_DHCPCLIENT &&
        Status == CIS_SRV_STATUS_ENABLED &&
        (Report == (NETTOOLS_STAT_RUNNING|DHCPCODE_IPADD) ||
         Report == (NETTOOLS_STAT_RUNNING|DHCPCODE_IPRENEW)) )
    {
        IPN IPTmp;

        // Manually add the DNS server when specified
        IPTmp = inet_addr(DNSServer);
        if( IPTmp )
            CfgAddEntry( 0, CFGTAG_SYSINFO, CFGITEM_DHCP_DOMAINNAMESERVER,
                         0, sizeof(IPTmp), (UINT8 *)&IPTmp, 0 );
#if 0        
        // We can now check on what the DHCP server supplied in 
        // response to our DHCP option tags.
        CheckDHCPOptions();   
#endif
                     
    }

    // Reset DHCP client service on failure
    if( Item==CFGITEM_SERVICE_DHCPCLIENT && (Report&~0xFF)==NETTOOLS_STAT_FAULT )
    {
        CI_SERVICE_DHCPC dhcpc;
        int tmp;

        // Get DHCP entry data (for index to pass to DHCP_reset).
        tmp = sizeof(dhcpc);
        CfgEntryGetData( h, &tmp, (UINT8 *)&dhcpc );

        // Create the task to reset DHCP on its designated IF
        // We must use TaskCreate instead of just calling the function as
        // we are in a callback function.
        TaskCreate( DHCP_reset, "DHCPreset", OS_TASKPRINORM, 0x1000,
                    dhcpc.cisargs.IfIdx, 1, 0 );
    }
}

void CheckDHCPOptions()
{
    char IPString[16];
    IPN  IPAddr;
    int  i, rc;

    // Now scan for DHCPOPT_SERVER_IDENTIFIER via configuration
    platform_write("\nDHCP Server ID:\n");
    for(i=1;;i++)
    {
        // Try and get a DNS server
        rc = CfgGetImmediate( 0, CFGTAG_SYSINFO, DHCPOPT_SERVER_IDENTIFIER,
                              i, 4, (UINT8 *)&IPAddr );
        if( rc != 4 )
            break;

        // We got something

        // Convert IP to a string:
        NtIPN2Str( IPAddr, IPString );
        platform_write("DHCP Server %d = '%s'\n", i, IPString);
    }
    if( i==1 )
        platform_write("None\n\n");
    else
        platform_write("\n");

    // Now scan for DHCPOPT_ROUTER via the configuration
    platform_write("Router Information:\n");
    for(i=1;;i++)
    {
        // Try and get a DNS server
        rc = CfgGetImmediate( 0, CFGTAG_SYSINFO, DHCPOPT_ROUTER,
                              i, 4, (UINT8 *)&IPAddr );
        if( rc != 4 )
            break;

        // We got something

        // Convert IP to a string:
        NtIPN2Str( IPAddr, IPString );
        platform_write("Router %d = '%s'\n", i, IPString);
    }
    if( i==1 )
        platform_write("None\n\n");
    else
        platform_write("\n");
}

#ifdef TEST_RAW_SEND
/* Routine to demonstrate sending raw ethernet packets using
 * send() / sendnc() APIs and AF_RAWETH family socket.
 */
static void SendRawEth()
{
    char*       pBuffer = NULL;
    ETHHDR*     ptr_eth_header;
    UINT32      rawether_type = 0x300, rawchannel_num = 0;
    UINT8       src_mac[6], dst_mac[6], bData[20];
    int         i, j, val, bytes, retVal;
    SOCKET      sraw = INVALID_SOCKET;
#ifdef TEST_RAW_NC
    PBM_Handle  hPkt = NULL;
#endif

    /* Allocate the file environment for this task */
    fdOpenSession( TaskSelf() );

    /* wait for the ethernet link to come up. */
	TaskSleep(20000);

    platform_write("Raw Eth Task Started ... \n");

    /* Demonstrating use of SO_PRIORITY to configure
     * custom properties for all packets travelling
     * using this socket.
     *
     * Here, in this example we will use it for 
     * configuring a distinct EMAC channel number for each
     * of the raw ethernet sockets.
     *
     * For example, Channel Assignment:
     *      Chan 0 - IP
     *      Chan 3 - Raw
     */
    rawchannel_num = 3;

    /* Create the raw ethernet socket */
    sraw = socket(AF_RAWETH, SOCK_RAWETH, rawether_type);
    if( sraw == INVALID_SOCKET ) 
    {
        platform_write("Fail socket, %d\n", fdError());
        fdCloseSession (TaskSelf());
        return;
    }    

    /* Configure the transmit device */
    val = 1;
    retVal = setsockopt(sraw, SOL_SOCKET, SO_IFDEVICE, &val, sizeof(val));
	if(retVal)
		platform_write("error in setsockopt \n");

    /* Configure the EMAC channel number */
    val = rawchannel_num;
    retVal = setsockopt(sraw, SOL_SOCKET, SO_PRIORITY, &val, sizeof(val));
	if(retVal)
		platform_write("error in setsockopt \n");

    /* Send the RAW eth packets out */
	for(j = 0; j < PACKET_COUNT; j++)
    {
#ifndef  TEST_RAW_NC
        if ((pBuffer = mmAlloc (sizeof(char) * PACKET_SIZE)) == NULL)
        {
            platform_write("OOM ?? \n");
            TaskExit();
        }
#else
    	if(getsendncbuff(sraw, PACKET_SIZE, (void **) &pBuffer, &hPkt))
        {
            platform_write("Error: Raw Eth getsendncbuff failed Error:%d\n", fdError());
		    fdCloseSession( TaskSelf() );
            fdClose(sraw);
		    TaskExit();
		}
#endif

	    /* Configure the Source MAC, Destination MAC, Protocol and Payload */
	    for (i = 0; i < 6; i++)
		{
	    	src_mac[i] = 0x10 + i;
		}

	    for (i = 0; i < 6; i++)
	        dst_mac[i] = 0x20 + i + 2;

	    for (i = 0; i < 20; i++)
	        bData[i] = 0x60 + i;

	    ptr_eth_header = (ETHHDR*)pBuffer;

	    /* Copy the source MAC address as is. */
	    mmCopy (ptr_eth_header->SrcMac, src_mac, 6);

	    /* Copy the destination MAC address as is. */
	    mmCopy (ptr_eth_header->DstMac, dst_mac, 6);

	    /* Configure the type in network order. */
	    ptr_eth_header->Type = HNC16(rawether_type);

	    /* Copy over the payload to the buffer */
	    mmCopy(pBuffer + ETHHDR_SIZE, bData, 20);


#ifndef TEST_RAW_NC
        /* Send the packet using the copy version of send() API. */
        bytes = send(sraw, (char *)pBuffer, PACKET_SIZE, 0);
#else
        /* Use the no-copy version of send, sendnc() to send out the data */
        bytes = sendnc(sraw, (char *)pBuffer, PACKET_SIZE, hPkt, 0);
#endif

#ifndef TEST_RAW_NC
        /* If we allocated the buffer, free it up. If we used up the sendnc()
         * API, the buffer will be freed up by the stack.
         */
	    mmFree(pBuffer);
#endif

		if( bytes < 0 ) 
        {
            platform_write("Error: Raw Eth Send failed Error:%d\n", fdError());

#ifdef TEST_RAW_NC
            sendncfree(hPkt);
#endif
            
		    fdCloseSession( TaskSelf() );
            fdClose(sraw);
		    TaskExit();
        }                 
    }

    platform_write("Raw Eth Task Ended ... \n");

    fdClose(sraw);

	/* Close the session & kill the task */
    fdCloseSession( TaskSelf() );
    TaskExit();
}
#endif

#ifdef TEST_RAW_RECV

/* Routine to demonstrate packet receive using recvnc() API 
 * and AF_RAWETH family sockets.
 */
static void RecvRawEth()
{
	SOCKET      sraw;
	INT32       val,retVal, count = 0, bytes;
    char*    	pBuf;
    HANDLE   	hBuffer;
	UINT32      rawchannel_num;
	ETHHDR*     ptr_eth_header;

	platform_write ("Raw Eth Rx Task has been started\n");

    /* Allocate the file environment for this task */
    fdOpenSession( TaskSelf() );

    /* Demonstrating use of SO_PRIORITY to configure
     * custom properties for all packets travelling
     * using this socket.
     *
     * Here, in this example we will use it for 
     * configuring a distinct EMAC channel number for each
     * of the raw ethernet sockets.
     *
     * For example, Channel Assignment:
     *      Chan 0 - IP
     *      Chan 3 - Raw
     */
    rawchannel_num = 3;

    /* Create the main UDP listen socket */
    sraw = socket(AF_RAWETH, SOCK_RAWETH, 0x300);
    if( sraw == INVALID_SOCKET )
        return;      

    /* Configure the transmit device */
    val = 1;
    retVal = setsockopt(sraw, SOL_SOCKET, SO_IFDEVICE, &val, sizeof(val));
	if(retVal)
		platform_write("error in setsockopt \n");

    /* Configure the EMAC channel number as the priority tag for packets */
    val = rawchannel_num;
    retVal = setsockopt(sraw, SOL_SOCKET, SO_PRIORITY, &val, sizeof(val));
	if(retVal)
		platform_write("error in setsockopt \n");

    /* Configure the Receive buffer size */
    val = 1000;
    retVal = setsockopt(sraw, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));
	if(retVal)
		platform_write("error in setsockopt \n");


    while (count < PACKET_COUNT) 
    {
		bytes = (int)recvnc( sraw, (void **)&pBuf, 0, &hBuffer );

        if (bytes < 0)
        {
			/* Receive failed: Close the session & kill the task */
			platform_write("Receive failed after packets Error:%d\n",fdError());
		    fdCloseSession( TaskSelf() );
		    TaskExit();
        }
		else
		{
    		ptr_eth_header = (ETHHDR*)pBuf;

		    platform_write("Received RAW ETH packet, len: %d \n", PBM_getValidLen((PBM_Handle)hBuffer));
			platform_write("Dst MAC Address = %02x-%02x-%02x-%02x-%02x-%02x Src MAC Address = %02x-%02x-%02x-%02x-%02x-%02x Eth Type = %d Data = %s \n",
           			ptr_eth_header->DstMac[0],ptr_eth_header->DstMac[1],ptr_eth_header->DstMac[2],
           			ptr_eth_header->DstMac[3],ptr_eth_header->DstMac[4],ptr_eth_header->DstMac[5],
           			ptr_eth_header->SrcMac[0],ptr_eth_header->SrcMac[1],ptr_eth_header->SrcMac[2],
           			ptr_eth_header->SrcMac[3],ptr_eth_header->SrcMac[4],ptr_eth_header->SrcMac[5],
           			ntohs(ptr_eth_header->Type), (pBuf + ETHHDR_SIZE));
		}


		count++;

		/* Clean out the buffer */
		recvncfree( hBuffer );
    }
         
	/* Close the session & kill the task */
    fdCloseSession( TaskSelf() );
    TaskExit();

    return;
}

#endif

