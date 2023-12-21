/**
 * @file linky.h
 * @author Dorian Benech
 * @brief
 * @version 1.0
 * @date 2023-10-11
 *
 * @copyright Copyright (c) 2023 GammaTroniques
 *
 */

#ifndef Linky_H
#define Linky_H

/*==============================================================================
 Local Include
===============================================================================*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "string.h"
#include "esp_zigbee_core.h"

/*==============================================================================
 Public Defines
==============================================================================*/

/*==============================================================================
 Public Macro
==============================================================================*/

/*==============================================================================
 Public Type
==============================================================================*/
typedef struct
{
    uint32_t value;
    time_t timestamp;
} TimeLabel;

// clang-format off
typedef struct
{
    //  Variables                   Taille      Unité       Description
    //-----------------------------------------------------------------------
    char ADCO[13];       //    12                     Adresse du compteur 
    char OPTARIF[5];     //     4                     Option tarifaire choisie
    uint32_t ISOUSC;     //     2         A           Intensité souscrite 

    uint64_t BASE;       //     9         Wh          Index option Base 
    //----------------------Index option Heures Creuses ----------------------
    uint64_t HCHC;       //     9         Wh          Index option Heures Creuses: Heures Creuses
    uint64_t HCHP;       //     9         Wh          Index option Heures Creuses: Heures Pleines
    
    //---------------------- Index option EJP  (Effacement des Jours de Pointe) --> 22 jours par an prix du kWh plus cher
    uint64_t EJPHN;      //     9         Wh          Heures Normales
    uint64_t EJPHPM;     //     9         Wh          Heures de Pointe Mobile
    uint64_t PEJP;       //     2         minutes     Préavis Début EJP (30 min)

    //---------------------- Index option Tempo ----------------------
    uint64_t BBRHCJB;    //     9         Wh          Heures Creuses Jours Bleus
    uint64_t BBRHPJB;    //     9         Wh          Heures Pleines Jours Bleus
    uint64_t BBRHCJW;    //     9         Wh          Heures Creuses Jours Blancs
    uint64_t BBRHPJW;    //     9         Wh          Heures Pleines Jours Blancs
    uint64_t BBRHCJR;    //     9         Wh          Heures Creuses Jours Rouges
    uint64_t BBRHPJR;    //     9         Wh          Heures Pleines Jours Rouges

    char PTEC[5];        //     4                     Période Tarifaire en cours: TH.. Heures Creuses, HP.. Heures Pleines, HC.. Heures Creuses, HN.. Heures Normales, PM.. Heures de Pointe Mobile, HCJB.. Heures Creuses Jours Bleus, HPJB.. Heures Pleines Jours Bleus, HCJW.. Heures Creuses Jours Blancs, HPJW.. Heures Pleines Jours Blancs, HCJR.. Heures Creuses Jours Rouges, HPJR.. Heures Pleines Jours Rouges
    char DEMAIN[5];      //     4                     Couleur du lendemain 

    uint16_t IINST;      //     3         A           Intensité Instantanée
    uint16_t IINST1;     //     3         A           Intensité Instantanée Phase 1
    uint16_t IINST2;     //     3         A           Intensité Instantanée Phase 2
    uint16_t IINST3;     //     3         A           Intensité Instantanée Phase 3
    uint16_t IMAX;       //     3         A           Intensité maximale appelée
    uint16_t IMAX1;      //     3         A           Intensité maximale appelée Phase 1
    uint16_t IMAX2;      //     3         A           Intensité maximale appelée Phase 2
    uint16_t IMAX3;      //     3         A           Intensité maximale appelée Phase 3
    uint16_t ADPS;       //     3         A           Avertissement de Dépassement de Puissance Souscrite
    uint16_t ADIR1;      //     3         A           Avertissement de Dépassement d'intensité de réglage Phase 1
    uint16_t ADIR2;      //     3         A           Avertissement de Dépassement d'intensité de réglage Phase 2
    uint16_t ADIR3;      //     3         A           Avertissement de Dépassement d'intensité de réglage Phase 3

    uint32_t PAPP;       //     5         VA          Puissance apparente
    uint32_t PMAX;       //     5         W           Puissance maximale triphasée atteinte 
    uint32_t PPOT;       //     2                     Présence des potentiels??????????????????????????????

    char HHPHC[4];       //     1                     Horaire Heures Pleines Heures Creuses
    char MOTDETAT[7];    //     6                     Mot d'état du compteur                            

} LinkyDataHist;


typedef struct
{
    //  Variables                           Taille      Unité       Description
    //--------------------------------------------------------------------------------------
    char ADSC[13];       //     12                   Adresse Secondaire du Compteur
    char VTIC[3];        //     2                    Version de la TIC
    TimeLabel DATE;      //     0                    Date du jour
    char NGTF[17];       //     16                   Nom du calendrier tarifaire fournisseur
    char LTARF[17];      //     16                   Libellé du calendrier tarifaire
    
    uint32_t EAST;       //     9         Wh         Energie active soutirée totale
    uint32_t EASF01;     //     9         Wh         Energie active soutirée Fournisseur, index 01
    uint32_t EASF02;     //     9         Wh         Energie active soutirée Fournisseur, index 02
    uint32_t EASF03;     //     9         Wh         Energie active soutirée Fournisseur, index 03
    uint32_t EASF04;     //     9         Wh         Energie active soutirée Fournisseur, index 04
    uint32_t EASF05;     //     9         Wh         Energie active soutirée Fournisseur, index 05
    uint32_t EASF06;     //     9         Wh         Energie active soutirée Fournisseur, index 06
    uint32_t EASF07;     //     9         Wh         Energie active soutirée Fournisseur, index 07
    uint32_t EASF08;     //     9         Wh         Energie active soutirée Fournisseur, index 08
    uint32_t EASF09;     //     9         Wh         Energie active soutirée Fournisseur, index 09
    uint32_t EASF10;     //     9         Wh         Energie active soutirée Fournisseur, index 10

    uint32_t EASD01;     //      9         Wh         Energie active soutirée Distributeur, index 01
    uint32_t EASD02;     //      9         Wh         Energie active soutirée Distributeur, index 02
    uint32_t EASD03;     //      9         Wh         Energie active soutirée Distributeur, index 03
    uint32_t EASD04;     //      9         Wh         Energie active soutirée Distributeur, index 04

    uint32_t EAIT;       //      9         Wh         Energie active injectée totale

    uint32_t ERQ1;       //      9         Wh         Energie réactive Q1 totale 
    uint32_t ERQ2;       //      9         Wh         Energie réactive Q2 totale
    uint32_t ERQ3;       //      9         Wh         Energie réactive Q3 totale
    uint32_t ERQ4;       //      9         Wh         Energie réactive Q4 totale

    uint16_t IRMS1;      //      3         A          Courant efficace, phase 1
    uint16_t IRMS2;      //      3         A          Courant efficace, phase 2
    uint16_t IRMS3;      //      3         A          Courant efficace, phase 3

    uint16_t URMS1;      //      3         V          Tension efficace, phase 1
    uint16_t URMS2;      //      3         V          Tension efficace, phase 2
    uint16_t URMS3;      //      3         V          Tension efficace, phase 3

    uint8_t PREF;        //      2         kVA        Puissance app. de référence (PREF) 
    uint8_t PCOUP;       //      2         kVA        Puissance app. de coupure (PCOUP)

    uint32_t SINSTS;     //      5         VA         Puissance apparente soutirée instantanée
    uint32_t SINSTS1;    //      5         VA         Puissance apparente soutirée instantanée, phase 1
    uint32_t SINSTS2;    //      5         VA         Puissance apparente soutirée instantanée, phase 2
    uint32_t SINSTS3;    //      5         VA         Puissance apparente soutirée instantanée, phase 3

    TimeLabel SMAXSN;    //      5         VA         Puissance app. max. soutirée n avec date et heure
    TimeLabel SMAXSN1;   //      5         VA         Puissance app. max. soutirée n avec date et heure, phase 1
    TimeLabel SMAXSN2;   //      5         VA         Puissance app. max. soutirée n avec date et heure, phase 2
    TimeLabel SMAXSN3;   //      5         VA         Puissance app. max. soutirée n avec date et heure, phase 3

    TimeLabel SMAXSN_1;  //      5         VA         Puissance app max. soutirée n-1 
    TimeLabel SMAXSN1_1; //      5         VA         Puissance app max. soutirée n-1, phase 1
    TimeLabel SMAXSN2_1; //      5         VA         Puissance app max. soutirée n-1, phase 2
    TimeLabel SMAXSN3_1; //      5         VA         Puissance app max. soutirée n-1, phase 3

    uint32_t SINSTI;     //      5         VA         Puissance app. Instantanée injectée
    
    TimeLabel SMAXIN;    //      5         VA         Puissance app. max. injectée n avec date et heure
    TimeLabel SMAXIN_1;  //      5         VA         Puissance app. max. injectée n-1 avec date et heure

    TimeLabel CCASN;     //      5         VA         Point n de la courbe de charge active soutirée
    TimeLabel CCASN_1;   //      5         VA         Point n-1 de la courbe de charge active soutirée
    TimeLabel CCAIN;     //      5         VA         Point n de la courbe de charge active injectée
    TimeLabel CCAIN_1;   //      5         VA         Point n-1 de la courbe de charge active injectée

    TimeLabel UMOY1;     //      3         V          Tension moyenne, phase 1
    TimeLabel UMOY2;     //      3         V          Tension moyenne, phase 2
    TimeLabel UMOY3;     //      3         V          Tension moyenne, phase 3

    char STGE[9];        //      8         -          Registre de Statuts

    TimeLabel DPM1;      //      2         -          Début Pointe Mobile 1
    TimeLabel FPM1;      //      2         -          Fin Pointe Mobile 1
    TimeLabel DPM2;      //      2         -          Début Pointe Mobile 2
    TimeLabel FPM2;      //      2         -          Fin Pointe Mobile 2
    TimeLabel DPM3;      //      2         -          Début Pointe Mobile 3
    TimeLabel FPM3;      //      2         -          Fin Pointe Mobile 3

    char MSG1[33];       //      32        -          Message court
    char MSG2[17];       //      16        -          Message Ultra court 
    char PRM[15];        //      14        -          PRM En mode standard la TIC retransmet le PRM.
    char RELAIS[4];      //      3         -          Etat des relais: Les données transmises correspondent à l’état des 8 relais dont 1 réel et 7 virtuels.
    char NTARF[3];       //      2         -          Numéro de l’index tarifaire en cours
    char NJOURF[3];      //      2         -          Numéro du jour en cours calendrier fournisseur
    char NJOURF_1[3];    //      2         -          Numéro du prochain jour calendrier fournisseur
    char PJOURF_1[99];   //      98        -          Profil du prochain jour calendrier fournisseur 
    char PPOINTE[99];    //      98        -          Profil du prochain jour de pointe

}LinkyDataStd;

// clang-format on
typedef enum
{
    UINT8 = 1,
    UINT16 = 2,
    UINT32 = 4,
    UINT64 = 8,
    STRING = 0,
    UINT32_TIME = 12,
    HA_NUMBER = 13,
    BLOB = 14,
} LinkyLabelType;

typedef enum
{
    MODE_HIST,
    MODE_STD,
    AUTO,
    NONE,
    ANY,
} linky_mode_t;

typedef enum
{
    C_ANY,
    C_BASE,
    C_HCHC,
    C_EJP,
    C_TEMPO,
} linky_contract_t;

typedef enum
{
    G_ANY,
    G_MONO,
    G_TRI,
} linky_grid_t;

typedef enum
{
    STATIC_VALUE,
    REAL_TIME
} real_time_t;

typedef enum
{
    NONE_CLASS,
    CURRENT,
    POWER_VA,
    POWER_kVA,
    POWER_W,
    POWER_Q,
    ENERGY,
    ENERGY_Q,
    TIMESTAMP,
    TENSION,
    TEXT,
    TIME,
    BOOL,
    BYTES,
} HADeviceClass;

typedef struct
{
    uint8_t id;
    const char *name;
    const char *label;
    void *data;
    const LinkyLabelType type;
    const uint8_t size;
    const linky_mode_t mode;
    const linky_contract_t contract;
    const linky_grid_t grid;
    const real_time_t realTime;
    const HADeviceClass device_class;
    const char *icon;
    const uint16_t clusterID;
    const uint16_t attributeID;
    const esp_zb_zcl_attr_access_t zb_access;
} LinkyGroup;

typedef struct
{
    LinkyDataHist hist;
    LinkyDataStd std;
    time_t timestamp;
} LinkyData;

/*==============================================================================
 Public Variables Declaration
==============================================================================*/
extern const char *const HADeviceClassStr[];
extern const char *const HAUnitsStr[];
extern const char *const ha_sensors_str[];

extern const LinkyGroup LinkyLabelList[];
extern const int32_t LinkyLabelListSize;
extern LinkyData linky_data; // The data
extern linky_mode_t linky_mode;
extern uint8_t linky_tree_phase;
extern uint8_t linky_reading;
extern uint8_t linky_want_debug_frame;
extern uint32_t linky_free_heap_size;
extern uint64_t linky_uptime;
/*==============================================================================
 Public Functions Declaration
==============================================================================*/

/**
 * @brief Init the linky
 *
 * @param mode: MODE_HIST, MODE_STD OR AUTO
 * @param RX: The RX pin of the Linky
 */
void linky_init(linky_mode_t mode, int RX);

/**
 * @brief Get new data from the linky (read, decode and store in linky_data)
 *
 * @return char: 1 if success, 0 if error
 */
char linky_update(); // Update the data

/**
 * @brief Print all data read from the linky
 *        If no data, the print will be empty
 *
 */
void linky_print();

/**
 * @brief Set the current mode of the linky
 *
 * @param mode: MODE_HIST or MODE_STD
 */

void linky_set_mode(linky_mode_t mode);

/**
 * @brief Get if a linky is correctly connected
 *
 * @return uint8_t 1 if connected, 0 if not
 */
uint8_t linky_presence();

#endif /* Linky_H */