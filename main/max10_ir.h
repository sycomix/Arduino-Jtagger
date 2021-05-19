/** @file ir.h
 * 
 * @brief IR instructions of Intel's implementation of MAX10 FPGA
 *
 */ 

#define PULSE_NCONFIG       0x1
#define PRELOAD_SAMPLE      0x5
#define IDCODE              0x6
#define USERCODE            0x7
#define CLAMP               0xa
#define HIGHZ               0xb
#define EXTEST              0xf

#define USER0               0xc
#define CONFIG_IO           0xd
#define USER1               0xe
#define CHANGE_EDREG        0x15

#define BGP_DISABLE         0x166
#define BGP_ENABLE          0x199

#define ISC_DISABLE         0x201
#define ISC_ADDRESS_SHIFT   0x203
#define ISC_READ            0x205
#define ISC_NOOP            0x210
#define ISC_ENABLE          0x2cc
#define ISC_ERASE           0x2f2
#define ISC_PROGRAM         0x2f4

#define LOCK                0x202
#define UNLOCK              0x208

#define PRIVATE_1           0x240
#define PRIVATE_2           0x230
#define PRIVATE_3           0x2e0
#define PRIVATE_4           0x231

#define DSM_VERIFY          0x307
#define DSM_ICB_PROGRAM     0x3f4
#define DSM_CLEAR           0x3f2
#define BYPASS              0x3ff

// #define NEW_1
// #define NEW_2
// #define NEW_3