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
#include "string.h"
#include "esp_zigbee_core.h"
#include "time.h"

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
    time_t time;
} time_label_t;

// clang-format off
typedef struct
{
    //  Variables                   Taille      Unité       Description
    //-----------------------------------------------------------------------
    char ADCO[14];       //    12                     Adresse du compteur 
    char OPTARIF[6];     //     4                     Option tarifaire choisie
    uint32_t ISOUSC;     //     2         A           Intensité souscrite 
    uint32_t PREF;       //     2         kVA         Puissance max contrat --> Value computes with ISOUSC (ISOUSC * 200) 

    uint64_t TOTAL;      //     9         Wh          Index total
    uint64_t BASE;       //     9         Wh          Index option Base 
    //----------------------Index option Heures Creuses ----------------------
    uint64_t HCHC;       //     9         Wh          Index option Heures Creuses: Heures Creuses
    uint64_t HCHP;       //     9         Wh          Index option Heures Creuses: Heures Pleines
    
    //---------------------- Index option EJP  (Effacement des Jours de Pointe) --> 22 jours par an prix du kWh plus cher
    uint64_t EJPHN;      //     9         Wh          Heures Normales
    uint64_t EJPHPM;     //     9         Wh          Heures de Pointe Mobile
    uint16_t PEJP;       //     2         minutes     Préavis Début EJP (30 min)

    //---------------------- Index option Tempo ----------------------
    uint64_t BBRHCJB;    //     9         Wh          Heures Creuses Jours Bleus
    uint64_t BBRHPJB;    //     9         Wh          Heures Pleines Jours Bleus
    uint64_t BBRHCJW;    //     9         Wh          Heures Creuses Jours Blancs
    uint64_t BBRHPJW;    //     9         Wh          Heures Pleines Jours Blancs
    uint64_t BBRHCJR;    //     9         Wh          Heures Creuses Jours Rouges
    uint64_t BBRHPJR;    //     9         Wh          Heures Pleines Jours Rouges

    char PTEC[6];        //     4                     Période Tarifaire en cours: TH.. Heures Creuses, HP.. Heures Pleines, HC.. Heures Creuses, HN.. Heures Normales, PM.. Heures de Pointe Mobile, HCJB.. Heures Creuses Jours Bleus, HPJB.. Heures Pleines Jours Bleus, HCJW.. Heures Creuses Jours Blancs, HPJW.. Heures Pleines Jours Blancs, HCJR.. Heures Creuses Jours Rouges, HPJR.. Heures Pleines Jours Rouges
    char AUJOUR[10];      //     4                     Couleur du jour
    char DEMAIN[10];      //     4                     Couleur du lendemain 

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

    char HHPHC[5];       //     1                     Horaire Heures Pleines Heures Creuses
    char MOTDETAT[8];    //     6                     Mot d'état du compteur                            

} linky_data_hist;


typedef struct
{
    //  Variables                           Taille      Unité       Description
    //--------------------------------------------------------------------------------------
    char ADSC[14];       //     12                   Adresse Secondaire du Compteur
    char VTIC[4];        //     2                    Version de la TIC
    time_label_t DATE;      //     0                    Date du jour
    char NGTF[18];       //     16                   Nom du calendrier tarifaire fournisseur
    char LTARF[18];      //     16                   Libellé du calendrier tarifaire
    
    uint64_t EAST;       //     9         Wh         Energie active soutirée totale
    uint64_t EASF01;     //     9         Wh         Energie active soutirée Fournisseur, index 01
    uint64_t EASF02;     //     9         Wh         Energie active soutirée Fournisseur, index 02
    uint64_t EASF03;     //     9         Wh         Energie active soutirée Fournisseur, index 03
    uint64_t EASF04;     //     9         Wh         Energie active soutirée Fournisseur, index 04
    uint64_t EASF05;     //     9         Wh         Energie active soutirée Fournisseur, index 05
    uint64_t EASF06;     //     9         Wh         Energie active soutirée Fournisseur, index 06
    uint64_t EASF07;     //     9         Wh         Energie active soutirée Fournisseur, index 07
    uint64_t EASF08;     //     9         Wh         Energie active soutirée Fournisseur, index 08
    uint64_t EASF09;     //     9         Wh         Energie active soutirée Fournisseur, index 09
    uint64_t EASF10;     //     9         Wh         Energie active soutirée Fournisseur, index 10

    uint64_t EASD01;     //      9         Wh         Energie active soutirée Distributeur, index 01
    uint64_t EASD02;     //      9         Wh         Energie active soutirée Distributeur, index 02
    uint64_t EASD03;     //      9         Wh         Energie active soutirée Distributeur, index 03
    uint64_t EASD04;     //      9         Wh         Energie active soutirée Distributeur, index 04

    uint64_t EAIT;       //      9         Wh         Energie active injectée totale

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

    uint32_t PREF;        //      2         kVA        Puissance app. de référence (PREF) 
    uint8_t PCOUP;       //      2         kVA        Puissance app. de coupure (PCOUP)

    uint32_t SINSTS;     //      5         VA         Puissance apparente soutirée instantanée
    uint32_t SINSTS1;    //      5         VA         Puissance apparente soutirée instantanée, phase 1
    uint32_t SINSTS2;    //      5         VA         Puissance apparente soutirée instantanée, phase 2
    uint32_t SINSTS3;    //      5         VA         Puissance apparente soutirée instantanée, phase 3

    time_label_t SMAXSN;    //      5         VA         Puissance app. max. soutirée n avec date et heure
    time_label_t SMAXSN1;   //      5         VA         Puissance app. max. soutirée n avec date et heure, phase 1
    time_label_t SMAXSN2;   //      5         VA         Puissance app. max. soutirée n avec date et heure, phase 2
    time_label_t SMAXSN3;   //      5         VA         Puissance app. max. soutirée n avec date et heure, phase 3

    time_label_t SMAXSN_1;  //      5         VA         Puissance app max. soutirée n-1 
    time_label_t SMAXSN1_1; //      5         VA         Puissance app max. soutirée n-1, phase 1
    time_label_t SMAXSN2_1; //      5         VA         Puissance app max. soutirée n-1, phase 2
    time_label_t SMAXSN3_1; //      5         VA         Puissance app max. soutirée n-1, phase 3

    uint32_t SINSTI;     //      5         VA         Puissance app. Instantanée injectée
    
    time_label_t SMAXIN;    //      5         VA         Puissance app. max. injectée n avec date et heure
    time_label_t SMAXIN_1;  //      5         VA         Puissance app. max. injectée n-1 avec date et heure

    time_label_t CCASN;     //      5         VA         Point n de la courbe de charge active soutirée
    time_label_t CCASN_1;   //      5         VA         Point n-1 de la courbe de charge active soutirée
    time_label_t CCAIN;     //      5         VA         Point n de la courbe de charge active injectée
    time_label_t CCAIN_1;   //      5         VA         Point n-1 de la courbe de charge active injectée

    time_label_t UMOY1;     //      3         V          Tension moyenne, phase 1
    time_label_t UMOY2;     //      3         V          Tension moyenne, phase 2
    time_label_t UMOY3;     //      3         V          Tension moyenne, phase 3

    char STGE[10];        //      8         -          Registre de Statuts
    char AUJOUR[10];       //      4         -          Couleur du jour
    char DEMAIN[10];      //      4         -          Couleur du lendemain

    time_label_t DPM1;      //      2         -          Début Pointe Mobile 1
    time_label_t FPM1;      //      2         -          Fin Pointe Mobile 1
    time_label_t DPM2;      //      2         -          Début Pointe Mobile 2
    time_label_t FPM2;      //      2         -          Fin Pointe Mobile 2
    time_label_t DPM3;      //      2         -          Début Pointe Mobile 3
    time_label_t FPM3;      //      2         -          Fin Pointe Mobile 3

    char MSG1[34];       //      32        -          Message court
    char MSG2[18];       //      16        -          Message Ultra court 
    char PRM[16];        //      14        -          PRM En mode standard la TIC retransmet le PRM.
    char RELAIS[5];      //      3         -          Etat des relais: Les données transmises correspondent à l’état des 8 relais dont 1 réel et 7 virtuels.
    uint16_t NTARF;      //      2         -          Numéro de l’index tarifaire en cours
    uint16_t NJOURF;     //      2         -          Numéro du jour en cours calendrier fournisseur
    uint16_t NJOURF_1;   //      2         -          Numéro du prochain jour calendrier fournisseur
    char PJOURF_1[100];  //      98        -          Profil du prochain jour calendrier fournisseur 
    char PPOINTE[100];   //      98        -          Profil du prochain jour de pointe

}linky_data_std;

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
    BOOL
} linky_label_type_t;

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
    C_UNKNOWN,
    C_ANY,
    C_BASE,
    C_HC,
    C_HEURES_SUPER_CREUSES,
    C_EJP,
    C_TEMPO,
    C_ZEN_FLEX,
    C_SEM_WE_LUNDI,
    C_SEM_WE_MERCREDI,
    C_SEM_WE_VENDREDI,
    C_COUNT,
} linky_contract_t;

typedef enum
{
    T_ANY,
    T_BASE,
    T_HC,
    T_HP,
    T_HN, // Heure Normale
    T_PM, // Heure de Pointe Mobile
    T_HCJB,
    T_HPJB,
    T_HCJW,
    T_HPJW,
    T_HCJR,
    T_HPJR,
} linky_tarif_t;

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
    POWER_kW,
    POWER_Q,
    ENERGY,
    ENERGY_Q,
    TIMESTAMP,
    TENSION,
    TEXT,
    TIME,
    TIME_M, // ms
    CLASS_BOOL,
    BYTES,
} HADeviceClass;

typedef struct
{
    uint16_t id;
    const char *name;
    const char *label;
    void *data;
    const linky_label_type_t type;
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
    const esp_zb_zcl_attr_type_t zb_type;
} LinkyGroup;

typedef struct
{
    linky_data_hist hist;
    linky_data_std std;
    time_t timestamp;
    uint64_t uptime;
} linky_data_t;

typedef enum
{
    DEBUG_NONE,
    DEBUG_HIST,
    DEBUG_STD,
    DEBUG_BAD_STD,

} linky_debug_t;

/*==============================================================================
 Public Variables Declaration
==============================================================================*/
extern const char *const HADeviceClassStr[];
extern const char *const HAUnitsStr[];
extern const char *const ha_sensors_str[];
extern const char *const linky_hist_str_contract[];
extern const char *const linky_std_str_contract[];
extern const char *const linky_tuya_str_contract[];
extern const char *const linky_str_mode[];

extern const LinkyGroup LinkyLabelList[];
extern const int32_t LinkyLabelListSize;

extern linky_data_t linky_data;
extern linky_mode_t linky_mode;
extern linky_contract_t linky_contract;

extern uint8_t linky_three_phase;
extern uint8_t linky_reading;
extern linky_debug_t linky_debug;
extern uint32_t linky_free_heap_size;

extern const void *linky_protected_data[];
extern const uint8_t linky_protected_data_size;

extern uint32_t linky_frame_size;

/*==============================================================================
 Public Functions Declaration
==============================================================================*/

/**
 * @brief Init the linky
 *
 * @param mode: MODE_HIST, MODE_STD OR AUTO
 * @param RX: The RX pin of the Linky
 */
void linky_init(int RX);

/**
 * @brief Get new data from the linky (read, decode and store in linky_data)
 *
 * @return char: 1 if success, 0 if error
 */
char linky_update(bool clear);

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

void linky_print_debug_frame();

linky_contract_t linky_get_contract(linky_data_t *data);

void linky_stats();

#endif /* Linky_H */