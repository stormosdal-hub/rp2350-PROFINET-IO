/*
 * options.h - p-net's build options. Upstream generates this from options.h.in with CMake; here
 * it is written out with rt-labs' defaults (p-net/CMakeLists.txt), the log level from CMake.
 */
#ifndef OPTIONS_H
#define OPTIONS_H

// PNET_LOG_LEVEL: set by lib/pnet/CMakeLists.txt (cache variable PNET_LOG_LEVEL)
#ifndef PNET_LOG_LEVEL
#define PNET_LOG_LEVEL 1   // 0 debug, 1 info, 2 warning, 3 error, 4 fatal
#endif

#ifndef PNET_OPTION_FAST_STARTUP
#define PNET_OPTION_FAST_STARTUP 1
#endif
#ifndef PNET_OPTION_PARAMETER_SERVER
#define PNET_OPTION_PARAMETER_SERVER 1
#endif
#ifndef PNET_OPTION_IR
#define PNET_OPTION_IR 1
#endif
#ifndef PNET_OPTION_SR
#define PNET_OPTION_SR 1
#endif
#ifndef PNET_OPTION_REDUNDANCY
#define PNET_OPTION_REDUNDANCY 1
#endif
#ifndef PNET_OPTION_AR_VENDOR_BLOCKS
#define PNET_OPTION_AR_VENDOR_BLOCKS 1
#endif
#ifndef PNET_OPTION_RS
#define PNET_OPTION_RS 1
#endif
#ifndef PNET_OPTION_MC_CR
#define PNET_OPTION_MC_CR 1
#endif
#ifndef PNET_OPTION_SRL
#define PNET_OPTION_SRL 0
#endif
#ifndef PNET_OPTION_SNMP
#define PNET_OPTION_SNMP 0   // Conformance Class B needs an SNMP agent; class A (the S7-1200) does not
#endif
#ifndef PNET_USE_ATOMICS
#define PNET_USE_ATOMICS 0
#endif

#ifndef PNET_MAX_AR
#define PNET_MAX_AR 2
#endif
#ifndef PNET_MAX_API
#define PNET_MAX_API 1
#endif
#ifndef PNET_MAX_CR
#define PNET_MAX_CR 2
#endif
#ifndef PNET_MAX_SLOTS
#define PNET_MAX_SLOTS 5   // the DAP and 4 I/O modules
#endif
#ifndef PNET_MAX_SUBSLOTS
#define PNET_MAX_SUBSLOTS 3   // the DAP needs 2 + PNET_MAX_PHYSICAL_PORTS
#endif
#ifndef PNET_MAX_DFP_IOCR
#define PNET_MAX_DFP_IOCR 2
#endif
#ifndef PNET_MAX_LOG_BOOK_ENTRIES
#define PNET_MAX_LOG_BOOK_ENTRIES 16
#endif
#ifndef PNET_MAX_ALARMS
#define PNET_MAX_ALARMS 6
#endif
#ifndef PNET_MAX_ALARM_PAYLOAD_DATA_SIZE
#define PNET_MAX_ALARM_PAYLOAD_DATA_SIZE 28
#endif
#ifndef PNET_MAX_DIAG_ITEMS
#define PNET_MAX_DIAG_ITEMS 200
#endif
#ifndef PNET_MAX_DIAG_MANUF_DATA_SIZE
#define PNET_MAX_DIAG_MANUF_DATA_SIZE 16
#endif

#if PNET_OPTION_MC_CR
#ifndef PNET_MAX_MC_CR
#define PNET_MAX_MC_CR 1
#endif
#endif

#if PNET_OPTION_AR_VENDOR_BLOCKS
#ifndef PNET_MAX_AR_VENDOR_BLOCKS
#define PNET_MAX_AR_VENDOR_BLOCKS 1
#endif
#ifndef PNET_MAX_AR_VENDOR_BLOCK_DATA_LENGTH
#define PNET_MAX_AR_VENDOR_BLOCK_DATA_LENGTH 512
#endif
#endif

#ifndef PNET_MAX_MAN_SPECIFIC_FAST_STARTUP_DATA_LENGTH
#define PNET_MAX_MAN_SPECIFIC_FAST_STARTUP_DATA_LENGTH 0
#endif
#ifndef PNET_MAX_SESSION_BUFFER_SIZE
#define PNET_MAX_SESSION_BUFFER_SIZE 4500
#endif
#ifndef PNET_MAX_FILENAME_SIZE
#define PNET_MAX_FILENAME_SIZE 30
#endif
#ifndef PNET_MAX_PORT_DESCRIPTION_SIZE
#define PNET_MAX_PORT_DESCRIPTION_SIZE 60
#endif

// logging: LOG_LEVEL_x and LOG_STATE_x come from osal_log.h
#ifndef LOG_LEVEL
#define LOG_LEVEL PNET_LOG_LEVEL
#endif
#ifndef PF_ETH_LOG
#define PF_ETH_LOG LOG_STATE_ON
#endif
#ifndef PF_LLDP_LOG
#define PF_LLDP_LOG LOG_STATE_ON
#endif
#ifndef PF_SNMP_LOG
#define PF_SNMP_LOG LOG_STATE_ON
#endif
#ifndef PF_CPM_LOG
#define PF_CPM_LOG LOG_STATE_ON
#endif
#ifndef PF_PPM_LOG
#define PF_PPM_LOG LOG_STATE_ON
#endif
#ifndef PF_DCP_LOG
#define PF_DCP_LOG LOG_STATE_ON
#endif
#ifndef PF_RPC_LOG
#define PF_RPC_LOG LOG_STATE_ON
#endif
#ifndef PF_ALARM_LOG
#define PF_ALARM_LOG LOG_STATE_ON
#endif
#ifndef PF_AL_BUF_LOG
#define PF_AL_BUF_LOG LOG_STATE_ON
#endif
#ifndef PF_PNAL_LOG
#define PF_PNAL_LOG LOG_STATE_ON
#endif
#ifndef PNET_LOG
#define PNET_LOG LOG_STATE_ON
#endif

#endif /* OPTIONS_H */
