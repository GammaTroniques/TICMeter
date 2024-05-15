/**
 * @file cpp
 * @author Dorian Benech
 * @brief
 * @version 1.0
 * @date 2023-10-11
 *
 * @copyright Copyright (c) 2023 GammaTroniques
 *
 */

/*==============================================================================
 Local Include
===============================================================================*/
#include <linky.h>
#include <config.h>
#include <time.h>
#include <gpio.h>
#include <wifi.h>
#include "esp_random.h"
#include "esp_pm.h"
#include "led.h"
#include "tests.h"

/*==============================================================================
 Local Define
===============================================================================*/
// clang-format off
#define LINKY_BUFFER_SIZE 2048 // The size of the UART buffer
#define START_OF_FRAME  0x02 // The start of frame character
#define END_OF_FRAME    0x03   // The end of frame character

#define START_OF_GROUP  0x0A  // The start of group character
#define END_OF_GROUP    0x0D    // The end of group character


#define RX_BUF_SIZE     2048 // The size of the UART buffer
#define FRAME_COUNT     5   // The max number of frame in buffer
#define FRAME_SIZE      500   // The size of one frame buffer
#define GROUP_COUNT     50

#define TAG "LINKY"

// clang-format on
/*==============================================================================
 Local Macro
===============================================================================*/
#define ZB_RP ESP_ZB_ZCL_ATTR_ACCESS_REPORTING
#define ZB_RO ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY
#define ZB_RW ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE
#define ZB_NO 0

#define ZB_OCTSTR ESP_ZB_ZCL_ATTR_TYPE_OCTET_STRING
#define ZB_CHARSTR ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING
#define ZB_UINT8 ESP_ZB_ZCL_ATTR_TYPE_U8
#define ZB_UINT16 ESP_ZB_ZCL_ATTR_TYPE_U16
#define ZB_UINT24 ESP_ZB_ZCL_ATTR_TYPE_U24
#define ZB_UINT32 ESP_ZB_ZCL_ATTR_TYPE_U32
#define ZB_UINT48 ESP_ZB_ZCL_ATTR_TYPE_U48
#define ZB_UINT64 ESP_ZB_ZCL_ATTR_TYPE_U64
#define ZB_INT8 ESP_ZB_ZCL_ATTR_TYPE_S8
#define ZB_INT16 ESP_ZB_ZCL_ATTR_TYPE_S16
#define ZB_INT24 ESP_ZB_ZCL_ATTR_TYPE_S24
#define ZB_INT32 ESP_ZB_ZCL_ATTR_TYPE_S32
#define ZB_INT48 ESP_ZB_ZCL_ATTR_TYPE_S48
#define ZB_INT64 ESP_ZB_ZCL_ATTR_TYPE_S64
#define ZB_BOOL ESP_ZB_ZCL_ATTR_TYPE_BOOL
#define ZB_ENUM8 ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM

#define ZB_TICMETER TICMETER_CLUSTER_ID

/*==============================================================================
 Local Type
===============================================================================*/

/*==============================================================================
 Local Function Declaration
===============================================================================*/
static uint8_t linky_read();                                     // Read the UART buffer
static char linky_decode();                                      // Decode the frame
static char linky_checksum(char *label, char *data, char *time); // Check the checksum
static void linky_create_debug_frame(uint8_t mode_std);
static time_t linky_decode_time(char *time); // Decode the time
static void linky_clear_data();
/*==============================================================================
Public Variable
===============================================================================*/
// clang-format off

uint32_t linky_free_heap_size = 0;

const LinkyGroup LinkyLabelList[] =
{   
    // ID  Name                               Label         DataPtr                        Type        Size   MODE    Contract   Grid    UpdateType    HA Class      Icon                                CLUSTER ATTRIBUTE  ACCESS  ZB_TYPE
    //------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //--------------------------- MODE HISTORIQUE --------------------------------
    {101, "Identifiant",                        "ADCO",        &linky_data.hist.ADCO,         STRING,      12, MODE_HIST, C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:card-account-details",            0x0702, 0x0308,  ZB_RO, ZB_OCTSTR,   },
    {107, "Type de contrat",                    "OPTARIF",     &linky_data.hist.OPTARIF,      STRING,       4, MODE_HIST, C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:cash-multiple",                   0xFF42, 0x0000,  ZB_RO, ZB_CHARSTR,   },  
    {000, "Intensité souscrite",                "ISOUSC",      &linky_data.hist.ISOUSC,       UINT32,       0, MODE_HIST, C_ANY,   G_ANY,  STATIC_VALUE,  CURRENT,     "",                                    0x0000, 0x0000,  ZB_NO, ZB_NO,       }, //TODO: zigbee: when  Meter Identification cluster
    {102, "Puissance Max contrat",              "pref",        &linky_data.hist.PREF ,        UINT32,       0, MODE_HIST, C_ANY,   G_ANY,  STATIC_VALUE,  POWER_kVA,   "",                                    0xFF42, 0x002b,  ZB_RO, ZB_UINT16,    }, //TODO: zigbee: when  Meter Identification cluster  0x0B01, 0x000D, 

    {999, "Index Total",                        "total",       &linky_data.hist.TOTAL,        UINT64,       0, MODE_HIST,  C_ANY,  G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0000,  ZB_RP, ZB_UINT48,   },
    {999, "Index Base",                         "BASE",        &linky_data.hist.BASE,         UINT64,       0, MODE_HIST, C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0100,  ZB_RP, ZB_UINT48,   },
    {999, "Index Heures Creuses",               "HCHC",        &linky_data.hist.HCHC,         UINT64,       0, MODE_HIST, C_HC,    G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0100,  ZB_RP, ZB_UINT48,   },
    {999, "Index Heures Pleines",               "HCHP",        &linky_data.hist.HCHP,         UINT64,       0, MODE_HIST, C_HC,    G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0102,  ZB_RP, ZB_UINT48,   },
    {999, "Index Heures Normales",              "EJPHN",       &linky_data.hist.EJPHN,        UINT64,       0, MODE_HIST, C_EJP,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0100,  ZB_RP, ZB_UINT48,   },
    {999, "Index Heures de Pointe Mobile",      "EJPHPM",      &linky_data.hist.EJPHPM,       UINT64,       0, MODE_HIST, C_EJP,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0102,  ZB_RP, ZB_UINT48,   },
    {999, "Préavis Début EJP",                  "PEJP",        &linky_data.hist.PEJP,         UINT16,       0, MODE_HIST, C_EJP,   G_ANY,  STATIC_VALUE,  CLASS_BOOL,  "mdi:clock",                           0xFF42, 0x0001,  ZB_RP, ZB_UINT16,   },
    {999, "Heures Creuses Jours Bleus",         "BBRHCJB",     &linky_data.hist.BBRHCJB,      UINT64,       0, MODE_HIST, C_TEMPO, G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0100,  ZB_RP, ZB_UINT48,   },
    {999, "Heures Pleines Jours Bleus",         "BBRHPJB",     &linky_data.hist.BBRHPJB,      UINT64,       0, MODE_HIST, C_TEMPO, G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0102,  ZB_RP, ZB_UINT48,   },
    {999, "Heures Creuses Jours Blancs",        "BBRHCJW",     &linky_data.hist.BBRHCJW,      UINT64,       0, MODE_HIST, C_TEMPO, G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0104,  ZB_RP, ZB_UINT48,   },
    {999, "Heures Pleines Jours Blancs",        "BBRHPJW",     &linky_data.hist.BBRHPJW,      UINT64,       0, MODE_HIST, C_TEMPO, G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0106,  ZB_RP, ZB_UINT48,   },
    {999, "Heures Creuses Jours Rouges",        "BBRHCJR",     &linky_data.hist.BBRHCJR,      UINT64,       0, MODE_HIST, C_TEMPO, G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0108,  ZB_RP, ZB_UINT48,   },
    {999, "Heures Pleines Jours Rouges",        "BBRHPJR",     &linky_data.hist.BBRHPJR,      UINT64,       0, MODE_HIST, C_TEMPO, G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x010A,  ZB_RP, ZB_UINT48,   },

    {108, "Période tarifaire en cours",         "PTEC",        &linky_data.hist.PTEC,         STRING,       4, MODE_HIST, C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:calendar-clock",                  0xFF42, 0x0039,  ZB_RP, ZB_CHARSTR,  }, //0x0702, 0x0020
    {109, "Couleur aujourd'hui",                "aujour",      &linky_data.hist.AUJOUR ,      STRING,       9, MODE_HIST, C_TEMPO, G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0x0000, 0x0000,  ZB_NO, ZB_NO,       },
    {110, "Couleur du lendemain",               "DEMAIN",      &linky_data.hist.DEMAIN,       STRING,       9, MODE_HIST, C_TEMPO, G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0xFF42, 0x0003,  ZB_RP, ZB_OCTSTR,   },

    {126, "Intensité instantanée",              "IINST",       &linky_data.hist.IINST,        UINT16,       0, MODE_HIST, C_ANY,   G_MONO, REAL_TIME,     CURRENT,     "",                                    0x0B04, 0x0508,  ZB_RP, ZB_UINT16,   },
    {127, "Intensité instantanée Phase 1",      "IINST1",      &linky_data.hist.IINST1,       UINT16,       0, MODE_HIST, C_ANY,   G_TRI,  REAL_TIME,     CURRENT,     "",                                    0x0B04, 0x0508,  ZB_RP, ZB_UINT16,   },
    {128, "Intensité instantanée Phase 2",      "IINST2",      &linky_data.hist.IINST2,       UINT16,       0, MODE_HIST, C_ANY,   G_TRI,  REAL_TIME,     CURRENT,     "",                                    0x0B04, 0x0908,  ZB_RP, ZB_UINT16,   },
    {129, "Intensité instantanée Phase 3",      "IINST3",      &linky_data.hist.IINST3,       UINT16,       0, MODE_HIST, C_ANY,   G_TRI,  REAL_TIME,     CURRENT,     "",                                    0x0B04, 0x0A08,  ZB_RP, ZB_UINT16,   },
    {000, "Intensité maximale",                 "IMAX",        &linky_data.hist.IMAX,         UINT16,       0, MODE_HIST, C_ANY,   G_MONO, STATIC_VALUE,  CURRENT,     "",                                    0x0B04, 0x050A,  ZB_RO, ZB_UINT16,   },
    {000, "Intensité maximale Phase 1",         "IMAX1",       &linky_data.hist.IMAX1,        UINT16,       0, MODE_HIST, C_ANY,   G_TRI,  STATIC_VALUE,  CURRENT,     "",                                    0x0B04, 0x050A,  ZB_RO, ZB_UINT16,   },
    {000, "Intensité maximale Phase 2",         "IMAX2",       &linky_data.hist.IMAX2,        UINT16,       0, MODE_HIST, C_ANY,   G_TRI,  STATIC_VALUE,  CURRENT,     "",                                    0x0B04, 0x090A,  ZB_RO, ZB_UINT16,   },
    {000, "Intensité maximale Phase 3",         "IMAX3",       &linky_data.hist.IMAX3,        UINT16,       0, MODE_HIST, C_ANY,   G_TRI,  STATIC_VALUE,  CURRENT,     "",                                    0x0B04, 0x0A0A,  ZB_RO, ZB_UINT16,   },
    {000, "Dépassement Puissance",              "ADPS",        &linky_data.hist.ADPS,         UINT16,       0, MODE_HIST, C_ANY,   G_MONO, STATIC_VALUE,  CURRENT,     "",                                    0xFF42, 0x0004,  ZB_RP, ZB_UINT16,   },
    {000, "Dépassement Intensité Phase 1",      "ADIR1",       &linky_data.hist.ADIR1,        UINT16,       0, MODE_HIST, C_ANY,   G_TRI,  STATIC_VALUE,  CURRENT,     "",                                    0xFF42, 0x0005,  ZB_RP, ZB_UINT16,   },
    {000, "Dépassement Intensité Phase 2",      "ADIR2",       &linky_data.hist.ADIR2,        UINT16,       0, MODE_HIST, C_ANY,   G_TRI,  STATIC_VALUE,  CURRENT,     "",                                    0xFF42, 0x0006,  ZB_RP, ZB_UINT16,   },
    {000, "Dépassement Intensité Phase 3",      "ADIR3",       &linky_data.hist.ADIR3,        UINT16,       0, MODE_HIST, C_ANY,   G_TRI,  STATIC_VALUE,  CURRENT,     "",                                    0xFF42, 0x0007,  ZB_RP, ZB_UINT16,   },

    {122, "Puissance apparente",                "PAPP",        &linky_data.hist.PAPP,         UINT32,       0, MODE_HIST, C_ANY,   G_ANY,  REAL_TIME,     POWER_VA,    "",                                    0x0B04, 0x050F,  ZB_RP, ZB_UINT16,   },
    {135, "Puissance maximale triphasée",       "PMAX",        &linky_data.hist.PMAX,         UINT32,       0, MODE_HIST, C_ANY,   G_TRI,  STATIC_VALUE,  POWER_W,     "",                                    0x0B04, 0x050D,  ZB_RO, ZB_INT16,    },
    {000, "Présence des potentiels",            "PPOT",        &linky_data.hist.PPOT,         UINT32,       0, MODE_HIST, C_ANY,   G_TRI,  REAL_TIME,     NONE_CLASS,  "",                                    0xFF42, 0x0008,  ZB_RO, ZB_UINT32,   },

    {000, "Horaire Heures Creuses",             "HHPHC",       &linky_data.hist.HHPHC,        STRING,       3, MODE_HIST, C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:home-clock",                      0xFF42, 0x0009,  ZB_RP, ZB_OCTSTR,   },
    {000, "Mot d'état du compteur",             "MOTDETAT",    &linky_data.hist.MOTDETAT,     STRING,       6, MODE_HIST, C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:state-machine",                   0xFF42, 0x000A,  ZB_RO, ZB_OCTSTR,   },

    //------------------------ MODE STANDARD ----------------------- 
    {101, "Identifiant",                        "ADSC",        &linky_data.std.ADSC,          STRING,      12, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:card-account-details",            0x0702, 0x0308,  ZB_RO, ZB_OCTSTR,   },
    {000, "Version de la TIC",                  "VTIC",        &linky_data.std.VTIC,          STRING,       2, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:tag",                             0xFF42, 0x002e,  ZB_RO, ZB_CHARSTR,  }, //TODO: zigbee: when  Meter Identification cluster 0x0B01, 0x000A,  
    {000, "Date et heure Compteur",             "DATE",        &linky_data.std.DATE,          UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:clipboard-text-clock",            0xFF42, 0x000B,  ZB_RO, ZB_UINT64,   },
    {107, "Nom du calendrier tarifaire",        "NGTF",        &linky_data.std.NGTF,          STRING,      16, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:calendar-clock",                  0xFF42, 0x0000,  ZB_RO, ZB_OCTSTR,   },
    {108, "Libellé tarif en cours",             "LTARF",       &linky_data.std.LTARF,         STRING,      16, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:tag-text",                        0xFF42, 0x0039,  ZB_RP, ZB_CHARSTR,  },

    {999, "Index Total Energie soutirée",       "EAST",        &linky_data.std.EAST,          UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0000,  ZB_RP, ZB_UINT48,   },
    {999, "Index 1 Energie soutirée",           "EASF01",      &linky_data.std.EASF01,        UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0100,  ZB_RP, ZB_UINT48,   },
    {999, "Index 2 Energie soutirée",           "EASF02",      &linky_data.std.EASF02,        UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0102,  ZB_RP, ZB_UINT48,   },
    {999, "Index 3 Energie soutirée",           "EASF03",      &linky_data.std.EASF03,        UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0104,  ZB_RP, ZB_UINT48,   },
    {999, "Index 4 Energie soutirée",           "EASF04",      &linky_data.std.EASF04,        UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0106,  ZB_RP, ZB_UINT48,   },
    {999, "Index 5 Energie soutirée",           "EASF05",      &linky_data.std.EASF05,        UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0108,  ZB_RP, ZB_UINT48,   },
    {999, "Index 6 Energie soutirée",           "EASF06",      &linky_data.std.EASF06,        UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x010A,  ZB_RP, ZB_UINT48,   },
    {999, "Index 7 Energie soutirée",           "EASF07",      &linky_data.std.EASF07,        UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x010C,  ZB_RP, ZB_UINT48,   },
    {999, "Index 8 Energie soutirée",           "EASF08",      &linky_data.std.EASF08,        UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x010E,  ZB_RP, ZB_UINT48,   },
    {999, "Index 9 Energie soutirée",           "EASF09",      &linky_data.std.EASF09,        UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0110,  ZB_RP, ZB_UINT48,   },
    {999, "Index 10 Energie soutirée",          "EASF10",      &linky_data.std.EASF10,        UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0112,  ZB_RP, ZB_UINT48,   },

    {000, "Index 1 Energie soutirée Distr",     "EASD01",      &linky_data.std.EASD01,        UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0xFF42, 0x000E,  ZB_RP, ZB_UINT48,   },
    {000, "Index 2 Energie soutirée Distr",     "EASD02",      &linky_data.std.EASD02,        UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0xFF42, 0x000F,  ZB_RP, ZB_UINT48,   },
    {000, "Index 3 Energie soutirée Distr",     "EASD03",      &linky_data.std.EASD03,        UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0xFF42, 0x0010,  ZB_RP, ZB_UINT48,   },
    {000, "Index 4 Energie soutirée Distr",     "EASD04",      &linky_data.std.EASD04,        UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0xFF42, 0x0011,  ZB_RP, ZB_UINT48,   },

    {999, "Energie injectée totale",            "EAIT",        &linky_data.std.EAIT,          UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "mdi:transmission-tower-export",       0x0702, 0x0001,  ZB_RP, ZB_UINT48,   },

    {000, "Energie réactive Q1 totale",         "ERQ1",        &linky_data.std.ERQ1,          UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY_Q,    "mdi:lightning-bolt",                  0x0B04, 0x0305,  ZB_RP, ZB_INT16,    },
    {000, "Energie réactive Q2 totale",         "ERQ2",        &linky_data.std.ERQ2,          UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY_Q,    "mdi:lightning-bolt",                  0x0B04, 0x050E,  ZB_RP, ZB_INT16,    },
    {000, "Energie réactive Q3 totale",         "ERQ3",        &linky_data.std.ERQ3,          UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY_Q,    "mdi:lightning-bolt",                  0x0B04, 0x090E,  ZB_RP, ZB_INT16,    },
    {000, "Energie réactive Q4 totale",         "ERQ4",        &linky_data.std.ERQ4,          UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY_Q,    "mdi:lightning-bolt",                  0x0B04, 0x0A0E,  ZB_RP, ZB_INT16,    },

    {127, "Courant efficace Phase 1",           "IRMS1",       &linky_data.std.IRMS1,         UINT16,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  CURRENT,     "",                                    0x0B04, 0x0508,  ZB_RP, ZB_UINT16,   },
    {128, "Courant efficace Phase 2",           "IRMS2",       &linky_data.std.IRMS2,         UINT16,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  CURRENT,     "",                                    0x0B04, 0x0908,  ZB_RP, ZB_UINT16,   },
    {129, "Courant efficace Phase 3",           "IRMS3",       &linky_data.std.IRMS3,         UINT16,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  CURRENT,     "",                                    0x0B04, 0x0A08,  ZB_RP, ZB_UINT16,   },

    {131, "Tension efficace Phase 1",           "URMS1",       &linky_data.std.URMS1,         UINT16,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  TENSION,     "",                                    0x0B04, 0x0505,  ZB_RP, ZB_UINT16,   },
    {132, "Tension efficace Phase 2",           "URMS2",       &linky_data.std.URMS2,         UINT16,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  TENSION,     "",                                    0x0B04, 0x0905,  ZB_RP, ZB_UINT16,   },
    {133, "Tension efficace Phase 3",           "URMS3",       &linky_data.std.URMS3,         UINT16,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  TENSION,     "",                                    0x0B04, 0x0A05,  ZB_RP, ZB_UINT16,   },

    {102, "Puissance app. de référence",        "PREF",        &linky_data.std.PREF,          UINT16,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_kVA,   "",                                    0xFF42, 0x002B,  ZB_RO, ZB_UINT16,   }, //TODO: zigbee: when  Meter Identification cluster 0x0B01, 0x000D
    {000, "Puissance app. de coupure",          "PCOUP",       &linky_data.std.PCOUP,         UINT8,        0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_kVA,   "",                                    0x0B01, 0x000E,  ZB_NO, ZB_UINT8,    }, //TODO: zigbee: when  Meter Identification cluster

    {122, "Puissance soutirée",                 "SINSTS",      &linky_data.std.SINSTS,        UINT32,       0, MODE_STD,  C_ANY,   G_MONO,  STATIC_VALUE, POWER_VA,    "",                                    0x0B04, 0x050F,  ZB_RP, ZB_INT16,    },
    {123, "Puissance soutirée Phase 1",         "SINSTS1",     &linky_data.std.SINSTS1,       UINT32,       0, MODE_STD,  C_ANY,   G_TRI,  STATIC_VALUE,  POWER_VA,    "",                                    0x0B04, 0x050F,  ZB_RP, ZB_INT16,    },
    {124, "Puissance soutirée Phase 2",         "SINSTS2",     &linky_data.std.SINSTS2,       UINT32,       0, MODE_STD,  C_ANY,   G_TRI,  STATIC_VALUE,  POWER_VA,    "",                                    0x0B04, 0x090F,  ZB_RP, ZB_INT16,    },
    {125, "Puissance soutirée Phase 3",         "SINSTS3",     &linky_data.std.SINSTS3,       UINT32,       0, MODE_STD,  C_ANY,   G_TRI,  STATIC_VALUE,  POWER_VA,    "",                                    0x0B04, 0x0A0F,  ZB_RP, ZB_INT16,    },

    {135, "Puissance max soutirée Auj.",        "SMAXSN",      &linky_data.std.SMAXSN,        UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0x0B04, 0x050D,  ZB_RO, ZB_INT16,    },
    {000, "Puissance max soutirée Auj. 1",      "SMAXSN1",     &linky_data.std.SMAXSN1,       UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0x0B04, 0x050D,  ZB_RO, ZB_INT16,    },
    {000, "Puissance max soutirée Auj. 2",      "SMAXSN2",     &linky_data.std.SMAXSN2,       UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0x0B04, 0x090D,  ZB_RO, ZB_INT16,    },
    {000, "Puissance max soutirée Auj. 3",      "SMAXSN3",     &linky_data.std.SMAXSN3,       UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0x0B04, 0x0A0D,  ZB_RO, ZB_INT16,    },
    {000, "Heure Puissance max soutirée Auj",   "smaxsn_time", &linky_data.std.SMAXSN.time,   UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0xFF42, 0x002F,  ZB_RO, ZB_UINT64,    },
    {000, "Heure Puissance max soutirée Auj. 1","smaxsn1_time",&linky_data.std.SMAXSN1.time,  UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0xFF42, 0x0030,  ZB_RO, ZB_UINT64,    },
    {000, "Heure Puissance max soutirée Auj. 2","smaxsn2_time",&linky_data.std.SMAXSN2.time,  UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0xFF42, 0x0031,  ZB_RO, ZB_UINT64,    },
    {000, "Heure Puissance max soutirée Auj. 3","smaxsn3_time",&linky_data.std.SMAXSN3.time,  UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0xFF42, 0x0032,  ZB_RO, ZB_UINT64,    },


    {000, "Puissance max soutirée Hier",        "SMAXSN-1",    &linky_data.std.SMAXSN_1,      UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0xFF42, 0x0012,  ZB_RO, ZB_INT16,    },
    {000, "Puissance max soutirée Hier 1",      "SMAXSN1-1",   &linky_data.std.SMAXSN1_1,     UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0xFF42, 0x0013,  ZB_RO, ZB_INT16,    },
    {000, "Puissance max soutirée Hier 2",      "SMAXSN2-1",   &linky_data.std.SMAXSN2_1,     UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0xFF42, 0x0014,  ZB_RO, ZB_INT16,    },
    {000, "Puissance max soutirée Hier 3",      "SMAXSN3-1",   &linky_data.std.SMAXSN3_1,     UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0xFF42, 0x0015,  ZB_RO, ZB_INT16,    },
    {000, "Heure Puissance max soutirée Hier",  "maxs-1_time", &linky_data.std.SMAXSN_1.time, UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  TIMESTAMP,    "",                                    0xFF42, 0x0033,  ZB_RO, ZB_UINT64,    },
    {000, "Heure Puissance max soutirée Hier 1","maxs1-1_time",&linky_data.std.SMAXSN1_1.time,UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  TIMESTAMP,    "",                                    0xFF42, 0x0034,  ZB_RO, ZB_UINT64,    },
    {000, "Heure Puissance max soutirée Hier 2","maxs2-1_time",&linky_data.std.SMAXSN2_1.time,UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  TIMESTAMP,    "",                                    0xFF42, 0x0035,  ZB_RO, ZB_UINT64,    },
    {000, "Heure Puissance max soutirée Hier 3","maxs3-1_time",&linky_data.std.SMAXSN3_1.time,UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  TIMESTAMP,    "",                                    0xFF42, 0x0036,  ZB_RO, ZB_UINT64,    },

    {134, "Puissance injectée",                 "SINSTI",      &linky_data.std.SINSTI,        UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "mdi:transmission-tower-export",       0xFF42, 0x0016,  ZB_RP, ZB_UINT32,   },
    {135, "Puissance max injectée Auj.",        "SMAXIN",      &linky_data.std.SMAXIN,        UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0xFF42, 0x0017,  ZB_RO, ZB_UINT32,   },
    {000, "Puissance max injectée Hier",        "SMAXIN-1",    &linky_data.std.SMAXIN_1,      UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0xFF42, 0x0018,  ZB_RO, ZB_UINT32,   },
    {000, "Heure Puissance max injectée Auj.",  "smaxin_time", &linky_data.std.SMAXIN.time,   UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0xFF42, 0x0037,  ZB_RO, ZB_UINT64,    },
    {000, "Heure Puissance max injectée Hier",  "maxin-1_time",&linky_data.std.SMAXIN_1.time, UINT64,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0xFF42, 0x0038,  ZB_RO, ZB_UINT64,    },

    {000, "Point n courbe soutirée",            "CCASN",       &linky_data.std.CCASN,         UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0x0B04, 0x050B,  ZB_RO, ZB_INT16,    },
    {000, "Point n-1 courbe soutirée",          "CCASN-1",     &linky_data.std.CCASN_1,       UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0x0B04, 0x090B,  ZB_RO, ZB_INT16,    },
    {000, "Point n courbe injectée",            "CCAIN",       &linky_data.std.CCAIN,         UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0xFF42, 0x0019,  ZB_RO, ZB_INT16,    },
    {000, "Point n-1 courbe injectée",          "CCAIN-1",     &linky_data.std.CCAIN_1,       UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0xFF42, 0x001a,  ZB_RO, ZB_INT16,    },
 
    {000, "Tension moyenne Phase 1",            "UMOY1",       &linky_data.std.UMOY1,         UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  TENSION,     "",                                    0x0B04, 0x0511,  ZB_RO, ZB_UINT16,   },
    {000, "Tension moyenne Phase 2",            "UMOY2",       &linky_data.std.UMOY2,         UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  TENSION,     "",                                    0x0B04, 0x0911,  ZB_RO, ZB_UINT16,   },
    {000, "Tension moyenne Phase 3",            "UMOY3",       &linky_data.std.UMOY3,         UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  TENSION,     "",                                    0x0B04, 0x0A11,  ZB_RO, ZB_UINT16,   },

    {000, "Registre de Statuts",                "STGE",        &linky_data.std.STGE,          STRING,       8, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:state-machine",                   0xFF42, 0x000A,  ZB_RO, ZB_OCTSTR,   },
    {109, "Couleur aujourd'hui",                "aujour",      &linky_data.std.AUJOUR,        STRING,       9, MODE_STD,  C_TEMPO, G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:state-machine",                   0x0000, 0x0000,  ZB_NO, ZB_NO,       }, 
    {110, "Couleur du lendemain",               "demain",      &linky_data.std.DEMAIN,        STRING,       9, MODE_STD,  C_TEMPO, G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:state-machine",                   0xFF42, 0x0003,  ZB_RP, ZB_OCTSTR,   }, // TODO: Enedis-NOI-CPT_54E p25 Couleur du lendemain --> tuya 109

    {000, "Début Pointe Mobile 1",              "DPM1",        &linky_data.std.DPM1,          UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0xFF42, 0x001c,  ZB_RO, ZB_UINT64,   },
    {000, "Fin Pointe Mobile 1",                "FPM1",        &linky_data.std.FPM1,          UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0xFF42, 0x001d,  ZB_RO, ZB_UINT64,   },
    {000, "Début Pointe Mobile 2",              "DPM2",        &linky_data.std.DPM2,          UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0xFF42, 0x001e,  ZB_RO, ZB_UINT64,   },
    {000, "Fin Pointe Mobile 2",                "FPM2",        &linky_data.std.FPM2,          UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0xFF42, 0x001f,  ZB_RO, ZB_UINT64,   },
    {000, "Début Pointe Mobile 3",              "DPM3",        &linky_data.std.DPM3,          UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0xFF42, 0x0020,  ZB_RO, ZB_UINT64,   },
    {000, "Fin Pointe Mobile 3",                "FPM3",        &linky_data.std.FPM3,          UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0xFF42, 0x0021,  ZB_RO, ZB_UINT64,   },

    {000, "Message court",                      "MSG1",        &linky_data.std.MSG1,          STRING,      32, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:message-text-outline",            0xFF42, 0x0022,  ZB_RO, ZB_CHARSTR,  },
    {000, "Message Ultra court",                "MSG2",        &linky_data.std.MSG2,          STRING,      16, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:message-outline",                 0xFF42, 0x0023,  ZB_RO, ZB_CHARSTR,  },
    {000, "Point Référence Mesure",             "PRM",         &linky_data.std.PRM,           STRING,      14, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0x0702, 0x0307,  ZB_RO, ZB_CHARSTR,  },
    {000, "Relais",                             "RELAIS",      &linky_data.std.RELAIS,        STRING,       3, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:toggle-switch-outline",           0xFF42, 0x0024,  ZB_RO, ZB_CHARSTR,   },
    {000, "Index tarifaire en cours",           "NTARF",       &linky_data.std.NTARF,         UINT16,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0xFF42, 0x0025,  ZB_RO, ZB_UINT16,    },
    {000, "N° jours en cours fournisseur",      "NJOURF",      &linky_data.std.NJOURF,        UINT16,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0xFF42, 0x0026,  ZB_RO, ZB_UINT16     },
    {000, "N° prochain jour fournisseur",       "NJOURF+1",    &linky_data.std.NJOURF_1,      UINT16,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0xFF42, 0x0027,  ZB_RO, ZB_UINT16     },
    {000, "Profil du prochain jour",            "PJOURF+1",    &linky_data.std.PJOURF_1,      STRING,      16, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:sun-clock",                       0xFF42, 0x0028,  ZB_RO, ZB_CHARSTR    },
    {000, "Profil du prochain jour pointe",     "PPOINTE",     &linky_data.std.PPOINTE,       STRING,      58, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:sun-clock",                       0xFF42, 0x0029,  ZB_RO, ZB_CHARSTR     },
    //---------------------------Home Assistant Specific ------------------------------------------------
    {103, "Temps d'actualisation",              "now-refresh", &config_values.refresh_rate,    UINT16,       0,      ANY,  C_ANY,   G_ANY,  STATIC_VALUE,  TIME,        "mdi:refresh",                         0xFF42, 0x0002,  ZB_RW, ZB_UINT16    },
    {000, "Temps d'actualisation",              "set-refresh", &config_values.refresh_rate,    HA_NUMBER,    0,      ANY,  C_ANY,   G_ANY,  STATIC_VALUE,  TIME,        "mdi:refresh",                         0x0000, 0x0000,  ZB_NO, ZB_NO        },
    {105, "Mode TIC",                           "mode-tic",    &linky_mode,                   UINT16,       0,      ANY,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:translate",                       0xFF42, 0x002c,  ZB_RO, ZB_UINT8     },
    {106, "Mode Electrique",                    "mode-elec",   &linky_three_phase,            UINT16,       0,      ANY,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:power-plug-outline",              0xFF42, 0x002a,  ZB_RO, ZB_UINT8     },
 // {000, "Dernière actualisation",             "timestamp",   &linky_data.timestamp,         UINT64,       0,      ANY,  C_ANY,   G_ANY,  STATIC_VALUE,  TIMESTAMP,   "",                                    0x0000, 0x0000,  ZB_NO, ZB_NO        },
    {104, "Temps de fonctionnement",            "uptime",      &linky_data.uptime,            UINT64,       0,      ANY,  C_ANY,   G_ANY,  REAL_TIME,     TIME_M,      "mdi:clock-time-eight-outline",        0xFF42, 0x002d,  ZB_RO, ZB_UINT48    },
 // {000, "Free RAM",                           "free-ram",    &linky_free_heap_size,         UINT32,       0,      ANY,  C_ANY,   G_ANY,  REAL_TIME,     BYTES,       "",                                    0x0000, 0x0000,  ZB_NO, ZB_NO        },

};
const int32_t LinkyLabelListSize = sizeof(LinkyLabelList) / sizeof(LinkyLabelList[0]);
// clang-format on

const void *linky_protected_data[] = {&config_values.refresh_rate, &linky_mode, &linky_three_phase};
const uint8_t linky_protected_data_size = sizeof(linky_protected_data) / sizeof(linky_protected_data[0]);
linky_data_t linky_data; // The data
linky_mode_t linky_mode = MODE_HIST;
linky_contract_t linky_contract = C_ANY;

uint8_t linky_three_phase = 0;
uint8_t linky_reading = 0;
uint8_t linky_want_debug_frame = 0;

char linky_buffer[LINKY_BUFFER_SIZE] = {0}; // The UART buffer

const char *const HADeviceClassStr[] = {
    [NONE_CLASS] = "",
    [CURRENT] = "current",
    [POWER_VA] = "apparent_power",
    [POWER_kVA] = "power",
    [POWER_W] = "power",
    [POWER_Q] = "reactive_power",
    [ENERGY] = "energy",
    [ENERGY_Q] = "", // reactive_energy
    [TIMESTAMP] = "timestamp",
    [TENSION] = "voltage",
    [TEXT] = "",
    [TIME] = "duration",
    [TIME_M] = "duration",
    [BYTES] = "",
    [CLASS_BOOL] = "",
};

const char *const HAUnitsStr[] = {
    [NONE_CLASS] = "",
    [CURRENT] = "A",
    [POWER_VA] = "VA",
    [POWER_kVA] = "kVA",
    [POWER_W] = "W",
    [POWER_kW] = "",
    [POWER_Q] = "var",
    [ENERGY] = "Wh",
    [ENERGY_Q] = "VArh",
    [TIMESTAMP] = "",
    [TENSION] = "V",
    [TEXT] = "",
    [TIME] = "s",
    [TIME_M] = "s",
    [CLASS_BOOL] = "",
    [BYTES] = "bytes",
};

const char *const ha_sensors_str[] = {
    [UINT8] = "sensor",
    [UINT16] = "sensor",
    [UINT32] = "sensor",
    [UINT64] = "sensor",
    [STRING] = "sensor",
    [UINT32_TIME] = "sensor",
    [HA_NUMBER] = "number",
    [BOOL] = "binary_sensor",
};

const char *const linky_hist_str_contract[] = {
    [C_ANY] = "ANY",
    [C_BASE] = "BASE",
    [C_HC] = "HC..",
    [C_EJP] = "EJP.",
    [C_TEMPO] = "BBR",
};

const char *const linky_std_str_contract[] = {
    [C_ANY] = "ANY",
    [C_BASE] = "BASE",
    [C_HC] = "H PLEINE/CREUSE",
    [C_HEURES_SUPER_CREUSES] = "H SUPER CREUSES",
    [C_TEMPO] = "TEMPO",
    [C_EJP] = "EJP",
    [C_ZEN_FLEX] = "ZEN FLEX",
    [C_SEM_WE_LUNDI] = "SEM WE LUNDI",
    [C_SEM_WE_MERCREDI] = "SEM WE MERCREDI",
    [C_SEM_WE_VENDREDI] = "SEM WE VENDREDI",
};

const char *const linky_tuya_str_contract[] = {
    [C_ANY] = "ANY",
    [C_UNKNOWN] = "UNKNOWN",
    [C_BASE] = "BASE",
    [C_HC] = "HCHP",
    [C_HEURES_SUPER_CREUSES] = "HSC",
    [C_TEMPO] = "TEMPO",
    [C_EJP] = "EJP",
    [C_ZEN_FLEX] = "ZEN_FLEX",
    [C_SEM_WE_LUNDI] = "SEM_WE_LUNDI",
    [C_SEM_WE_MERCREDI] = "SEM_WE_MERCREDI",
    [C_SEM_WE_VENDREDI] = "SEM_WE_VENDREDI",
    [C_UNKNOWN] = "INCONNU",

};

const char *const linky_str_mode[] = {
    [MODE_STD] = "STD",
    [MODE_HIST] = "HIST",
    [AUTO] = "AUTO",
    [NONE] = "NONE",
};

/*==============================================================================
 Local Variable
===============================================================================*/
static char linky_uart_rx = 0; // The RX pin of the linky
// static char *linky_frame = NULL;             // The received frame from the linky
static uint8_t linky_group_separator = 0x20; // The group separator character (changes depending on the mode) (0x20 in historique mode, 0x09 in standard mode)
static uint16_t linky_frame_size = 0;        // The size of the frame
static uint32_t linky_rx_bytes = 0;          // store the number of bytes read
static char linky_frame[LINKY_BUFFER_SIZE] = {0};
static const char linky_std_debug_buffer[] = {0x2, 0xa, 0x41, 0x44, 0x53, 0x43, 0x9, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x9, 0x2d, 0xd, 0xa, 0x56, 0x54, 0x49, 0x43, 0x9, 0x30, 0x32, 0x9, 0x4a, 0xd, 0xa, 0x44, 0x41, 0x54, 0x45, 0x9, 0x48, 0x32, 0x34, 0x30, 0x31, 0x30, 0x37, 0x31, 0x37, 0x35, 0x38, 0x31, 0x30, 0x9, 0x9, 0x45, 0xd, 0xa, 0x4e, 0x47, 0x54, 0x46, 0x9, 0x20, 0x20, 0x20, 0x20, 0x20, 0x54, 0x45, 0x4d, 0x50, 0x4f, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x9, 0x46, 0xd, 0xa, 0x4c, 0x54, 0x41, 0x52, 0x46, 0x9, 0x20, 0x20, 0x20, 0x20, 0x48, 0x50, 0x20, 0x20, 0x42, 0x4c, 0x45, 0x55, 0x20, 0x20, 0x20, 0x20, 0x9, 0x2b, 0xd, 0xa, 0x45, 0x41, 0x53, 0x54, 0x9, 0x30, 0x35, 0x30, 0x30, 0x31, 0x39, 0x32, 0x32, 0x36, 0x9, 0x28, 0xd, 0xa, 0x45, 0x41, 0x53, 0x46, 0x30, 0x31, 0x9, 0x30, 0x32, 0x32, 0x32, 0x33, 0x35, 0x33, 0x34, 0x30, 0x9, 0x37, 0xd, 0xa, 0x45, 0x41, 0x53, 0x46, 0x30, 0x32, 0x9, 0x30, 0x32, 0x36, 0x35, 0x38, 0x37, 0x32, 0x37, 0x30, 0x9, 0x48, 0xd, 0xa, 0x45, 0x41, 0x53, 0x46, 0x30, 0x33, 0x9, 0x30, 0x30, 0x30, 0x34, 0x32, 0x35, 0x36, 0x39, 0x36, 0x9, 0x44, 0xd, 0xa, 0x45, 0x41, 0x53, 0x46, 0x30, 0x34, 0x9, 0x30, 0x30, 0x30, 0x34, 0x39, 0x34, 0x36, 0x31, 0x34, 0x9, 0x41, 0xd, 0xa, 0x45, 0x41, 0x53, 0x46, 0x30, 0x35, 0x9, 0x30, 0x30, 0x30, 0x31, 0x31, 0x35, 0x30, 0x34, 0x35, 0x9, 0x36, 0xd, 0xa, 0x45, 0x41, 0x53, 0x46, 0x30, 0x36, 0x9, 0x30, 0x30, 0x30, 0x31, 0x36, 0x31, 0x32, 0x36, 0x31, 0x9, 0x38, 0xd, 0xa, 0x45, 0x41, 0x53, 0x46, 0x30, 0x37, 0x9, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x9, 0x28, 0xd, 0xa, 0x45, 0x41, 0x53, 0x46, 0x30, 0x38, 0x9, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x9, 0x29, 0xd, 0xa, 0x45, 0x41, 0x53, 0x46, 0x30, 0x39, 0x9, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x9, 0x2a, 0xd, 0xa, 0x45, 0x41, 0x53, 0x46, 0x31, 0x30, 0x9, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x9, 0x22, 0xd, 0xa, 0x45, 0x41, 0x53, 0x44, 0x30, 0x31, 0x9, 0x30, 0x32, 0x30, 0x39, 0x37, 0x36, 0x37, 0x33, 0x38, 0x9, 0x4a, 0xd, 0xa, 0x45, 0x41, 0x53, 0x44, 0x30, 0x32, 0x9, 0x30, 0x32, 0x35, 0x30, 0x33, 0x33, 0x37, 0x33, 0x35, 0x9, 0x3d, 0xd, 0xa, 0x45, 0x41, 0x53, 0x44, 0x30, 0x33, 0x9, 0x30, 0x30, 0x31, 0x37, 0x39, 0x39, 0x33, 0x34, 0x33, 0x9, 0x46, 0xd, 0xa, 0x45, 0x41, 0x53, 0x44, 0x30, 0x34, 0x9, 0x30, 0x30, 0x32, 0x32, 0x30, 0x39, 0x34, 0x31, 0x30, 0x9, 0x35, 0xd, 0xa, 0x49, 0x52, 0x4d, 0x53, 0x31, 0x9, 0x30, 0x31, 0x37, 0x9, 0x36, 0xd, 0xa, 0x55, 0x52, 0x4d, 0x53, 0x31, 0x9, 0x32, 0x32, 0x37, 0x9, 0x45, 0xd, 0xa, 0x50, 0x52, 0x45, 0x46, 0x9, 0x31, 0x32, 0x9, 0x42, 0xd, 0xa, 0x50, 0x43, 0x4f, 0x55, 0x50, 0x9, 0x31, 0x32, 0x9, 0x5c, 0xd, 0xa, 0x53, 0x49, 0x4e, 0x53, 0x54, 0x53, 0x9, 0x30, 0x33, 0x39, 0x30, 0x30, 0x9, 0x52, 0xd, 0xa, 0x53, 0x4d, 0x41, 0x58, 0x53, 0x4e, 0x9, 0x48, 0x32, 0x34, 0x30, 0x31, 0x30, 0x37, 0x30, 0x32, 0x34, 0x37, 0x32, 0x33, 0x9, 0x30, 0x38, 0x36, 0x31, 0x31, 0x9, 0x3d, 0xd, 0xa, 0x53, 0x4d, 0x41, 0x58, 0x53, 0x4e, 0x2d, 0x31, 0x9, 0x48, 0x32, 0x34, 0x30, 0x31, 0x30, 0x36, 0x30, 0x33, 0x33, 0x37, 0x32, 0x35, 0x9, 0x30, 0x39, 0x39, 0x34, 0x31, 0x9, 0x23, 0xd, 0xa, 0x43, 0x43, 0x41, 0x53, 0x4e, 0x9, 0x48, 0x32, 0x34, 0x30, 0x31, 0x30, 0x37, 0x31, 0x37, 0x33, 0x30, 0x30, 0x30, 0x9, 0x30, 0x33, 0x35, 0x39, 0x34, 0x9, 0x49, 0xd, 0xa, 0x43, 0x43, 0x41, 0x53, 0x4e, 0x2d, 0x31, 0x9, 0x48, 0x32, 0x34, 0x30, 0x31, 0x30, 0x37, 0x31, 0x37, 0x30, 0x30, 0x30, 0x30, 0x9, 0x30, 0x33, 0x36, 0x36, 0x38, 0x9, 0x26, 0xd, 0xa, 0x55, 0x4d, 0x4f, 0x59, 0x31, 0x9, 0x48, 0x32, 0x34, 0x30, 0x31, 0x30, 0x37, 0x31, 0x37, 0x35, 0x30, 0x30, 0x30, 0x9, 0x32, 0x32, 0x35, 0x9, 0x32, 0xd, 0xa, 0x53, 0x54, 0x47, 0x45, 0x9, 0x30, 0x31, 0x33, 0x41, 0x43, 0x34, 0x30, 0x31, 0x9, 0x52, 0xd, 0xa, 0x44, 0x50, 0x4d, 0x32, 0x9, 0x20, 0x32, 0x34, 0x30, 0x31, 0x30, 0x38, 0x30, 0x36, 0x30, 0x30, 0x30, 0x30, 0x9, 0x30, 0x30, 0x9, 0x23, 0xd, 0xa, 0x46, 0x50, 0x4d, 0x32, 0x9, 0x20, 0x32, 0x34, 0x30, 0x31, 0x30, 0x39, 0x30, 0x36, 0x30, 0x30, 0x30, 0x30, 0x9, 0x30, 0x30, 0x9, 0x26, 0xd, 0xa, 0x4d, 0x53, 0x47, 0x31, 0x9, 0x50, 0x41, 0x53, 0x20, 0x44, 0x45, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x4d, 0x45, 0x53, 0x53, 0x41, 0x47, 0x45, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x9, 0x3c, 0xd, 0xa, 0x50, 0x52, 0x4d, 0x9, 0x30, 0x39, 0x33, 0x31, 0x32, 0x33, 0x30, 0x30, 0x39, 0x32, 0x36, 0x36, 0x39, 0x35, 0x9, 0x38, 0xd, 0xa, 0x52, 0x45, 0x4c, 0x41, 0x49, 0x53, 0x9, 0x30, 0x30, 0x30, 0x9, 0x42, 0xd, 0xa, 0x4e, 0x54, 0x41, 0x52, 0x46, 0x9, 0x30, 0x32, 0x9, 0x4f, 0xd, 0xa, 0x4e, 0x4a, 0x4f, 0x55, 0x52, 0x46, 0x9, 0x30, 0x30, 0x9, 0x26, 0xd, 0xa, 0x4e, 0x4a, 0x4f, 0x55, 0x52, 0x46, 0x2b, 0x31, 0x9, 0x30, 0x30, 0x9, 0x42, 0xd, 0xa, 0x50, 0x4a, 0x4f, 0x55, 0x52, 0x46, 0x2b, 0x31, 0x9, 0x30, 0x30, 0x30, 0x30, 0x34, 0x30, 0x30, 0x31, 0x20, 0x30, 0x36, 0x30, 0x30, 0x34, 0x30, 0x30, 0x32, 0x20, 0x32, 0x32, 0x30, 0x30, 0x34, 0x30, 0x30, 0x31, 0x20, 0x4e, 0x4f, 0x4e, 0x55, 0x54, 0x49, 0x4c, 0x45, 0x20, 0x4e, 0x4f, 0x4e, 0x55, 0x54, 0x49, 0x4c, 0x45, 0x20, 0x4e, 0x4f, 0x4e, 0x55, 0x54, 0x49, 0x4c, 0x45, 0x20, 0x4e, 0x4f, 0x4e, 0x55, 0x54, 0x49, 0x4c, 0x45, 0x20, 0x4e, 0x4f, 0x4e, 0x55, 0x54, 0x49, 0x4c, 0x45, 0x20, 0x4e, 0x4f, 0x4e, 0x55, 0x54, 0x49, 0x4c, 0x45, 0x20, 0x4e, 0x4f, 0x4e, 0x55, 0x54, 0x49, 0x4c, 0x45, 0x20, 0x4e, 0x4f, 0x4e, 0x55, 0x54, 0x49, 0x4c, 0x45, 0x9, 0x2e, 0xd, 0xa, 0x50, 0x50, 0x4f, 0x49, 0x4e, 0x54, 0x45, 0x9, 0x30, 0x30, 0x30, 0x30, 0x34, 0x30, 0x30, 0x35, 0x20, 0x30, 0x36, 0x30, 0x30, 0x34, 0x30, 0x30, 0x36, 0x20, 0x32, 0x32, 0x30, 0x30, 0x34, 0x30, 0x30, 0x35, 0x20, 0x4e, 0x4f, 0x4e, 0x55, 0x54, 0x49, 0x4c, 0x45, 0x20, 0x4e, 0x4f, 0x4e, 0x55, 0x54, 0x49, 0x4c, 0x45, 0x20, 0x4e, 0x4f, 0x4e, 0x55, 0x54, 0x49, 0x4c, 0x45, 0x20, 0x4e, 0x4f, 0x4e, 0x55, 0x54, 0x49, 0x4c, 0x45, 0x20, 0x4e, 0x4f, 0x4e, 0x55, 0x54, 0x49, 0x4c, 0x45, 0x20, 0x4e, 0x4f, 0x4e, 0x55, 0x54, 0x49, 0x4c, 0x45, 0x20, 0x4e, 0x4f, 0x4e, 0x55, 0x54, 0x49, 0x4c, 0x45, 0x20, 0x4e, 0x4f, 0x4e, 0x55, 0x54, 0x49, 0x4c, 0x45, 0x9, 0x27, 0xd, 0x3};

static esp_pm_lock_handle_t linky_pm_lock = NULL;

static uint32_t linky_decode_count = 0;
static uint32_t linky_decode_checksum_error = 0;
/*==============================================================================
Function Implementation
===============================================================================*/

/**
 * @brief Linky init function
 *
 * @param mode MODE_STANDARD or MODE_HISTORIQUE
 * @param RX RX pin number for the UART
 */
void linky_init(linky_mode_t mode, int RX)
{
    linky_mode = mode;
    linky_uart_rx = RX;
    // mode Historique: 0x20
    // mode Standard: 0x09
    linky_group_separator = (mode == MODE_HIST) ? 0x20 : 0x09;
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    switch (config_values.linky_mode)
    {
    case AUTO:
        ESP_LOGI(TAG, "Trying to autodetect Linky mode, testing last known mode: %s", linky_str_mode[config_values.last_linky_mode]);
        if (config_values.last_linky_mode == NONE)
        {
            linky_set_mode(MODE_STD); // we don't know the last mode, we start with historique
        }
        else
        {
            linky_set_mode(config_values.last_linky_mode);
        }
        break;
    case MODE_HIST:
        linky_set_mode(MODE_HIST);
        break;
    case MODE_STD:
        linky_set_mode(MODE_STD);
        break;
    default:
        break;
    }

    if (linky_pm_lock == NULL)
    {
        esp_err_t err = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "linky", &linky_pm_lock);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to create PM lock: 0x%x", err);
        }
    }

    linky_clear_data();
    // esp_log_level_set(TAG, ESP_LOG_DEBUG);
}

void linky_set_mode(linky_mode_t newMode)
{
    linky_data_t empty;
    memset(&empty, 0, sizeof empty);
    if (newMode > MODE_STD)
    {
        newMode = MODE_HIST;
        ESP_LOGW(TAG, "Invalid mode, switching to historique mode");
    }

    linky_mode = newMode;
    switch (newMode)
    {
    case MODE_HIST:
        linky_data.hist = empty.hist;
        break;
    case MODE_STD:
        linky_data.std = empty.std;
        break;
    default:
        break;
    }
    ESP_LOGI(TAG, "Changed mode to %s", linky_str_mode[linky_mode]);

    if (uart_is_driver_installed(UART_NUM_1))
    {
        uart_driver_delete(UART_NUM_1);
    }
    uart_config_t uart_config = {
        .baud_rate = 1200,
        .data_bits = UART_DATA_7_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,

    };
    switch (linky_mode)
    {
    case MODE_STD:
        // start the serial communication at 9600 bauds, 7E1
        uart_config.baud_rate = 9600;
        linky_mode = MODE_STD;
        linky_group_separator = 0x09;
        break;
    case MODE_HIST:
    default:
        // start the serial communication at 1200 bauds, 7E1
        uart_config.baud_rate = 1200;
        linky_mode = MODE_HIST;
        linky_group_separator = 0x20;
        break;
    }
    esp_err_t ret = uart_driver_install(UART_NUM_1, RX_BUF_SIZE, 0, 0, NULL, 0); // set UART1 buffer size
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(ret));
        return;
    }
    ret = uart_param_config(UART_NUM_1, &uart_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGD(TAG, "UART configured: pins RX:%d", linky_uart_rx);
    ret = uart_set_pin(UART_NUM_1, UART_PIN_NO_CHANGE, linky_uart_rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGD(TAG, "UART set up");
}

/**
 * @brief Read the data from the UART and store it in the buffer
 *
 */
static uint8_t linky_read()
{
    uint32_t timeout = (xTaskGetTickCount() * portTICK_PERIOD_MS) + 5000; // 5 seconds timeout
    memset(linky_buffer, 0, sizeof linky_buffer);                         // clear the buffer
    linky_rx_bytes = 0;
    uart_flush(UART_NUM_1); // clear the UART buffer

    uint32_t startOfFrame = UINT_MAX; // store the index of the start frame
    uint32_t endOfFrame = UINT_MAX;   // store the index of the end frame

    if (linky_want_debug_frame != 0)
    {
        linky_create_debug_frame(linky_want_debug_frame - 1);
    }
    bool hasFrame = false;
    do
    {
        linky_rx_bytes += uart_read_bytes(UART_NUM_1, linky_buffer + linky_rx_bytes, (RX_BUF_SIZE - 1) - linky_rx_bytes, 500 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "Read %lu bytes, remaning:%ld", linky_rx_bytes, timeout - xTaskGetTickCount() * portTICK_PERIOD_MS);
        //----------------------------------------------------------
        // Firt step: find the start and end of the frame
        //----------------------------------------------------------

        for (int i = 0; i < linky_rx_bytes; i++) // for each character in the buffer
        {
            if (linky_buffer[i] == START_OF_FRAME) // if the character is a start of frame
            {
                if (i + 1 < linky_rx_bytes) // we have a char after
                {
                    if (linky_buffer[i + 1] == START_OF_GROUP) // valid start of frame
                    {
                        startOfFrame = i; // store the index
                    }
                }
            }
            else if (linky_buffer[i] == END_OF_FRAME && i > startOfFrame) // if the character is an end of frame and an start of frame has been found
            {
                if (i - 1 >= 0)
                {
                    if (linky_buffer[i - 1] == END_OF_GROUP) // valid end of frame
                    {
                        endOfFrame = i; // store the index
                        break;          // stop the loop
                    }
                }
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
        hasFrame = (endOfFrame != UINT_MAX && startOfFrame != UINT_MAX && (startOfFrame < endOfFrame));
    } while (!hasFrame && (MILLIS < timeout) && linky_rx_bytes < RX_BUF_SIZE - 1);
    // ESP_LOG_BUFFER_HEXDUMP(TAG, linky_buffer, linky_rx_bytes, ESP_LOG_WARN);

    if (endOfFrame == UINT_MAX || startOfFrame == UINT_MAX || (startOfFrame > endOfFrame)) // if a start of frame and an end of frame has been found
    {
        ESP_LOGE(TAG, "Error: Frame not found");
        linky_frame_size = 0;
        linky_frame[0] = 0;
        return 0;
    }

    ESP_LOGD(TAG, "Start of frame: %lu", startOfFrame);
    ESP_LOGD(TAG, "End of frame: %lu", endOfFrame);
    linky_frame_size = endOfFrame - startOfFrame;
    memcpy(linky_frame, linky_buffer + startOfFrame, linky_frame_size);
    linky_frame[linky_frame_size] = 0;
    return 1;
    // ESP_LOGW(TAG, "-------------------");
    // ESP_LOGW(TAG, "Buffer: %s", linky_buffer);
}

/**
 * @brief Decode the data from the buffer and store it in variables
 *
 * @return 0 if an error occured, 1 if the data is valid
 */
static char linky_decode()
{
    //----------------------------------------------------------
    // Clear the previous data
    //----------------------------------------------------------
    linky_clear_data();
    linky_decode_count = 0;
    linky_decode_checksum_error = 0;
    if (linky_frame[0] == 0) // if no frame found
    {
        if (config_values.linky_mode == AUTO)
        {
            switch (linky_mode)
            {
            case MODE_HIST:
                if (strlen(linky_data.hist.ADCO) > 0)
                {
                    ESP_LOGI(TAG, "Auto mode: Mode Historique Found!");
                    config_values.linky_mode = MODE_HIST;
                    config_write();
                }
                else
                {
                    ESP_LOGI(TAG, "Auto mode: Mode Historique Not Found! Try Mode Standard");
                    linky_set_mode(MODE_STD);
                    return 2;
                }
                break;
            case MODE_STD:
                if (strlen(linky_data.std.ADSC) > 0)
                {
                    ESP_LOGI(TAG, "Auto mode: Mode Standard Found!");
                    config_values.linky_mode = MODE_STD;
                    config_write();
                }
                else
                {
                    ESP_LOGI(TAG, "Auto mode: Mode Standard Not Found! Try Mode Historique");
                    linky_set_mode(MODE_HIST);
                    return 2;
                }
                break;
            default:
                break;
            }
        }
        return 0;
    }

    // if we have a valid frame, with mode auto and its a new value mode, we save it.
    if (config_values.linky_mode == AUTO && linky_mode != config_values.last_linky_mode)
    {
        ESP_LOGI(TAG, "Linky mode: %d", linky_mode);
        ESP_LOGI(TAG, "Auto mode: New mode found: %s", linky_str_mode[linky_mode]);
        config_values.last_linky_mode = linky_mode;
        config_write();
    }

    // ESP_LOG_BUFFER_HEXDUMP(TAG, frame, endOfFrame - startOfFrame, ESP_LOG_INFO);
    //-------------------------------------
    // Second step: Find goups of data in the frame
    //-------------------------------------
    unsigned int startOfGroup[GROUP_COUNT] = {UINT_MAX}; // store starts index of each group
    unsigned int endOfGroup[GROUP_COUNT] = {UINT_MAX};   // store ends index of each group
    unsigned int startOfGroupIndex = 0;                  // store the current index of starts of group array
    unsigned int endOfGroupIndex = 0;                    // store the current index of ends of group array

    for (unsigned int i = 0; i < linky_frame_size + 1; i++) // for each character in the frame
    {
        switch (linky_frame[i])
        {
        case START_OF_GROUP: // if the character is a start of group
            // ESP_LOGI(TAG, "START OF GROUP: %u (%x) --> startOfGroupIndex: %u", i, frame[i], startOfGroupIndex);
            startOfGroup[startOfGroupIndex++] = i; // store the index and increment it
            break;                                 //
        case END_OF_GROUP:                         // if the character is a end of group
            // ESP_LOGI(TAG, "END OF GROUP: %u (%x) --> endOfGroupIndex: %u", i, frame[i], endOfGroupIndex);
            endOfGroup[endOfGroupIndex++] = i; // store the index and increment it
            break;
        default:
            break;
        }
    }

    if (startOfGroup[0] == UINT_MAX || endOfGroup[0] == UINT_MAX) // if not group found (keep the UINT_MAX value)
    {
        ESP_LOGI(TAG, "No group found");
        return 0; // exit the function (no group found)
    }

    if (startOfGroupIndex != endOfGroupIndex) // if the number of starts is not equal to the number of ends: Error
    {
        // error: number of start and end frames are not equal
        ESP_LOGI(TAG, "error: number of start and end group are not equal: %d %d", startOfGroupIndex, endOfGroupIndex);
        return 0;
    }

    // for (int i = 0; i < startOfGroupIndex; i++) // for each group
    // {
    //     ESP_LOGI(TAG, "Group %d: %d - %d", i, startOfGroup[i], endOfGroup[i]);
    // }

    //------------------------------------------
    // Third step: Find fields in each group
    //------------------------------------------
    for (int i = 0; i < startOfGroupIndex; i++) // for each group
    {
        unsigned int separators[GROUP_COUNT] = {0};           // store index of separators
        uint8_t separatorIndex = 0;                           // store the current index of separators array
        for (int j = startOfGroup[i]; j < endOfGroup[i]; j++) // for each character in group
        {
            if (linky_frame[j] == linky_group_separator) // if the character is a separator
            {
                separators[separatorIndex++] = j; // store the index of the separator
            }
        }

        char label[20] = {0};   // store the label as a string
        char value[100] = {0};  // store the data as a string
        char time[20] = {0};    // store the time as a string (H081225223518)
        char checksum[5] = {0}; // store the checksum as a string

        //-----------------------------------------------------------------------------------------------------------------replace to MEMCOPY
        memcpy(label, linky_frame + startOfGroup[i] + 1, separators[0] - startOfGroup[i] - 1); // copy the label from the group
        if (linky_mode == MODE_STD && separatorIndex == 3)                                     // if the mode is standard and the number of separators is 3
        {
            memcpy(time, linky_frame + separators[0] + 1, separators[1] - separators[0] - 1);     // copy the time from the group
            memcpy(value, linky_frame + separators[1] + 1, separators[2] - separators[1] - 1);    // copy the data from the group
            memcpy(checksum, linky_frame + separators[2] + 1, endOfGroup[i] - separators[2] - 1); // copy the checksum from the group
        }
        else
        {
            memcpy(value, linky_frame + separators[0] + 1, separators[1] - separators[0] - 1);    // copy the data from the group
            memcpy(checksum, linky_frame + separators[1] + 1, endOfGroup[i] - separators[1] - 1); // copy the checksum from the group
        }
        // ESP_LOGI(TAG, "label: %s value: %s checksum: %s", label, value, checksum);

        if (linky_checksum(label, value, time) != checksum[0]) // check the checksum with the label, data and time
        {
            // error: checksum is not correct, skip the field
            linky_decode_checksum_error++;
            ESP_LOGE(TAG, "%s = %s: checksum is not correct: %s, expected: %c", label, value, checksum, linky_checksum(label, value, time));
            continue;
        }
        else
        {
            //------------------------------------------------------------
            // Fourth step: Copy values from each field to the variables
            //------------------------------------------------------------
            for (uint32_t j = 0; j < LinkyLabelListSize; j++)
            {
                if (linky_mode != LinkyLabelList[j].mode)
                    continue;
                if (strcmp(LinkyLabelList[j].label, label) == 0)
                {
                    linky_decode_count++;
                    switch (LinkyLabelList[j].type)
                    {
                    case STRING:
                    {
                        uint32_t size = strlen(value);
                        if (size > LinkyLabelList[j].size)
                        {
                            size = LinkyLabelList[j].size;
                        }
                        strncpy((char *)LinkyLabelList[j].data, value, size);
                        ((char *)LinkyLabelList[j].data)[size] = 0;
                        break;
                    }

                    case UINT8:
                        *(uint8_t *)LinkyLabelList[j].data = strtoul(value, NULL, 10);
                        break;
                    case UINT16:
                        *(uint16_t *)LinkyLabelList[j].data = strtoul(value, NULL, 10);
                        break;
                    case UINT32:
                        *(uint32_t *)LinkyLabelList[j].data = strtoul(value, NULL, 10);
                        break;
                    case UINT64:
                        *(uint64_t *)LinkyLabelList[j].data = strtoull(value, NULL, 10);
                        break;
                    case UINT32_TIME:
                    {
                        time_label_t timeLabel = {0};
                        timeLabel.time = linky_decode_time(time);
                        timeLabel.value = strtoull(value, NULL, 10);
                        *(time_label_t *)LinkyLabelList[j].data = timeLabel;
                        break;
                    }
                    default:
                        break;
                    }
                }
            }
        }
    }

    if (linky_want_debug_frame == 3)
    {
        ESP_LOGI(TAG, "Debug frame 3: STD ALL");
        linky_set_mode(MODE_STD);
        linky_data.std = tests_std_data;
    }
    else if (linky_want_debug_frame == 4)
    {
        ESP_LOGI(TAG, "Debug frame 4: HIST ALL");
        linky_set_mode(MODE_HIST);
        linky_data.hist = tests_hist_data;
    }

    linky_data.timestamp = wifi_get_timestamp();
    linky_data.uptime = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // remove spaces from contract name
    remove_char(linky_data.hist.OPTARIF, ' ');
    remove_char(linky_data.std.NGTF, ' ');

    linky_contract = linky_get_contract(&linky_data);

    switch (linky_mode)
    {
    case MODE_HIST:
        if (linky_data.hist.PMAX != UINT32_MAX)
        {
            linky_three_phase = 1;
        }
        else
        {
            linky_three_phase = 0;
        }

        // Compute values
        if (linky_data.hist.ISOUSC != UINT32_MAX)
        {
            linky_data.hist.PREF = linky_data.hist.ISOUSC * 200 / 1000;
        }
        else
        {
            linky_data.hist.PREF = UINT32_MAX;
        }

        switch (linky_contract)
        {
        case C_BASE:
            linky_data.hist.TOTAL = linky_data.hist.BASE;
            break;
        case C_HC:
            linky_data.hist.TOTAL = linky_data.hist.HCHC + linky_data.hist.HCHP;
            break;
        case C_EJP:
            linky_data.hist.TOTAL = linky_data.hist.EJPHN + linky_data.hist.EJPHPM;
            break;
        case C_TEMPO:
            linky_data.hist.TOTAL = linky_data.hist.BBRHCJB + linky_data.hist.BBRHPJB + linky_data.hist.BBRHCJW + linky_data.hist.BBRHPJW + linky_data.hist.BBRHCJR + linky_data.hist.BBRHPJR;
            break;
        default:
            ESP_LOGE(TAG, "Error: Unknown contract: %d", linky_contract);
            linky_data.hist.TOTAL = UINT32_MAX;
            break;
        }

        if (strnlen(linky_data.hist.PTEC, sizeof(linky_data.hist.PTEC)) > 0)
        {
            if (strstr(linky_data.hist.PTEC, "JB") != NULL)
            {
                strcpy(linky_data.hist.AUJOUR, "BLEU");
            }
            else if (strstr(linky_data.hist.PTEC, "JW") != NULL)
            {
                strcpy(linky_data.hist.AUJOUR, "BLANC");
            }
            else if (strstr(linky_data.hist.PTEC, "JR") != NULL)
            {
                strcpy(linky_data.hist.AUJOUR, "ROUGE");
            }
            else
            {
                strcpy(linky_data.hist.AUJOUR, "");
            }
            break;
        }

        break;
    case MODE_STD:
        if (linky_data.std.IRMS2 != UINT16_MAX || linky_data.std.URMS3 != UINT16_MAX || linky_data.std.SINSTS1 != UINT32_MAX)
        {
            linky_three_phase = 1;
        }
        else
        {
            linky_three_phase = 0;
        }

        if (strnlen(linky_data.std.STGE, sizeof(linky_data.std.STGE)) > 0)
        {
            uint64_t value = strtoull(linky_data.std.STGE, NULL, 16);
            ESP_LOGI(TAG, "STGE: 0x%llx", value);
            // tomorrow color: bit 26 and 27
            uint8_t tomorrow_color = (value >> 26) & 0x3;
            ESP_LOGI(TAG, "tomorrow color: %d", tomorrow_color);
            switch (tomorrow_color)
            {
            case 1:
                strcpy(linky_data.std.DEMAIN, "BLEU");
                break;
            case 2:
                strcpy(linky_data.std.DEMAIN, "BLANC");
                break;
            case 3:
                strcpy(linky_data.std.DEMAIN, "ROUGE");
                break;
            default:
                strcpy(linky_data.std.DEMAIN, "INCONNU");
                break;
            }
        }

        if (strnlen(linky_data.std.LTARF, sizeof(linky_data.std.LTARF)) > 0)
        {
            if (strstr(linky_data.std.LTARF, "BLEU") != NULL)
            {
                strcpy(linky_data.std.AUJOUR, "BLEU");
            }
            else if (strstr(linky_data.std.LTARF, "BLANC") != NULL)
            {
                strcpy(linky_data.std.AUJOUR, "BLANC");
            }
            else if (strstr(linky_data.std.LTARF, "ROUG") != NULL)
            {
                strcpy(linky_data.std.AUJOUR, "ROUGE");
            }
            else
            {
                strcpy(linky_data.std.AUJOUR, "INCONNU");
            }
            break;
        }
    default:
        break;
    }

    return 1;
}

/**
 * @brief Update the data from the Linky
 * Read the UART and decode the frame
 *
 * @return char 1 if success, 0 if error
 */
char linky_update()
{
    uint8_t ret;

    esp_pm_lock_acquire(linky_pm_lock);
    linky_reading = 1;
    if (linky_mode > MODE_STD)
    {
        ESP_LOGE(TAG, "Error: Unknown mode: %d", linky_mode);
        return 0;
    }
    ESP_LOGI(TAG, "Mode: %s", linky_str_mode[linky_mode]);

    led_start_pattern(LED_LINKY_READING);

    uint32_t try = 0;
    do
    {
        ret = linky_read(); // read the data
        if (ret == 0)
        {
            // ESP_LOGE(TAG, "Error: no frame found");
            // we continue to decode the frame and test the auto mode
        }

        ret = linky_decode(); // decode the frame
        try++;

    } while (ret == 2 && try < 2); // if the mode is auto, we try the other mode if the first one failed
    linky_reading = 0;

    led_stop_pattern(LED_LINKY_READING);

    esp_pm_lock_release(linky_pm_lock);
    switch (ret)
    {
    case 0:
        linky_clear_data();
        ESP_LOGE(TAG, "Error: Decode failed");
        led_start_pattern(LED_LINKY_FAILED);
        return 0;
        break;

    case 2:
        ESP_LOGE(TAG, "Auto mode: Unable to find mode automatically");
        linky_clear_data();
        led_start_pattern(LED_LINKY_FAILED);
        return 0;
        break;

    default:
        return 1;
        break;
    }
}

/**
 * @brief Print the data
 *
 */
void linky_print()
{
    ESP_LOGI(TAG, "-------------------");
    for (uint32_t i = 0; i < LinkyLabelListSize; i++)
    {
        if (LinkyLabelList[i].data == NULL)
            continue;
        if (linky_mode != LinkyLabelList[i].mode)
            continue;

        char str_value[100] = {0};
        switch (LinkyLabelList[i].type)
        {
        case STRING:
            if (strnlen((char *)LinkyLabelList[i].data, LinkyLabelList[i].size) == 0) // print only if we have a value
                continue;
            // ESP_LOGI(TAG, "%s: %s", LinkyLabelList[i].label, (char *)LinkyLabelList[i].data);
            snprintf(str_value, sizeof(str_value), "%s", (char *)LinkyLabelList[i].data);
            break;
        case UINT8:
            if (*(uint8_t *)LinkyLabelList[i].data == UINT8_MAX) // print only if we have a value
                continue;
            // ESP_LOGI(TAG, "%s: %u", LinkyLabelList[i].label, *(uint8_t *)LinkyLabelList[i].data);
            snprintf(str_value, sizeof(str_value), "%u", *(uint8_t *)LinkyLabelList[i].data);
            break;
        case UINT16:
            if (*(uint16_t *)LinkyLabelList[i].data == UINT16_MAX) // print only if we have a value
                continue;
            // ESP_LOGI(TAG, "%s: %u", LinkyLabelList[i].label, *(uint16_t *)LinkyLabelList[i].data);
            snprintf(str_value, sizeof(str_value), "%u", *(uint16_t *)LinkyLabelList[i].data);
            break;
        case UINT32:
            if (*(uint32_t *)LinkyLabelList[i].data == UINT32_MAX) // print only if we have a value
                continue;
            // ESP_LOGI(TAG, "%s: %lu", LinkyLabelList[i].label, *(uint32_t *)LinkyLabelList[i].data);
            snprintf(str_value, sizeof(str_value), "%lu", *(uint32_t *)LinkyLabelList[i].data);
            break;
        case UINT64:
            if (*(uint64_t *)LinkyLabelList[i].data == UINT64_MAX) // print only if we have a value
                continue;
            // ESP_LOGI(TAG, "%s: %llu", LinkyLabelList[i].label, *(uint64_t *)LinkyLabelList[i].data);
            snprintf(str_value, sizeof(str_value), "%llu", *(uint64_t *)LinkyLabelList[i].data);
            break;
        case UINT32_TIME:
        {
            time_label_t timeLabel = *(time_label_t *)LinkyLabelList[i].data;
            if (timeLabel.value == UINT32_MAX) // print only if we have a value
                continue;
            struct tm *timeinfo = localtime(&timeLabel.time);
            char timeString[20];
            strftime(timeString, sizeof(timeString), "%d/%m/%Y %H:%M:%S", timeinfo);
            // ESP_LOGI(TAG, "%s: %s %lu", LinkyLabelList[i].label, timeString, timeLabel.value);
            snprintf(str_value, sizeof(str_value), "%s %lu", timeString, timeLabel.value);

            break;
        }
        default:
            continue;
            break;
        }
        char *class = (char *)HAUnitsStr[LinkyLabelList[i].device_class];
        if (class == NULL)
        {
            class = "";
        }
        ESP_LOGI(TAG, "%s (%s): %s %s", LinkyLabelList[i].name, LinkyLabelList[i].label, str_value, class);
    }

    char *contract = (char *)linky_tuya_str_contract[linky_contract];
    if (contract == NULL)
    {
        contract = "";
    }
    ESP_LOGI(TAG, "Contract: %s", contract);

    char *mode = (char *)linky_str_mode[linky_mode];
    if (mode == NULL)
    {
        mode = "";
    }
    ESP_LOGI(TAG, "Mode: %s", mode);
    ESP_LOGI(TAG, "Three phases: %s", linky_three_phase ? "Yes" : "No");

    ESP_LOGI(TAG, "-------------------");
}

/**
 * @brief Calculate the checksum
 *
 * @param label name of the field
 * @param data value of the field
 * @return return the character of the checksum
 */
static char linky_checksum(char *label, char *data, char *time)
{
    int S1 = 0;                             // sum of the ASCII codes of the characters in the label
    for (int i = 0; i < strlen(label); i++) // for each character in the label
    {                                       //
        S1 += label[i];                     // add the ASCII code of the label character to the sum
    } //
    S1 += linky_group_separator;           // add the ASCII code of the separator to the sum
    for (int i = 0; i < strlen(data); i++) // for each character in the data
    {                                      //
        S1 += data[i];                     // add the ASCII code of the data character to the sum
    } //
    if (linky_mode == MODE_STD)                    // if the mode is standard
    {                                              //
        S1 += linky_group_separator;               // add the ASCII code of the separator to the sum
        if (time != NULL && strlen(time) != 0)     //
        {                                          //
            for (int i = 0; i < strlen(time); i++) // for each character in the time
            {                                      //
                S1 += time[i];                     // add the ASCII code of the time character to the sum
            } //
            S1 += linky_group_separator; //
        } //
    } //
    return (S1 & 0x3F) + 0x20; // return the checksum
}

static time_t linky_decode_time(char *time)
{
    // Le format utilisé pour les horodates est SAAMMJJhhmmss, c'est-à-dire Saison, Année, Mois, Jour, heure, minute, seconde.
    // La saison est codée sur 1 caractère :
    // - H pour Hiver (du 1er novembre au 31 mars)
    // - E pour Eté (du 1er avril au 31 octobre)
    // L'année est codée sur 2 caractères.
    // Le mois est codé sur 2 caractères.
    // Le jour est codé sur 2 caractères.
    // L'heure est codée sur 2 caractères.
    // La minute est codée sur 2 caractères.
    // La seconde est codée sur 2 caractères.
    if (strlen(time) != 13)
    {
        ESP_LOGE(TAG, "Error: Time format is not correct");
        return 0;
    }
    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));
    tm.tm_year = (time[1] - '0') * 10 + (time[2] - '0') + 100; // year since 1900
    tm.tm_mon = (time[3] - '0') * 10 + (time[4] - '0') - 1;    // month sinc S1 += linky_group_separator;e January [0-11]
    tm.tm_mday = (time[5] - '0') * 10 + (time[6] - '0');       // day of the month [1-31]
    tm.tm_hour = (time[7] - '0') * 10 + (time[8] - '0');       // hours since midnight [0-23]
    tm.tm_min = (time[9] - '0') * 10 + (time[10] - '0');       // minutes after the hour [0-59]
    tm.tm_sec = (time[11] - '0') * 10 + (time[12] - '0');      // seconds after the minute [0-60]
    return mktime(&tm);
}

uint8_t linky_presence()
{
    switch (linky_mode)
    {
    case MODE_HIST:
        if (strlen(linky_data.hist.ADCO) > 0)
            return 1;
        else
            return 0;
        break;
    case MODE_STD:
        if (strlen(linky_data.std.ADSC) > 0)
            return 1;
        else
            return 0;
        break;
    default:
        break;
    }
    return 0;
}

static void linky_create_debug_frame(uint8_t mode_std)
{
    // debug frame:
    // ADCO 031976306475 J
    // OPTARIF BASE 0
    // ISOUSC 30 9
    // BASE 062105110 [
    // PTEC TH.. $
    // IINST1 000 H
    // IINST2 000 I
    // IINST3 002 L
    // IMAX1 060 6
    // IMAX2 060 7
    // IMAX3 060 8
    // PMAX 06082 6
    // PAPP 00540 *
    // HHPHC A ,
    // MOTDETAT 000000 B
    // OT 00 #
    struct debug_group_t
    {
        char name[50];
        char value[100];
        char checksum;
    };
    struct debug_group_t debug_hist[16] = {
        {"ADCO", "031976306475", 'J'},
        {"OPTARIF", "BASE", '0'},
        {"ISOUSC", "30", '9'},
        {"BASE", "062105110", '['},
        {"PTEC", "TH..", '$'},
        {"IINST1", "000", 'H'},
        {"IINST2", "000", 'I'},
        {"IINST3", "002", 'L'},
        {"IMAX1", "060", '6'},
        {"IMAX2", "060", '7'},
        {"IMAX3", "060", '8'},
        {"PMAX", "06082", '6'},
        {"PAPP", "00540", '*'},
        {"HHPHC", "A", ','},
        {"MOTDETAT", "000000", 'B'},
        {"OT", "00", '#'},
    };

    if (mode_std)
    {
        ESP_LOGI(TAG, "Debug frame: Mode Standard");
        linky_set_mode(MODE_STD);
        linky_rx_bytes = 0;
        memcpy(linky_buffer, linky_std_debug_buffer, sizeof(linky_std_debug_buffer));
        linky_rx_bytes = sizeof(linky_std_debug_buffer);
        return;
    }
    linky_set_mode(MODE_HIST);

    // random base value
    snprintf(debug_hist[3].value, sizeof(debug_hist[3].value), "%ld", esp_random() % 1000000);
    debug_hist[3].checksum = linky_checksum(debug_hist[3].name, debug_hist[3].value, NULL);

    const uint16_t debugGroupCount = sizeof(debug_hist) / sizeof(debug_hist[0]);
    linky_rx_bytes = 0;
    linky_buffer[linky_rx_bytes++] = START_OF_FRAME;
    for (uint16_t i = 0; i < debugGroupCount - 1; i++)
    {
        linky_buffer[linky_rx_bytes++] = START_OF_GROUP;
        for (uint16_t j = 0; j < strlen(debug_hist[i].name); j++)
        {
            linky_buffer[linky_rx_bytes++] = debug_hist[i].name[j];
        }
        linky_buffer[linky_rx_bytes++] = linky_group_separator;
        for (uint16_t j = 0; j < strlen(debug_hist[i].value); j++)
        {
            linky_buffer[linky_rx_bytes++] = debug_hist[i].value[j];
        }
        linky_buffer[linky_rx_bytes++] = linky_group_separator;
        linky_buffer[linky_rx_bytes++] = debug_hist[i].checksum;
        linky_buffer[linky_rx_bytes++] = END_OF_GROUP;
    }
    linky_buffer[linky_rx_bytes++] = END_OF_FRAME;
    // ESP_LOG_BUFFER_HEXDUMP(TAG, linky_buffer, linky_rx_bytes + 1, ESP_LOG_INFO);
}

static void linky_clear_data()
{
    for (uint32_t i = 0; i < LinkyLabelListSize; i++)
    {
        if (LinkyLabelList[i].data == NULL)
        {
            continue;
        }
        uint8_t found = 0;
        for (uint32_t j = 0; j < linky_protected_data_size; j++)
        {
            if (LinkyLabelList[i].data == linky_protected_data[j])
            {
                found = 1;
                continue;
            }
        }
        if (found)
        {
            continue;
        }

        switch (LinkyLabelList[i].type)
        {
        case STRING:
            memset((char *)LinkyLabelList[i].data, 0, LinkyLabelList[i].size);
            break;
        case UINT8:
            *(uint8_t *)LinkyLabelList[i].data = UINT8_MAX;
            break;
        case UINT16:
            *(uint16_t *)LinkyLabelList[i].data = UINT16_MAX;
            break;
        case UINT32:
            *(uint32_t *)LinkyLabelList[i].data = UINT32_MAX;
            break;
        case UINT64:
            *(uint64_t *)LinkyLabelList[i].data = UINT64_MAX;
            break;
        case UINT32_TIME:
        {
            memset((char *)LinkyLabelList[i].data, 0xFF, sizeof(time_label_t));
            break;
        }
        default:
            break;
        }
    }
}

void linky_print_debug_frame()
{
    printf("\n");
    for (int i = 0; i < linky_frame_size + 2; i++)
    {
        if (linky_frame[i] <= 0x20)
        {
            printf("[%d]", linky_frame[i]);
        }
        else
        {
            printf("%c", linky_frame[i]);
        }
        if (linky_frame[i] == 13)
        {
            printf("\n");
        }
    }
    printf("\n");
}

linky_contract_t linky_get_contract(linky_data_t *data)
{
    assert(data != NULL);
    linky_contract_t contract = C_ANY;
    char raw[20] = {0};
    switch (linky_mode)
    {
    case MODE_HIST:
        strncpy(raw, data->hist.OPTARIF, MIN(sizeof(data->hist.OPTARIF), sizeof(raw)));
        break;
    case MODE_STD:
        strncpy(raw, data->std.NGTF, MIN(sizeof(data->std.NGTF), sizeof(raw)));
        break;
    default:
        ESP_LOGE(TAG, "linky_get_contract: Unknown mode");
        return C_ANY;
        break;
    }

    switch (linky_mode)
    {
    case MODE_HIST:
        for (int i = 0; i < sizeof(linky_hist_str_contract) / sizeof(linky_hist_str_contract[0]); i++)
        {
            if (linky_hist_str_contract[i] == NULL)
            {
                continue;
            }
            if (strncmp(linky_hist_str_contract[i], raw, MIN(sizeof(linky_hist_str_contract[i]), sizeof(raw))) == 0)
            {
                contract = i;
                break;
            }
        }
        break;
    case MODE_STD:
        for (int i = 0; i < sizeof(linky_std_str_contract) / sizeof(linky_std_str_contract[0]); i++)
        {
            if (linky_std_str_contract[i] == NULL)
            {
                continue;
            }
            if (strncmp(linky_std_str_contract[i], raw, MIN(sizeof(linky_std_str_contract[i]), sizeof(raw))) == 0)
            {
                contract = i;
                break;
            }
        }
        break;
    default:
        ESP_LOGE(TAG, "linky_get_contract: Unknown mode");
        break;
    }
    return contract;
}

void linky_stats()
{
    printf("Linky mode: %s\n", linky_str_mode[linky_mode]);
    printf("Linky three phase: %s\n", linky_three_phase ? "Yes" : "No");
    printf("Linky presence: %s\n", linky_presence() ? "Yes" : "No");
    printf("Linky contract: %s\n", linky_hist_str_contract[linky_contract]);
    printf("Linky refresh rate: %d\n", config_values.refresh_rate);
    printf("Linky decode count: %ld\n", linky_decode_count);
    printf("Linky checksum error: %ld\n", linky_decode_checksum_error);
}