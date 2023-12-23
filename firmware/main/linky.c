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

/*==============================================================================
 Local Define
===============================================================================*/
// clang-format off
#define LINKY_BUFFER_SIZE 1024 // The size of the UART buffer
#define START_OF_FRAME  0x02 // The start of frame character
#define END_OF_FRAME    0x03   // The end of frame character

#define START_OF_GROUP  0x0A  // The start of group character
#define END_OF_GROUP    0x0D    // The end of group character


#define RX_BUF_SIZE     1024 // The size of the UART buffer
#define FRAME_COUNT     5   // The max number of frame in buffer
#define FRAME_SIZE      500   // The size of one frame buffer
#define GROUP_COUNT     50

#define TAG "Linky"

// clang-format on
/*==============================================================================
 Local Macro
===============================================================================*/
#define ZB_RP ESP_ZB_ZCL_ATTR_ACCESS_REPORTING
#define ZB_RO ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY
#define ZB_RW ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE
#define ZB_NO 0
/*==============================================================================
 Local Type
===============================================================================*/

/*==============================================================================
 Local Function Declaration
===============================================================================*/
static void linky_read();                                        // Read the UART buffer
static char linky_decode();                                      // Decode the frame
static char linky_checksum(char *label, char *data, char *time); // Check the checksum
static void linky_create_debug_frame();
static time_t linky_decode_time(char *time); // Decode the time
static void linky_clear_data();
/*==============================================================================
Public Variable
===============================================================================*/
// clang-format off

uint32_t linky_free_heap_size = 0;
uint64_t linky_uptime = 0;

const LinkyGroup LinkyLabelList[] =
{   
    // ID  Name                               Label         DataPtr                        Type                MODE     UpdateType    Class          Icon                                   ZB_CLUSTER_ID, ZB_ATTRIBUTE_ID
    //-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //--------------------------- MODE HISTORIQUE --------------------------------
    {101, "Identifiant",                     "ADCO",        &linky_data.hist.ADCO,         STRING,      12, MODE_HIST, C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:card-account-details",            0x0702, 0x0308,  ZB_RO},
    {102, "Option tarifaire",                "OPTARIF",     &linky_data.hist.OPTARIF,      STRING,       4, MODE_HIST, C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:cash-multiple",                   0x0000, 0x0000,  ZB_RO},  
    {103, "Intensité souscrite",             "ISOUSC",      &linky_data.hist.ISOUSC,       UINT32,       0, MODE_HIST, C_ANY,   G_ANY,  STATIC_VALUE,  CURRENT,     "",                                    0x0B01, 0x000D,  ZB_NO},    

    {104, "Index Base",                      "BASE",        &linky_data.hist.BASE,         UINT64,       0, MODE_HIST, C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0100,  ZB_RP},
    {105, "Index Heures Creuses",            "HCHC",        &linky_data.hist.HCHC,         UINT64,       0, MODE_HIST, C_HCHC,  G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0100,  ZB_RP},
    {106, "Index Heures Pleines",            "HCHP",        &linky_data.hist.HCHP,         UINT64,       0, MODE_HIST, C_HCHC,  G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0102,  ZB_RP},
    {107, "Index Heures Normales",           "EJPHN",       &linky_data.hist.EJPHN,        UINT64,       0, MODE_HIST, C_EJP,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0100,  ZB_RP},
    {108, "Index Heures de Pointe Mobile",   "EJPHPM",      &linky_data.hist.EJPHPM,       UINT64,       0, MODE_HIST, C_EJP,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0102,  ZB_RP},
    {0,   "Préavis Début EJP",               "PEJP",        &linky_data.hist.PEJP,         UINT64,       0, MODE_HIST, C_EJP,   G_ANY,  STATIC_VALUE,  BOOL,        "mdi:clock",                           0x0000, 0x0000,  ZB_RP},
    {109, "Heures Creuses Jours Bleus",      "BBRHCJB",     &linky_data.hist.BBRHCJB,      UINT64,       0, MODE_HIST, C_TEMPO, G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0100,  ZB_RP},
    {110, "Heures Pleines Jours Bleus",      "BBRHPJB",     &linky_data.hist.BBRHPJB,      UINT64,       0, MODE_HIST, C_TEMPO, G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0102,  ZB_RP},
    {111, "Heures Creuses Jours Blancs",     "BBRHCJW",     &linky_data.hist.BBRHCJW,      UINT64,       0, MODE_HIST, C_TEMPO, G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0104,  ZB_RP},
    {112, "Heures Pleines Jours Blancs",     "BBRHPJW",     &linky_data.hist.BBRHPJW,      UINT64,       0, MODE_HIST, C_TEMPO, G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0106,  ZB_RP},
    {113, "Heures Creuses Jours Rouges",     "BBRHCJR",     &linky_data.hist.BBRHCJR,      UINT64,       0, MODE_HIST, C_TEMPO, G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x0108,  ZB_RP},
    {114, "Heures Pleines Jours Rouges",     "BBRHPJR",     &linky_data.hist.BBRHPJR,      UINT64,       0, MODE_HIST, C_TEMPO, G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0702, 0x010A,  ZB_RP},

    {115, "Période tarifaire en cours",      "PTEC",        &linky_data.hist.PTEC,         STRING,       4, MODE_HIST, C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:calendar-clock",                  0x0702, 0x0020,  ZB_NO},
    {116, "Couleur du lendemain",            "DEMAIN",      &linky_data.hist.DEMAIN,       STRING,       4, MODE_HIST, C_TEMPO, G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0x0000, 0x0000,  ZB_RP},

    {117, "Intensité instantanée",           "IINST",       &linky_data.hist.IINST,        UINT16,       0, MODE_HIST, C_ANY,   G_MONO, REAL_TIME,     CURRENT,     "",                                    0x0B04, 0x0508,  ZB_RP},
    {3,   "Intensité instantanée Phase 1",   "IINST1",      &linky_data.hist.IINST1,       UINT16,       0, MODE_HIST, C_ANY,   G_TRI,  REAL_TIME,     CURRENT,     "",                                    0x0B04, 0x0508,  ZB_RP},
    {4,   "Intensité instantanée Phase 2",   "IINST2",      &linky_data.hist.IINST2,       UINT16,       0, MODE_HIST, C_ANY,   G_TRI,  REAL_TIME,     CURRENT,     "",                                    0x0B04, 0x0908,  ZB_RP},
    {5,   "Intensité instantanée Phase 3",   "IINST3",      &linky_data.hist.IINST3,       UINT16,       0, MODE_HIST, C_ANY,   G_TRI,  REAL_TIME,     CURRENT,     "",                                    0x0B04, 0x0A08,  ZB_RP},
    {118, "Intensité maximale",              "IMAX",        &linky_data.hist.IMAX,         UINT16,       0, MODE_HIST, C_ANY,   G_MONO, STATIC_VALUE,  CURRENT,     "",                                    0x0B04, 0x050A,  ZB_RO},
    {6,   "Intensité maximale Phase 1",      "IMAX1",       &linky_data.hist.IMAX1,        UINT16,       0, MODE_HIST, C_ANY,   G_TRI,  STATIC_VALUE,  CURRENT,     "",                                    0x0B04, 0x050A,  ZB_RO},
    {7,   "Intensité maximale Phase 2",      "IMAX2",       &linky_data.hist.IMAX2,        UINT16,       0, MODE_HIST, C_ANY,   G_TRI,  STATIC_VALUE,  CURRENT,     "",                                    0x0B04, 0x090A,  ZB_RO},
    {8,   "Intensité maximale Phase 3",      "IMAX3",       &linky_data.hist.IMAX3,        UINT16,       0, MODE_HIST, C_ANY,   G_TRI,  STATIC_VALUE,  CURRENT,     "",                                    0x0B04, 0x0A0A,  ZB_RO},
    {119, "Dépassement Puissance",           "ADPS",        &linky_data.hist.ADPS,         UINT16,       0, MODE_HIST, C_ANY,   G_MONO, STATIC_VALUE,  CURRENT,     "",                                    0x0000, 0x0000,  ZB_RP},
    {9,   "Dépassement Intensité Phase 1",   "ADIR1",       &linky_data.hist.ADIR1,        UINT16,       0, MODE_HIST, C_ANY,   G_TRI,  STATIC_VALUE,  CURRENT,     "",                                    0x0000, 0x0000,  ZB_RP},
    {10,  "Dépassement Intensité Phase 2",   "ADIR2",       &linky_data.hist.ADIR2,        UINT16,       0, MODE_HIST, C_ANY,   G_TRI,  STATIC_VALUE,  CURRENT,     "",                                    0x0000, 0x0000,  ZB_RP},
    {11,  "Dépassement Intensité Phase 3",   "ADIR3",       &linky_data.hist.ADIR3,        UINT16,       0, MODE_HIST, C_ANY,   G_TRI,  STATIC_VALUE,  CURRENT,     "",                                    0x0000, 0x0000,  ZB_RP},

    {120, "Puissance apparente",             "PAPP",        &linky_data.hist.PAPP,         UINT32,       0, MODE_HIST, C_ANY,   G_ANY,  REAL_TIME,     POWER_VA,    "",                                    0x0B04, 0x050F,  ZB_RP},
    {121, "Puissance maximale triphasée",    "PMAX",        &linky_data.hist.PMAX,         UINT32,       0, MODE_HIST, C_ANY,   G_TRI,  STATIC_VALUE,  POWER_W,     "",                                    0x0B04, 0x050D,  ZB_RO},
    {12,  "Présence des potentiels",         "PPOT",        &linky_data.hist.PPOT,         UINT32,       0, MODE_HIST, C_ANY,   G_TRI,  REAL_TIME,     NONE_CLASS,  "",                                    0x0000, 0x0000,  ZB_RO},

    {122, "Horaire Heures Creuses",          "HHPHC",       &linky_data.hist.HHPHC,        STRING,       3, MODE_HIST, C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:home-clock",                      0x0000, 0x0000,  ZB_NO},
    {123, "Mot d'état du compteur",          "MOTDETAT",    &linky_data.hist.MOTDETAT,     STRING,       6, MODE_HIST, C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:state-machine",                   0x0000, 0x0000,  ZB_NO},

    //------------------------ MODE STANDARD -----------------------C_ANY, G_ANY, 
    {101, "Identifiant",                     "ADSC",        &linky_data.std.ADSC,          STRING,      12, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:card-account-details",            0x0000, 0x0000,  ZB_NO},
    {1,   "Version de la TIC",               "VTIC",        &linky_data.std.VTIC,          STRING,       2, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:tag",                             0x0000, 0x0000,  ZB_NO},
    {3,   "Date et heure courante",          "DATE",        &linky_data.std.DATE,          UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:clipboard-text-clock",            0x0000, 0x0000,  ZB_NO},
    {102, "Nom du calendrier tarifaire",     "NGTF",        &linky_data.std.NGTF,          STRING,      16, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:calendar-clock",                  0x0000, 0x0000,  ZB_NO},
    {115, "Libellé tarif en cours",          "LTARF",       &linky_data.std.LTARF,         STRING,      16, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:tag-text",                        0x0000, 0x0000,  ZB_NO},

    {104, "Index Total Energie soutirée",    "EAST",        &linky_data.std.EAST,          UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0000, 0x0000,  ZB_NO},
    {105, "Index 1 Energie soutirée",        "EASF01",      &linky_data.std.EASF01,        UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0000, 0x0000,  ZB_NO},
    {106, "Index 2 Energie soutirée",        "EASF02",      &linky_data.std.EASF02,        UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0000, 0x0000,  ZB_NO},
    {107, "Index 3 Energie soutirée",        "EASF03",      &linky_data.std.EASF03,        UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0000, 0x0000,  ZB_NO},
    {108, "Index 4 Energie soutirée",        "EASF04",      &linky_data.std.EASF04,        UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0000, 0x0000,  ZB_NO},
    {109, "Index 5 Energie soutirée",        "EASF05",      &linky_data.std.EASF05,        UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0000, 0x0000,  ZB_NO},
    {110, "Index 6 Energie soutirée",        "EASF06",      &linky_data.std.EASF06,        UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0000, 0x0000,  ZB_NO},
    {111, "Index 7 Energie soutirée",        "EASF07",      &linky_data.std.EASF07,        UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0000, 0x0000,  ZB_NO},
    {112, "Index 8 Energie soutirée",        "EASF08",      &linky_data.std.EASF08,        UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0000, 0x0000,  ZB_NO},
    {113, "Index 9 Energie soutirée",        "EASF09",      &linky_data.std.EASF09,        UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0000, 0x0000,  ZB_NO},
    {114, "Index 10 Energie soutirée",       "EASF10",      &linky_data.std.EASF10,        UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0000, 0x0000,  ZB_NO},

    {0,   "Index 1 Energie soutirée Distr",  "EASD01",      &linky_data.std.EASD01,        UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0000, 0x0000,  ZB_NO},
    {0,   "Index 2 Energie soutirée Distr",  "EASD02",      &linky_data.std.EASD02,        UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0000, 0x0000,  ZB_NO},
    {0,   "Index 3 Energie soutirée Distr",  "EASD03",      &linky_data.std.EASD03,        UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0000, 0x0000,  ZB_NO},
    {0,   "Index 4 Energie soutirée Distr",  "EASD04",      &linky_data.std.EASD04,        UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "",                                    0x0000, 0x0000,  ZB_NO},

    {124, "Energie injectée totale",         "EAIT",        &linky_data.std.EAIT,          UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY,      "mdi:transmission-tower-export",       0x0000, 0x0000,  ZB_NO},

    {4,   "Energie réactive Q1 totale",      "ERQ1",        &linky_data.std.ERQ1,          UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY_Q,    "",                                    0x0B04, 0x0305,  ZB_NO},
    {5,   "Energie réactive Q2 totale",      "ERQ2",        &linky_data.std.ERQ2,          UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY_Q,    "",                                    0x0B04, 0x050E,  ZB_NO},
    {6,   "Energie réactive Q3 totale",      "ERQ3",        &linky_data.std.ERQ3,          UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY_Q,    "",                                    0x0B04, 0x090E,  ZB_NO},
    {7,   "Energie réactive Q4 totale",      "ERQ4",        &linky_data.std.ERQ4,          UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  ENERGY_Q,    "",                                    0x0B04, 0x0A0E,  ZB_NO},

    {117, "Courant efficace Phase 1",        "IRMS1",       &linky_data.std.IRMS1,         UINT16,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  CURRENT,     "",                                    0x0B04, 0x0508,  ZB_NO},
    {0,   "Courant efficace Phase 2",        "IRMS2",       &linky_data.std.IRMS2,         UINT16,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  CURRENT,     "",                                    0x0B04, 0x0908,  ZB_NO},
    {0,   "Courant efficace Phase 3",        "IRMS3",       &linky_data.std.IRMS3,         UINT16,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  CURRENT,     "",                                    0x0B04, 0x0A08,  ZB_NO},

    {125, "Tension efficace Phase 1",        "URMS1",       &linky_data.std.URMS1,         UINT16,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  TENSION,     "",                                    0x0B04, 0x0505,  ZB_NO},
    {0,   "Tension efficace Phase 2",        "URMS2",       &linky_data.std.URMS2,         UINT16,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  TENSION,     "",                                    0x0B04, 0x0905,  ZB_NO},
    {0,   "Tension efficace Phase 3",        "URMS3",       &linky_data.std.URMS3,         UINT16,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  TENSION,     "",                                    0x0B04, 0x0A05,  ZB_NO},

    {126, "Puissance app. de référence",     "PREF",        &linky_data.std.PREF,          UINT8,        0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_kVA,   "",                                    0x0000, 0x0000,  ZB_RO},
    {127, "Puissance app. de coupure",       "PCOUP",       &linky_data.std.PCOUP,         UINT8,        0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_kVA,   "",                                    0x0000, 0x0000,  ZB_RO},

    {120, "Puissance soutirée",              "SINSTS",      &linky_data.std.SINSTS,        UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0x0B04, 0x0306,  ZB_NO}, //0x050F (mono) et 0x0306 tri
    {0,   "Puissance soutirée Phase 1",      "SINSTS1",     &linky_data.std.SINSTS1,       UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0x0B04, 0x050F,  ZB_NO},
    {0,   "Puissance soutirée Phase 2",      "SINSTS2",     &linky_data.std.SINSTS2,       UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0x0B04, 0x090F,  ZB_NO},
    {0,   "Puissance soutirée Phase 3",      "SINSTS3",     &linky_data.std.SINSTS3,       UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0x0B04, 0x0A0F,  ZB_NO},

    {121, "Puissance max soutirée Auj.",     "SMAXSN",      &linky_data.std.SMAXSN,        UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0x0B04, 0x0304,  ZB_NO}, //0x0304 tri et 0x050D mono
    {0,   "Puissance max soutirée Auj. 1",   "SMAXSN1",     &linky_data.std.SMAXSN1,       UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0x0B04, 0x050D,  ZB_NO},
    {0,   "Puissance max soutirée Auj. 2",   "SMAXSN2",     &linky_data.std.SMAXSN2,       UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0x0B04, 0x090D,  ZB_NO},
    {0,   "Puissance max soutirée Auj. 3",   "SMAXSN3",     &linky_data.std.SMAXSN3,       UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0x0B04, 0x0A0D,  ZB_NO},

    {0,   "Puissance max soutirée Hier",     "SMAXSN-1",    &linky_data.std.SMAXSN_1,      UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0x0000, 0x0000,  ZB_NO},
    {0,   "Puissance max soutirée Hier 1",   "SMAXSN1-1",   &linky_data.std.SMAXSN1_1,     UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0x0000, 0x0000,  ZB_NO},
    {0,   "Puissance max soutirée Hier 2",   "SMAXSN2-1",   &linky_data.std.SMAXSN2_1,     UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0x0000, 0x0000,  ZB_NO},
    {0,   "Puissance max soutirée Hier 3",   "SMAXSN3-1",   &linky_data.std.SMAXSN3_1,     UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0x0000, 0x0000,  ZB_NO},

    {128, "Puissance injectée",              "SINSTI",      &linky_data.std.SINSTI,        UINT32,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "mdi:transmission-tower-export",       0x0000, 0x0000,  ZB_NO},
    {129, "Puissance max injectée Auj.",     "SMAXIN",      &linky_data.std.SMAXIN,        UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0x0000, 0x0000,  ZB_NO},
    {130, "Puissance max injectée Hier",     "SMAXIN-1",    &linky_data.std.SMAXIN_1,      UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  POWER_VA,    "",                                    0x0000, 0x0000,  ZB_NO},

    {8,   "Point n courbe soutirée",         "CCASN",       &linky_data.std.CCASN,         UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0x0B04, 0x050B,  ZB_NO},
    {9,   "Point n-1 courbe soutirée",       "CCASN-1",     &linky_data.std.CCASN_1,       UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0x0B04, 0x090B,  ZB_NO},
    {10,  "Point n courbe injectée",         "CCAIN",       &linky_data.std.CCAIN,         UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0x0000, 0x0000,  ZB_NO},
    {11,  "Point n-1 courbe injectée",       "CCAIN-1",     &linky_data.std.CCAIN_1,       UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0x0000, 0x0000,  ZB_NO},
 
    {12,  "Tension moyenne Phase 1",         "UMOY1",       &linky_data.std.UMOY1,         UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  TENSION,     "",                                    0x0B04, 0x0511,  ZB_NO},
    {13,  "Tension moyenne Phase 2",         "UMOY2",       &linky_data.std.UMOY2,         UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  TENSION,     "",                                    0x0B04, 0x0911,  ZB_NO},
    {14,  "Tension moyenne Phase 3",         "UMOY3",       &linky_data.std.UMOY3,         UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  TENSION,     "",                                    0x0B04, 0x0A11,  ZB_NO},

    {15,  "Registre de Statuts",             "STGE",        &linky_data.std.STGE,          STRING,       0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:state-machine",                   0x0000, 0x0000,  ZB_NO},

    {16,  "Début Pointe Mobile 1",           "DPM1",        &linky_data.std.DPM1,          UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0x0000, 0x0000,  ZB_NO},
    {17,  "Fin Pointe Mobile 1",             "FPM1",        &linky_data.std.FPM1,          UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0x0000, 0x0000,  ZB_NO},
    {18,  "Début Pointe Mobile 2",           "DPM2",        &linky_data.std.DPM2,          UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0x0000, 0x0000,  ZB_NO},
    {19,  "Fin Pointe Mobile 2",             "FPM2",        &linky_data.std.FPM2,          UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0x0000, 0x0000,  ZB_NO},
    {20,  "Début Pointe Mobile 3",           "DPM3",        &linky_data.std.DPM3,          UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0x0000, 0x0000,  ZB_NO},
    {21,  "Fin Pointe Mobile 3",             "FPM3",        &linky_data.std.FPM3,          UINT32_TIME,  0, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0x0000, 0x0000,  ZB_NO},

    {22,  "Message court",                   "MSG1",        &linky_data.std.MSG1,          STRING,      32, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:message-text-outline",            0x0000, 0x0000,  ZB_RO},
    {123, "Message Ultra court",             "MSG2",        &linky_data.std.MSG2,          STRING,      16, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:message-outline",                 0x0000, 0x0000,  ZB_RO},
    {23,  "PRM",                             "PRM",         &linky_data.std.PRM,           STRING,      14, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0x0000, 0x0000,  ZB_RO},
    {143, "Relais",                          "RELAIS",      &linky_data.std.RELAIS,        STRING,       3, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:toggle-switch-outline",           0x0000, 0x0000,  ZB_RO},
    {144, "Index tarifaire en cours",        "NTARF",       &linky_data.std.NTARF,         STRING,       2, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0x0000, 0x0000,  ZB_NO},
    {24,  "N° jours en cours fournisseur",   "NJOURF",      &linky_data.std.NJOURF,        STRING,       2, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0x0000, 0x0000,  ZB_NO},
    {25,  "N° prochain jour fournisseur",    "NJOURF+1",    &linky_data.std.NJOURF_1,      STRING,       2, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0x0000, 0x0000,  ZB_NO},
    {26,  "Profil du prochain jour",         "PJOURF+1",    &linky_data.std.MSG2,          STRING,      16, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:sun-clock",                       0x0000, 0x0000,  ZB_NO},
    {27,  "Profil du prochain jour pointe",  "PPOINTE",     &linky_data.std.PPOINTE,       STRING,      98, MODE_STD,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "mdi:sun-clock",                       0x0000, 0x0000,  ZB_NO},
    //---------------------------Home Assistant Specific ------------------------------------------------
    {131, "Temps d'actualisation",          "now-refresh",  &config_values.refreshRate,    UINT16,       0,      ANY,  C_ANY,   G_ANY,  STATIC_VALUE,  TIME,        "mdi:refresh",                         0x0000, 0x0000,  ZB_NO},
    {0,   "Temps d'actualisation",          "set-refresh",  &config_values.refreshRate,    HA_NUMBER,    0,      ANY,  C_ANY,   G_ANY,  STATIC_VALUE,  TIME,        "mdi:refresh",                         0x0000, 0x0000,  ZB_NO},
 // {132, "Mode TIC",                       "mode-tic",    &mode,                          UINT16,       0,      ANY,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0x0000, 0x0000,  ZB_NO},
 // {133, "Mode Elec",                      "mode-tri",    &linky_tree_phase,              UINT16,       0,      ANY,  C_ANY,   G_ANY,  STATIC_VALUE,  NONE_CLASS,  "",                                    0x0000, 0x0000,  ZB_NO},
    {0,   "Dernière actualisation",         "timestamp",   &linky_data.timestamp,          UINT64,       0,      ANY,  C_ANY,   G_ANY,  STATIC_VALUE,  TIMESTAMP,   "",                                    0x0000, 0x0000,  ZB_NO},
    {134, "Temps de fonctionnement",        "uptime",      &linky_uptime,                  UINT64,       0,      ANY,  C_ANY,   G_ANY,     REAL_TIME,  TIME,        "mdi:clock-time-eight-outline",        0x0000, 0x0000,  ZB_NO},
    {0,   "Free RAM",                       "free-ram",    &linky_free_heap_size,          UINT32,       0,      ANY,  C_ANY,   G_ANY,     REAL_TIME,  BYTES,       "",                                    0x0000, 0x0000,  ZB_NO},

};
const int32_t LinkyLabelListSize = sizeof(LinkyLabelList) / sizeof(LinkyLabelList[0]);
// clang-format on

LinkyData linky_data; // The data
linky_mode_t linky_mode = MODE_HIST;
uint8_t linky_tree_phase = 0;
uint8_t linky_reading = 0;
uint8_t linky_want_debug_frame = 0;
char linky_buffer[LINKY_BUFFER_SIZE] = {0}; // The UART buffer

const char *const HADeviceClassStr[] = {
    [NONE_CLASS] = "",
    [CURRENT] = "current",
    [POWER_VA] = "apparent_power",
    [POWER_kVA] = "power",
    [POWER_W] = "power",
    [POWER_Q] = "power",
    [ENERGY] = "energy",
    [ENERGY_Q] = "energy",
    [TIMESTAMP] = "timestamp",
    [TENSION] = "voltage",
    [TEXT] = "",
    [TIME] = "duration",
    [BOOL] = "binary_sensor",
    [BYTES] = "",
};

const char *const HAUnitsStr[] = {
    [NONE_CLASS] = "",
    [CURRENT] = "A",
    [POWER_VA] = "VA",
    [POWER_kVA] = "kVA",
    [POWER_W] = "W",
    [POWER_Q] = "VAr",
    [ENERGY] = "Wh",
    [ENERGY_Q] = "VArh",
    [TIMESTAMP] = "",
    [TENSION] = "V",
    [TEXT] = "",
    [TIME] = "s",
    [BOOL] = "",
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
};

/*==============================================================================
 Local Variable
===============================================================================*/
static char linky_uart_rx = 0;               // The RX pin of the linky
static char *linky_frame = NULL;             // The received frame from the linky
static uint8_t linky_group_separator = 0x20; // The group separator character (changes depending on the mode) (0x20 in historique mode, 0x09 in standard mode)
static uint16_t linky_frame_size = 0;        // The size of the frame
static uint32_t linky_rx_bytes = 0;          // store the number of bytes read

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
    esp_log_level_set(TAG, ESP_LOG_INFO);

    switch (config_values.linkyMode)
    {
    case AUTO:
        ESP_LOGI(TAG, "Trying to autodetect Linky mode, testing last known mode: %s", (linky_mode == MODE_HIST) ? "MODE_HISTORIQUE" : "MODE_STANDARD");
        linky_set_mode(config_values.linkyMode);
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

    // esp_log_level_set(TAG, ESP_LOG_DEBUG);
}

void linky_set_mode(linky_mode_t newMode)
{
    LinkyData empty;
    memset(&empty, 0, sizeof empty);

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
    ESP_LOGI(TAG, "Changed mode to %s", (newMode == MODE_HIST) ? "MODE_HISTORIQUE" : "MODE_STANDARD");
    uart_driver_delete(UART_NUM_1);
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
static void linky_read()
{
    uint32_t timeout = (xTaskGetTickCount() * portTICK_PERIOD_MS) + 5000; // 5 seconds timeout
    memset(linky_buffer, 0, sizeof linky_buffer);                         // clear the buffer
    linky_rx_bytes = 0;
    uart_flush(UART_NUM_1); // clear the UART buffer

    uint32_t startOfFrame = UINT_MAX; // store the index of the start frame
    uint32_t endOfFrame = UINT_MAX;   // store the index of the end frame

    if (linky_want_debug_frame)
    {
        linky_create_debug_frame();
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

    if (endOfFrame == UINT_MAX || startOfFrame == UINT_MAX || (startOfFrame > endOfFrame)) // if a start of frame and an end of frame has been found
    {
        ESP_LOGE(TAG, "Error: Frame not found");
        linky_frame_size = 0;
        linky_frame = NULL;
        return;
    }
    else
    {
        ESP_LOGD(TAG, "Start of frame: %lu", startOfFrame);
        ESP_LOGD(TAG, "End of frame: %lu", endOfFrame);
        linky_frame_size = endOfFrame - startOfFrame;
        linky_frame = linky_buffer + startOfFrame;
    }
    ESP_LOG_BUFFER_HEXDUMP(TAG, linky_buffer, linky_rx_bytes, ESP_LOG_DEBUG);
    ESP_LOGD(TAG, "-------------------");
    ESP_LOGD(TAG, "Buffer: %s", linky_buffer);
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
    if (!linky_frame)
    {
        if (config_values.linkyMode == AUTO)
        {
            switch (linky_mode)
            {
            case MODE_HIST:
                if (strlen(linky_data.hist.ADCO) > 0)
                {
                    ESP_LOGI(TAG, "Auto mode: Mode Historique Found!");
                    config_values.linkyMode = MODE_HIST;
                    config_write();
                }
                else
                {
                    ESP_LOGI(TAG, "Auto mode: Mode Historique Not Found! Try Mode Standard");
                    linky_set_mode(MODE_STD);
                }
                break;
            case MODE_STD:
                if (strlen(linky_data.std.ADSC) > 0)
                {
                    ESP_LOGI(TAG, "Auto mode: Mode Standard Found!");
                    config_values.linkyMode = MODE_STD;
                    config_write();
                }
                else
                {
                    ESP_LOGI(TAG, "Auto mode: Mode Standard Not Found! Try Mode Historique");
                    linky_set_mode(MODE_HIST);
                }
                break;
            default:
                break;
            }
        }

        return 0;
    }
    // ESP_LOG_BUFFER_HEXDUMP(TAG, frame, endOfFrame - startOfFrame, ESP_LOG_INFO);
    //-------------------------------------
    // Second step: Find goups of data in the frame
    //-------------------------------------
    unsigned int startOfGroup[GROUP_COUNT] = {UINT_MAX}; // store starts index of each group
    unsigned int endOfGroup[GROUP_COUNT] = {UINT_MAX};   // store ends index of each group
    unsigned int startOfGroupIndex = 0;                  // store the current index of starts of group array
    unsigned int endOfGroupIndex = 0;                    // store the current index of ends of group array
    for (unsigned int i = 0; i < linky_frame_size; i++)  // for each character in the frame
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

        char label[10] = {0};   // store the label as a string
        char value[100] = {0};  // store the data as a string
        char time[15] = {0};    // store the time as a string (H081225223518)
        char checksum[5] = {0}; // store the checksum as a string

        //-----------------------------------------------------------------------------------------------------------------replace to MEMCOPY
        memcpy(label, linky_frame + startOfGroup[i] + 1, separators[0] - startOfGroup[i] - 1); // copy the label from the group
        memcpy(value, linky_frame + separators[0] + 1, separators[1] - separators[0] - 1);     // copy the data from the group
        if (linky_mode == MODE_STD && separatorIndex == 3)                                     // if the mode is standard and the number of separators is 3
        {
            memcpy(time, linky_frame + separators[1] + 1, separators[2] - separators[1] - 1);     // copy the time from the group
            memcpy(checksum, linky_frame + separators[2] + 1, endOfGroup[i] - separators[2] - 1); // copy the checksum from the group
        }
        else
        {
            memcpy(checksum, linky_frame + separators[1] + 1, endOfGroup[i] - separators[1] - 1); // copy the checksum from the group
        }
        // ESP_LOGI(TAG, "label: %s value: %s checksum: %s", label, value, checksum);

        if (linky_checksum(label, value, time) != checksum[0]) // check the checksum with the label, data and time
        {
            // error: checksum is not correct, skip the field
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
                    switch (LinkyLabelList[j].type)
                    {
                    case STRING:
                        strncpy((char *)LinkyLabelList[j].data, value, LinkyLabelList[j].size);
                        break;
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
                        TimeLabel timeLabel = {0};
                        timeLabel.timestamp = linky_decode_time(time);
                        timeLabel.value = strtoull(value, NULL, 10);
                        *(TimeLabel *)LinkyLabelList[j].data = timeLabel;
                        break;
                    }
                    default:
                        break;
                    }
                }
            }
        }
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
    linky_reading = 1;
    xTaskCreate(gpio_led_task_linky_reading, "gpio_led_task_linky_reading", 2048, NULL, 10, NULL);
    linky_read();       // read the UART
    if (linky_decode()) // decode the frame
    {
        linky_reading = 0;
        return 1;
    }
    linky_reading = 0;
    return 0;
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

        if (linky_mode != LinkyLabelList[i].mode)
            continue;
        switch (LinkyLabelList[i].type)
        {
        case STRING:
            if (strlen((char *)LinkyLabelList[i].data) > 0) // print only if we have a value
                ESP_LOGI(TAG, "%s: %s", LinkyLabelList[i].label, (char *)LinkyLabelList[i].data);
            break;
        case UINT8:
            if (*(uint8_t *)LinkyLabelList[i].data != UINT8_MAX) // print only if we have a value
                ESP_LOGI(TAG, "%s: %u", LinkyLabelList[i].label, *(uint8_t *)LinkyLabelList[i].data);
            break;
        case UINT16:
            if (*(uint16_t *)LinkyLabelList[i].data != UINT16_MAX) // print only if we have a value
                ESP_LOGI(TAG, "%s: %u", LinkyLabelList[i].label, *(uint16_t *)LinkyLabelList[i].data);
            break;
        case UINT32:
            if (*(uint32_t *)LinkyLabelList[i].data != UINT32_MAX) // print only if we have a value
                ESP_LOGI(TAG, "%s: %lu", LinkyLabelList[i].label, *(uint32_t *)LinkyLabelList[i].data);
            break;
        case UINT64:
            if (*(uint64_t *)LinkyLabelList[i].data != UINT64_MAX) // print only if we have a value
                ESP_LOGI(TAG, "%s: %llu", LinkyLabelList[i].label, *(uint64_t *)LinkyLabelList[i].data);
            break;
        case UINT32_TIME:
        {
            TimeLabel timeLabel = *(TimeLabel *)LinkyLabelList[i].data;
            if (timeLabel.value != 0) // print only if we have a value
            {
                struct tm *timeinfo = localtime(&timeLabel.timestamp);
                char timeString[20];
                strftime(timeString, sizeof(timeString), "%d/%m/%Y %H:%M:%S", timeinfo);
                ESP_LOGI(TAG, "%s: %s %lu", LinkyLabelList[i].label, timeString, timeLabel.value);
            }
            break;
        }
        default:
            break;
        }
    }
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
    int S1 = 0;                                    // sum of the ASCII codes of the characters in the label
    for (int i = 0; i < strlen(label); i++)        // for each character in the label
    {                                              //
        S1 += label[i];                            // add the ASCII code of the label character to the sum
    }                                              //
    S1 += linky_group_separator;                   // add the ASCII code of the separator to the sum
    for (int i = 0; i < strlen(data); i++)         // for each character in the data
    {                                              //
        S1 += data[i];                             // add the ASCII code of the data character to the sum
    }                                              //
    if (linky_mode == MODE_STD)                    // if the mode is standard
    {                                              //
        S1 += linky_group_separator;               // add the ASCII code of the separator to the sum
        if (time != NULL && strlen(time) != 0)     //
        {                                          //
            for (int i = 0; i < strlen(time); i++) // for each character in the time
            {                                      //
                S1 += time[i];                     // add the ASCII code of the time character to the sum
            }                                      //
            S1 += linky_group_separator;           //
        }                                          //
    }                                              //
    return (S1 & 0x3F) + 0x20;                     // return the checksum
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

static void linky_create_debug_frame()
{
    // debug frame:
    //     ADCO 031976306475 J
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
    struct debugGroup
    {
        char name[10];
        char value[20];
        char checksum;
    };
    struct debugGroup debugGroups[16] = {
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

    // random base value
    snprintf(debugGroups[3].value, sizeof(debugGroups[3].value), "%ld", esp_random() % 1000000);
    debugGroups[3].checksum = linky_checksum(debugGroups[3].name, debugGroups[3].value, NULL);

    const uint16_t debugGroupCount = sizeof(debugGroups) / sizeof(debugGroups[0]);
    linky_rx_bytes = 0;
    linky_buffer[linky_rx_bytes++] = START_OF_FRAME;
    for (uint16_t i = 0; i < debugGroupCount - 1; i++)
    {
        linky_buffer[linky_rx_bytes++] = START_OF_GROUP;
        for (uint16_t j = 0; j < strlen(debugGroups[i].name); j++)
        {
            linky_buffer[linky_rx_bytes++] = debugGroups[i].name[j];
        }
        linky_buffer[linky_rx_bytes++] = linky_group_separator;
        for (uint16_t j = 0; j < strlen(debugGroups[i].value); j++)
        {
            linky_buffer[linky_rx_bytes++] = debugGroups[i].value[j];
        }
        linky_buffer[linky_rx_bytes++] = linky_group_separator;
        linky_buffer[linky_rx_bytes++] = debugGroups[i].checksum;
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
        if (LinkyLabelList[i].data == &config_values.refreshRate)
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
            TimeLabel timeLabel = {0};
            *(TimeLabel *)LinkyLabelList[i].data = timeLabel;
            break;
        }
        default:
            break;
        }
    }
}