#ifndef Linky_H
#define Linky_H
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "string.h"

// clang-format off
// #define MODE_HISTORIQUE 0
// #define MODE_STANDARD   1

#define START_OF_FRAME  0x02 // The start of frame character
#define END_OF_FRAME    0x03   // The end of frame character

#define START_OF_GROUP  0x0A  // The start of group character
#define END_OF_GROUP    0x0D    // The end of group character


#define RX_BUF_SIZE     1024 // The size of the UART buffer
#define BUFFER_SIZE     1024 // The size of the UART buffer
#define FRAME_SIZE      500   // The size of one frame buffer
#define GROUP_COUNT     15

#define LINKY_TAG "Linky"

typedef struct
{
    //  Variables                   Taille      Unité       Description
    //-----------------------------------------------------------------------
    char ADCO[13]       = {0}; //    12                     Adresse du compteur 
    char OPTARIF[5]     = {0}; //     4                     Option tarifaire choisie
    uint32_t ISOUSC     = 0;   //     2         A           Intensité souscrite 

    uint32_t BASE       = 0;   //     9         Wh          Index option Base 
    //----------------------Index option Heures Creuses ----------------------
    uint32_t HCHC       = 0;   //     9         Wh          Index option Heures Creuses: Heures Creuses
    uint32_t HCHP       = 0;   //     9         Wh          Index option Heures Creuses: Heures Pleines
    
    //---------------------- Index option EJP  (Effacement des Jours de Pointe) --> 22 jours par an prix du kWh plus cher
    uint32_t EJPHN      = 0;   //     9         Wh          Heures Normales
    uint32_t EJPHPM     = 0;   //     9         Wh          Heures de Pointe Mobile
    uint32_t PEJP       = 0;   //     2         minutes     Préavis Début EJP (30 min)

    //---------------------- Index option Tempo ----------------------
    uint32_t BBRHCJB    = 0;   //     9         Wh          Heures Creuses Jours Bleus
    uint32_t BBRHPJB    = 0;   //     9         Wh          Heures Pleines Jours Bleus
    uint32_t BBRHCJW    = 0;   //     9         Wh          Heures Creuses Jours Blancs
    uint32_t BBRHPJW    = 0;   //     9         Wh          Heures Pleines Jours Blancs
    uint32_t BBRHCJR    = 0;   //     9         Wh          Heures Creuses Jours Rouges
    uint32_t BBRHPJR    = 0;   //     9         Wh          Heures Pleines Jours Rouges
    
    char PTEC[5]        = {0}; //     4                     Période Tarifaire en cours: TH.. Heures Creuses, HP.. Heures Pleines, HC.. Heures Creuses, HN.. Heures Normales, PM.. Heures de Pointe Mobile, HCJB.. Heures Creuses Jours Bleus, HPJB.. Heures Pleines Jours Bleus, HCJW.. Heures Creuses Jours Blancs, HPJW.. Heures Pleines Jours Blancs, HCJR.. Heures Creuses Jours Rouges, HPJR.. Heures Pleines Jours Rouges
    char DEMAIN[5]      = {0}; //     4                     Couleur du lendemain 
    
    uint32_t IINST      = 0;   //     3         A           Intensité Instantanée
    uint32_t IINST1     = 0;   //     3         A           Intensité Instantanée Phase 1
    uint32_t IINST2     = 0;   //     3         A           Intensité Instantanée Phase 2
    uint32_t IINST3     = 0;   //     3         A           Intensité Instantanée Phase 3
    uint32_t IMAX       = 0;   //     3         A           Intensité maximale appelée
    uint32_t IMAX1      = 0;   //     3         A           Intensité maximale appelée Phase 1
    uint32_t IMAX2      = 0;   //     3         A           Intensité maximale appelée Phase 2
    uint32_t IMAX3      = 0;   //     3         A           Intensité maximale appelée Phase 3
    uint32_t ADPS       = 0;   //     3         A           Avertissement de Dépassement de Puissance Souscrite
    uint32_t ADIR1      = 0;   //     3         A           Avertissement de Dépassement d'intensité de réglage Phase 1
    uint32_t ADIR2      = 0;   //     3         A           Avertissement de Dépassement d'intensité de réglage Phase 2
    uint32_t ADIR3      = 0;   //     3         A           Avertissement de Dépassement d'intensité de réglage Phase 3
    
    uint32_t PAPP       = 0;   //     5         VA          Puissance apparente
    uint32_t PMAX       = 0;   //     5         W           Puissance maximale triphasée atteinte 
    uint32_t PPOT       = 0;   //     2                     Présence des potentiels??????????????????????????????

    char HHPHC[4]       = {0}; //     1                     Horaire Heures Pleines Heures Creuses
    char MOTDETAT[7]    = {0}; //     6                     Mot d'état du compteur                            
    
    time_t timestamp    = 0;
} LinkyData;

#define TYPE_STRING 0
#define TYPE_UINT8  1
#define TYPE_UINT16 2
#define TYPE_UINT32 3
#define TYPE_UINT48 4

struct LinkyGroup
{
    char label[10] = {0};
    void *data = NULL;
    uint8_t type = TYPE_STRING;
};

// clang-format on
enum LinkyMode
{
    MODE_HISTORIQUE,
    MODE_STANDARD,
    AUTO
}; // The state of the UART buffer
class Linky
{

public:
    Linky(LinkyMode mode, int RX, int TX); // Constructor
                                           //
    char update();                         // Update the data
    void print();                          // Print the data
                                           //
    LinkyData data;
    void begin(); // Begin the linky
    // void rx_task(void *arg);

    uint16_t index = 0;             // The index of the UART buffer
    char buffer[BUFFER_SIZE] = {0}; // The UART buffer
    LinkyMode mode = MODE_HISTORIQUE;

private:
    char UARTRX = 0;                // The RX pin of the linky
    char UARTTX = 0;                // The TX pin of the linky
    uint8_t GROUP_SEPARATOR = 0x20; // The group separator character (changes depending on the mode) (0x20 in historique mode, 0x09 in standard mode)

    void read();                                        // Read the UART buffer
    char decode();                                      // Decode the frame
    char checksum(char *label, char *data, char *time); // Check the checksum
};

extern Linky linky;

#endif