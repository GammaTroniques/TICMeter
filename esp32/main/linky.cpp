#include <linky.h>
#include <config.h>
#include <time.h>
#include <gpio.h>
#include <wifi.h>

Linky linky(MODE_HISTORIQUE, RX_LINKY);

// clang-format off
const struct LinkyGroup LinkyLabelList[] =
{   
    //     Name                          Label           DataPtr                       Type          MODE
    //----------------------------------------------------------------------------------------------------------------
    //--------------------------- MODE HISTORIQUE --------------------------------
    {"Identifiant",                     "ADCO",        &linky.data.hist.ADCO,         STRING,       MODE_HISTORIQUE, STATIC_VALUE,  NONE_CLASS,  "mdi:card-account-details",      },
    {"Option tarifaire",                "OPTARIF",     &linky.data.hist.OPTARIF,      STRING,       MODE_HISTORIQUE, STATIC_VALUE,  NONE_CLASS,  "mdi:cash-multiple",             },
    {"Intensité souscrite",             "ISOUSC",      &linky.data.hist.ISOUSC,       UINT16,       MODE_HISTORIQUE, STATIC_VALUE,  CURRENT,     "",                                },
    
    {"Index Base",                      "BASE",        &linky.data.hist.BASE,         UINT32,       MODE_HISTORIQUE, STATIC_VALUE,  ENERGY,      "",                                },
    
    {"Index Heures Creuses",            "HCHC",        &linky.data.hist.HCHC,         UINT32,       MODE_HISTORIQUE, STATIC_VALUE,  ENERGY,      "",                                },
    {"Index Heures Pleines",            "HCHP",        &linky.data.hist.HCHP,         UINT32,       MODE_HISTORIQUE, STATIC_VALUE,  ENERGY,      "",                                },
    
    {"Index Heures Normales",           "EJPHN",       &linky.data.hist.EJPHN,        UINT32,       MODE_HISTORIQUE, STATIC_VALUE,  ENERGY,      "",                                },
    {"Index Heures de Pointe Mobile",   "EJPHPM",      &linky.data.hist.EJPHPM,       UINT32,       MODE_HISTORIQUE, STATIC_VALUE,  ENERGY,      "",                                },
    {"Préavis Début EJP",               "PEJP",        &linky.data.hist.PEJP,         UINT32,       MODE_HISTORIQUE, STATIC_VALUE,  BOOL,        "mdi:clock",                     },
    
    {"Heures Creuses Jours Bleus",      "BBRHCJB",     &linky.data.hist.BBRHCJB,      UINT32,       MODE_HISTORIQUE, STATIC_VALUE,  ENERGY,      "",                                },
    {"Heures Pleines Jours Bleus",      "BBRHPJB",     &linky.data.hist.BBRHPJB,      UINT32,       MODE_HISTORIQUE, STATIC_VALUE,  ENERGY,      "",                                },
    {"Heures Creuses Jours Blancs",     "BBRHCJW",     &linky.data.hist.BBRHCJW,      UINT32,       MODE_HISTORIQUE, STATIC_VALUE,  ENERGY,      "",                                },
    {"Heures Pleines Jours Blancs",     "BBRHPJW",     &linky.data.hist.BBRHPJW,      UINT32,       MODE_HISTORIQUE, STATIC_VALUE,  ENERGY,      "",                                },
    {"Heures Creuses Jours Rouges",     "BBRHCJR",     &linky.data.hist.BBRHCJR,      UINT32,       MODE_HISTORIQUE, STATIC_VALUE,  ENERGY,      "",                                },
    {"Heures Pleines Jours Rouges",     "BBRHPJR",     &linky.data.hist.BBRHPJR,      UINT32,       MODE_HISTORIQUE, STATIC_VALUE,  ENERGY,      "",                                },
    
    {"Période tarifaire en cours",      "PTEC",        &linky.data.hist.PTEC,         STRING,       MODE_HISTORIQUE, STATIC_VALUE,  NONE_CLASS,  "mdi:calendar-clock",            },
    {"Couleur du lendemain",            "DEMAIN",      &linky.data.hist.DEMAIN,       STRING,       MODE_HISTORIQUE, STATIC_VALUE,  NONE_CLASS,  "",                                },
    
    {"Intensité instantanée",           "IINST",       &linky.data.hist.IINST,        UINT16,       MODE_HISTORIQUE, REAL_TIME,     CURRENT,     "",                                },
    {"Intensité instantanée Phase 1",   "IINST1",      &linky.data.hist.IINST1,       UINT16,       MODE_HISTORIQUE, REAL_TIME,     CURRENT,     "",                                },
    {"Intensité instantanée Phase 2",   "IINST2",      &linky.data.hist.IINST2,       UINT16,       MODE_HISTORIQUE, REAL_TIME,     CURRENT,     "",                                },
    {"Intensité instantanée Phase 3",   "IINST3",      &linky.data.hist.IINST3,       UINT16,       MODE_HISTORIQUE, REAL_TIME,     CURRENT,     "",                                },
    {"Intensité maximale",              "IMAX",        &linky.data.hist.IMAX,         UINT16,       MODE_HISTORIQUE, STATIC_VALUE,  CURRENT,     "",                                },
    {"Intensité maximale Phase 1",      "IMAX1",       &linky.data.hist.IMAX1,        UINT16,       MODE_HISTORIQUE, STATIC_VALUE,  CURRENT,     "",                                },
    {"Intensité maximale Phase 2",      "IMAX2",       &linky.data.hist.IMAX2,        UINT16,       MODE_HISTORIQUE, STATIC_VALUE,  CURRENT,     "",                                },
    {"Intensité maximale Phase 3",      "IMAX3",       &linky.data.hist.IMAX3,        UINT16,       MODE_HISTORIQUE, STATIC_VALUE,  CURRENT,     "",                                },
    {"Dépassement Puissance",           "ADPS",        &linky.data.hist.ADPS,         UINT16,       MODE_HISTORIQUE, STATIC_VALUE,  CURRENT,     "",                                },
    {"Dépassement Intensité Phase 1",   "ADIR1",       &linky.data.hist.ADIR1,        UINT16,       MODE_HISTORIQUE, STATIC_VALUE,  CURRENT,     "",                                },
    {"Dépassement Intensité Phase 2",   "ADIR2",       &linky.data.hist.ADIR2,        UINT16,       MODE_HISTORIQUE, STATIC_VALUE,  CURRENT,     "",                                },
    {"Dépassement Intensité Phase 3",   "ADIR3",       &linky.data.hist.ADIR3,        UINT16,       MODE_HISTORIQUE, STATIC_VALUE,  CURRENT,     "",                                },
    
    {"Puissance apparente",             "PAPP",        &linky.data.hist.PAPP,         UINT32,       MODE_HISTORIQUE, REAL_TIME,     POWER_VA,    "",                                },
    {"Puissance maximale triphasée",    "PMAX",        &linky.data.hist.PMAX,         UINT32,       MODE_HISTORIQUE, STATIC_VALUE,  POWER_W,     "",                                },
    {"Présence des potentiels",         "PPOT",        &linky.data.hist.PPOT,         UINT32,       MODE_HISTORIQUE, REAL_TIME,     NONE_CLASS,  "",                                },
        
    {"Horaire Heures Creuses",          "HHPHC",       &linky.data.hist.HHPHC,        UINT8,        MODE_HISTORIQUE, STATIC_VALUE,  NONE_CLASS,  "mdi:home-clock",                },
    {"Mot d'état du compteur",          "MOTDETAT",    &linky.data.hist.MOTDETAT,     STRING,       MODE_HISTORIQUE, STATIC_VALUE,  NONE_CLASS,  "mdi:state-machine",             },

    //------------------------ MODE STANDARD -----------------------
    {"Identifiant",                     "ADSC",        &linky.data.std.ADSC,          STRING,       MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "mdi:card-account-details",      },
    {"Version de la TIC",               "VTIC",        &linky.data.std.VTIC,          STRING,       MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "mdi:tag",                       },
    {"Date et heure courante",          "DATE",        &linky.data.std.DATE,          UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "mdi:clipboard-text-clock",      },
    {"Nom du calendrier tarifaire",     "NGTF",        &linky.data.std.NGTF,          STRING,       MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "mdi:calendar-clock",            },
    {"Libellé tarif en cours",          "LTARF",       &linky.data.std.LTARF,         STRING,       MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "mdi:tag-text",                  },

    {"Index Total Energie soutirée",    "EAST",        &linky.data.std.EAST,          UINT32,       MODE_STANDARD,   STATIC_VALUE,  ENERGY,      "",                                },
    {"Index 1 Energie soutirée",        "EASF01",      &linky.data.std.EASF01,        UINT32,       MODE_STANDARD,   STATIC_VALUE,  ENERGY,      "",                                },
    {"Index 2 Energie soutirée",        "EASF02",      &linky.data.std.EASF02,        UINT32,       MODE_STANDARD,   STATIC_VALUE,  ENERGY,      "",                                },
    {"Index 3 Energie soutirée",        "EASF03",      &linky.data.std.EASF03,        UINT32,       MODE_STANDARD,   STATIC_VALUE,  ENERGY,      "",                                },
    {"Index 4 Energie soutirée",        "EASF04",      &linky.data.std.EASF04,        UINT32,       MODE_STANDARD,   STATIC_VALUE,  ENERGY,      "",                                },
    {"Index 5 Energie soutirée",        "EASF05",      &linky.data.std.EASF05,        UINT32,       MODE_STANDARD,   STATIC_VALUE,  ENERGY,      "",                                },
    {"Index 6 Energie soutirée",        "EASF06",      &linky.data.std.EASF06,        UINT32,       MODE_STANDARD,   STATIC_VALUE,  ENERGY,      "",                                },
    {"Index 7 Energie soutirée",        "EASF07",      &linky.data.std.EASF07,        UINT32,       MODE_STANDARD,   STATIC_VALUE,  ENERGY,      "",                                },
    {"Index 8 Energie soutirée",        "EASF08",      &linky.data.std.EASF08,        UINT32,       MODE_STANDARD,   STATIC_VALUE,  ENERGY,      "",                                },
    {"Index 9 Energie soutirée",        "EASF09",      &linky.data.std.EASF09,        UINT32,       MODE_STANDARD,   STATIC_VALUE,  ENERGY,      "",                                },
    {"Index 10 Energie soutirée",       "EASF10",      &linky.data.std.EASF10,        UINT32,       MODE_STANDARD,   STATIC_VALUE,  ENERGY,      "",                                },

    {"Index 1 Energie soutirée Distr",  "EASD01",      &linky.data.std.EASD01,        UINT32,       MODE_STANDARD,   STATIC_VALUE,  ENERGY,      "",                                },
    {"Index 2 Energie soutirée Distr",  "EASD02",      &linky.data.std.EASD02,        UINT32,       MODE_STANDARD,   STATIC_VALUE,  ENERGY,      "",                                },
    {"Index 3 Energie soutirée Distr",  "EASD03",      &linky.data.std.EASD03,        UINT32,       MODE_STANDARD,   STATIC_VALUE,  ENERGY,      "",                                },
    {"Index 4 Energie soutirée Distr",  "EASD04",      &linky.data.std.EASD04,        UINT32,       MODE_STANDARD,   STATIC_VALUE,  ENERGY,      "",                                },

    {"Energie injectée totale",         "EAIT",        &linky.data.std.EAIT,          UINT32,       MODE_STANDARD,   STATIC_VALUE,  ENERGY,      "mdi:transmission-tower-export", },

    {"Energie réactive Q1 totale",      "ERQ1",        &linky.data.std.ERQ1,          UINT32,       MODE_STANDARD,   STATIC_VALUE,  ENERGY_Q,    "",                                },
    {"Energie réactive Q2 totale",      "ERQ2",        &linky.data.std.ERQ2,          UINT32,       MODE_STANDARD,   STATIC_VALUE,  ENERGY_Q,    "",                                },
    {"Energie réactive Q3 totale",      "ERQ3",        &linky.data.std.ERQ3,          UINT32,       MODE_STANDARD,   STATIC_VALUE,  ENERGY_Q,    "",                                },
    {"Energie réactive Q4 totale",      "ERQ4",        &linky.data.std.ERQ4,          UINT32,       MODE_STANDARD,   STATIC_VALUE,  ENERGY_Q,    "",                                },

    {"Courant efficace Phase 1",        "IRMS1",       &linky.data.std.IRMS1,         UINT16,       MODE_STANDARD,   STATIC_VALUE,  CURRENT,     "",                                },
    {"Courant efficace Phase 2",        "IRMS2",       &linky.data.std.IRMS2,         UINT16,       MODE_STANDARD,   STATIC_VALUE,  CURRENT,     "",                                },
    {"Courant efficace Phase 3",        "IRMS3",       &linky.data.std.IRMS3,         UINT16,       MODE_STANDARD,   STATIC_VALUE,  CURRENT,     "",                                },

    {"Tension efficace Phase 1",        "URMS1",       &linky.data.std.URMS1,         UINT16,       MODE_STANDARD,   STATIC_VALUE,  TENSION,     "",                                },
    {"Tension efficace Phase 2",        "URMS2",       &linky.data.std.URMS2,         UINT16,       MODE_STANDARD,   STATIC_VALUE,  TENSION,     "",                                },
    {"Tension efficace Phase 3",        "URMS3",       &linky.data.std.URMS3,         UINT16,       MODE_STANDARD,   STATIC_VALUE,  TENSION,     "",                                },

    {"Puissance app. de référence",     "PREF",        &linky.data.std.PREF,          UINT8,        MODE_STANDARD,   STATIC_VALUE,  POWER_kVA,   "",                                },
    {"Puissance app. de coupure",       "PCOUP",       &linky.data.std.PCOUP,         UINT8,        MODE_STANDARD,   STATIC_VALUE,  POWER_kVA,   "",                                },

    {"Puissance soutirée",              "SINSTS",      &linky.data.std.SINSTS,        UINT32,       MODE_STANDARD,   STATIC_VALUE,  POWER_VA,    "",                                },
    {"Puissance soutirée Phase 1",      "SINSTS1",     &linky.data.std.SINSTS1,       UINT32,       MODE_STANDARD,   STATIC_VALUE,  POWER_VA,    "",                                },
    {"Puissance soutirée Phase 2",      "SINSTS2",     &linky.data.std.SINSTS2,       UINT32,       MODE_STANDARD,   STATIC_VALUE,  POWER_VA,    "",                                },
    {"Puissance soutirée Phase 3",      "SINSTS3",     &linky.data.std.SINSTS3,       UINT32,       MODE_STANDARD,   STATIC_VALUE,  POWER_VA,    "",                                },

    {"Puissance max soutirée Auj.",     "SMAXSN",      &linky.data.std.SMAXSN,        UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  POWER_VA,    "",                                },
    {"Puissance max soutirée Auj. 1",   "SMAXSN1",     &linky.data.std.SMAXSN1,       UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  POWER_VA,    "",                                },
    {"Puissance max soutirée Auj. 2",   "SMAXSN2",     &linky.data.std.SMAXSN2,       UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  POWER_VA,    "",                                },
    {"Puissance max soutirée Auj. 3",   "SMAXSN3",     &linky.data.std.SMAXSN3,       UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  POWER_VA,    "",                                },

    {"Puissance max soutirée Hier",     "SMAXSN-1",    &linky.data.std.SMAXSN_1,      UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  POWER_VA,    "",                                },
    {"Puissance max soutirée Hier 1",   "SMAXSN1-1",   &linky.data.std.SMAXSN1_1,     UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  POWER_VA,    "",                                },
    {"Puissance max soutirée Hier 2",   "SMAXSN2-1",   &linky.data.std.SMAXSN2_1,     UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  POWER_VA,    "",                                },
    {"Puissance max soutirée Hier 3",   "SMAXSN3-1",   &linky.data.std.SMAXSN3_1,     UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  POWER_VA,    "",                                },

    {"Puissance injectée",              "SINSTI",      &linky.data.std.SINSTI,        UINT32,       MODE_STANDARD,   STATIC_VALUE,  POWER_VA,    "mdi:transmission-tower-export", },
    {"Puissance max injectée Auj.",     "SMAXIN",      &linky.data.std.SMAXIN,        UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  POWER_VA,    "",                                },
    {"Puissance max injectée Hier",     "SMAXIN-1",    &linky.data.std.SMAXIN_1,      UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  POWER_VA,    "",                                },

    {"Point n courbe soutirée",         "CCASN",       &linky.data.std.CCASN,         UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "",                                },
    {"Point n-1 courbe soutirée",       "CCASN-1",     &linky.data.std.CCASN_1,       UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "",                                },
    {"Point n courbe injectée",         "CCAIN",       &linky.data.std.CCAIN,         UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "",                                },
    {"Point n-1 courbe injectée",       "CCAIN-1",     &linky.data.std.CCAIN_1,       UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "",                                },

    {"Tension moyenne Phase 1",         "UMOY1",       &linky.data.std.UMOY1,         UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  TENSION,     "",                                },
    {"Tension moyenne Phase 2",         "UMOY2",       &linky.data.std.UMOY2,         UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  TENSION,     "",                                },
    {"Tension moyenne Phase 3",         "UMOY3",       &linky.data.std.UMOY3,         UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  TENSION,     "",                                },

    {"Registre de Statuts",             "STGE",        &linky.data.std.STGE,          STRING,       MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "mdi:state-machine",             },

    {"Début Pointe Mobile 1",           "DPM1",        &linky.data.std.DPM1,          UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "",                                },
    {"Fin Pointe Mobile 1",             "FPM1",        &linky.data.std.FPM1,          UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "",                                },
    {"Début Pointe Mobile 2",           "DPM2",        &linky.data.std.DPM2,          UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "",                                },
    {"Fin Pointe Mobile 2",             "FPM2",        &linky.data.std.FPM2,          UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "",                                },
    {"Début Pointe Mobile 3",           "DPM3",        &linky.data.std.DPM3,          UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "",                                },
    {"Fin Pointe Mobile 3",             "FPM3",        &linky.data.std.FPM3,          UINT32_TIME,  MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "",                                },

    {"Message court",                   "MSG1",        &linky.data.std.MSG1,          STRING,       MODE_STANDARD,   STATIC_VALUE,  TEXT,        "mdi:message-text-outline",      },
    {"Message Ultra court",             "MSG2",        &linky.data.std.MSG2,          STRING,       MODE_STANDARD,   STATIC_VALUE,  TEXT,        "mdi:message-outline",           },
    {"PRM",                             "PRM",         &linky.data.std.PRM,           STRING,       MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "",                                },
    {"Relais",                          "RELAIS",      &linky.data.std.RELAIS,        STRING,       MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "mdi:toggle-switch-outline",     },
    {"Index tarifaire en cours",        "NTARF",       &linky.data.std.NTARF,         STRING,       MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "",                                },
    {"N° jours en cours fournisseur",   "NJOURF",      &linky.data.std.NJOURF,        STRING,       MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "",                                },
    {"N° prochain jour fournisseur",    "NJOURF+1",    &linky.data.std.NJOURF_1,      STRING,       MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "",                                },
    {"Profil du prochain jour",         "PJOURF+1",    &linky.data.std.MSG2,          STRING,       MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "mdi:sun-clock",                 },
    {"Profil du prochain jour pointe",  "PPOINTE",     &linky.data.std.PPOINTE,       STRING,       MODE_STANDARD,   STATIC_VALUE,  NONE_CLASS,  "mdi:sun-clock",                 },
    //---------------------------Home Assistant Specific ------------------------------------------------

    { "Dernière actualisation",         "timestamp",   &linky.data.timestamp,         UINT64,       ANY,             STATIC_VALUE,  TIMESTAMP,   ""                                 },
    { "Temps d'actualisation",          "currRfsh",    &config.values.refreshRate,    UINT16,       ANY,             STATIC_VALUE,  NONE_CLASS,  "mdi:refresh"                    },
    { "Temps d'actualisation",          "Refresh",     &config.values.refreshRate,    HA_NUMBER,    ANY,             STATIC_VALUE,  NONE_CLASS,  "mdi:refresh"                    },

};
const int32_t LinkyLabelListSize = sizeof(LinkyLabelList) / sizeof(LinkyLabelList[0]);
// clang-format on

/**
 * @brief Linky constructor
 *
 * @param mode MODE_STANDARD or MODE_HISTORIQUE
 * @param RX RX pin number for the UART
 */
Linky::Linky(LinkyMode mode, int RX)
{
    this->mode = mode;
    UARTRX = RX;
    // mode Historique: 0x20
    // mode Standard: 0x09
    GROUP_SEPARATOR = (mode == MODE_HISTORIQUE) ? 0x20 : 0x09;
    esp_log_level_set(LINKY_TAG, ESP_LOG_INFO);
}

/**
 * @brief Start the serial communication
 *
 */
void Linky::begin()
{
    switch (config.values.linkyMode)
    {
    case AUTO:
        ESP_LOGI(LINKY_TAG, "Trying to autodetect Linky mode, testing last known mode: %s", (mode == MODE_HISTORIQUE) ? "MODE_HISTORIQUE" : "MODE_STANDARD");
        setMode(config.values.linkyMode);
        break;
    case MODE_HISTORIQUE:
        setMode(MODE_HISTORIQUE);
        break;
    case MODE_STANDARD:
        setMode(MODE_STANDARD);
        break;
    default:
        break;
    }
}

void Linky::setMode(LinkyMode newMode)
{
    this->mode = newMode;
    switch (newMode)
    {
    case MODE_HISTORIQUE:
        this->data.hist = {}; // reset all values
        break;
    case MODE_STANDARD:
        this->data.std = {};
        break;
    default:
        break;
    }
    ESP_LOGI(LINKY_TAG, "Changed mode to %s", (newMode == MODE_HISTORIQUE) ? "MODE_HISTORIQUE" : "MODE_STANDARD");
    uart_driver_delete(UART_NUM_1);
    uart_config_t uart_config = {
        .data_bits = UART_DATA_7_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
    };
    switch (mode)
    {
    case MODE_HISTORIQUE:
        // start the serial communication at 1200 bauds, 7E1
        uart_config.baud_rate = 1200;
        mode = MODE_HISTORIQUE;
        GROUP_SEPARATOR = 0x20;
        break;
    case MODE_STANDARD:
        // start the serial communication at 9600 bauds, 7E1
        uart_config.baud_rate = 9600;
        mode = MODE_STANDARD;
        GROUP_SEPARATOR = 0x09;
        break;
    default:
        break;
    }
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE, 0, 0, NULL, 0); // set UART1 buffer size
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, UART_PIN_NO_CHANGE, UARTRX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

/**
 * @brief Read the data from the UART and store it in the buffer
 *
 */
void Linky::read()
{
    uart_flush(UART_NUM_1);                                               // clear the UART buffer
    uint32_t timeout = (xTaskGetTickCount() * portTICK_PERIOD_MS) + 5000; // 5 seconds timeout
    memset(buffer, 0, sizeof buffer);                                     // clear the buffer
    rxBytes = 0;

    uint32_t startOfFrame = UINT_MAX; // store the index of the start frame
    uint32_t endOfFrame = UINT_MAX;   // store the index of the end frame

    // debugFrame();
    do
    {
        rxBytes += uart_read_bytes(UART_NUM_1, buffer + rxBytes, (RX_BUF_SIZE - 1) - rxBytes, 500 / portTICK_PERIOD_MS);
        ESP_LOGI(LINKY_TAG, "Read %lu bytes, remaning:%ld", rxBytes, timeout - xTaskGetTickCount() * portTICK_PERIOD_MS);
        //----------------------------------------------------------
        // Firt step: find the start and end of the frame
        //----------------------------------------------------------

        for (int i = 0; i < BUFFER_SIZE; i++) // for each character in the buffer
        {
            if (buffer[i] == START_OF_FRAME) // if the character is a start of frame
            {
                startOfFrame = i; // store the index
            }
            else if (buffer[i] == END_OF_FRAME && i > startOfFrame) // if the character is an end of frame and an start of frame has been found
            {
                endOfFrame = i; // store the index
                break;          // stop the loop
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    } while ((endOfFrame == UINT_MAX || startOfFrame == UINT_MAX || (startOfFrame > endOfFrame)) && ((xTaskGetTickCount() * portTICK_PERIOD_MS) < timeout));

    if (endOfFrame == UINT_MAX || startOfFrame == UINT_MAX || (startOfFrame > endOfFrame)) // if a start of frame and an end of frame has been found
    {
        ESP_LOGE(LINKY_TAG, "Error: Frame not found");
        frameSize = 0;
        frame = NULL;
        return;
    }
    else
    {
        ESP_LOGI(LINKY_TAG, "Start of frame: %lu", startOfFrame);
        ESP_LOGI(LINKY_TAG, "End of frame: %lu", endOfFrame);
        frameSize = endOfFrame - startOfFrame;
        frame = buffer + startOfFrame;
    }
    // ESP_LOG_BUFFER_HEXDUMP(LINKY_TAG, buffer, rxBytes, ESP_LOG_INFO);
    // ESP_LOGI(LINKY_TAG, "-------------------");
    ESP_LOGD(LINKY_TAG, "Buffer: %s", buffer);
}

/**
 * @brief Decode the data from the buffer and store it in variables
 *
 * @return 0 if an error occured, 1 if the data is valid
 */
char Linky::decode()
{
    //----------------------------------------------------------
    // Clear the previous data
    //----------------------------------------------------------
    switch (mode)
    {
    case MODE_HISTORIQUE:
        data.hist = {0};
        break;
    case MODE_STANDARD:
        data.std = {0};
        break;
    default:
        break;
    }
    if (!frame)
    {
        if (config.values.linkyMode == AUTO)
        {
            switch (this->mode)
            {
            case MODE_HISTORIQUE:
                if (strlen(data.hist.ADCO) > 0)
                {
                    ESP_LOGI(LINKY_TAG, "Auto mode: Mode Historique Found!");
                    config.values.linkyMode = MODE_HISTORIQUE;
                    config.write();
                }
                else
                {
                    ESP_LOGI(LINKY_TAG, "Auto mode: Mode Historique Not Found! Try Mode Standard");
                    setMode(MODE_STANDARD);
                }
                break;
            case MODE_STANDARD:
                if (strlen(data.std.ADSC) > 0)
                {
                    ESP_LOGI(LINKY_TAG, "Auto mode: Mode Standard Found!");
                    config.values.linkyMode = MODE_STANDARD;
                    config.write();
                }
                else
                {
                    ESP_LOGI(LINKY_TAG, "Auto mode: Mode Standard Not Found! Try Mode Historique");
                    setMode(MODE_HISTORIQUE);
                }
                break;
            default:
                break;
            }
        }
        return 0;
    }
    // ESP_LOG_BUFFER_HEXDUMP(LINKY_TAG, frame, endOfFrame - startOfFrame, ESP_LOG_INFO);
    //-------------------------------------
    // Second step: Find goups of data in the frame
    //-------------------------------------
    unsigned int startOfGroup[GROUP_COUNT] = {UINT_MAX}; // store starts index of each group
    unsigned int endOfGroup[GROUP_COUNT] = {UINT_MAX};   // store ends index of each group
    unsigned int startOfGroupIndex = 0;                  // store the current index of starts of group array
    unsigned int endOfGroupIndex = 0;                    // store the current index of ends of group array
    for (unsigned int i = 0; i < frameSize; i++)         // for each character in the frame
    {
        switch (frame[i])
        {
        case START_OF_GROUP: // if the character is a start of group
            // ESP_LOGI(LINKY_TAG, "START OF GROUP: %u (%x) --> startOfGroupIndex: %u", i, frame[i], startOfGroupIndex);
            startOfGroup[startOfGroupIndex++] = i; // store the index and increment it
            break;                                 //
        case END_OF_GROUP:                         // if the character is a end of group
            // ESP_LOGI(LINKY_TAG, "END OF GROUP: %u (%x) --> endOfGroupIndex: %u", i, frame[i], endOfGroupIndex);
            endOfGroup[endOfGroupIndex++] = i; // store the index and increment it
            break;
        default:
            break;
        }
    }

    if (startOfGroup[0] == UINT_MAX || endOfGroup[0] == UINT_MAX) // if not group found (keep the UINT_MAX value)
    {
        ESP_LOGI(LINKY_TAG, "No group found");
        return 0; // exit the function (no group found)
    }

    if (startOfGroupIndex != endOfGroupIndex) // if the number of starts is not equal to the number of ends: Error
    {
        // error: number of start and end frames are not equal
        ESP_LOGI(LINKY_TAG, "error: number of start and end group are not equal: %d %d", startOfGroupIndex, endOfGroupIndex);
        return 0;
    }

    // for (int i = 0; i < startOfGroupIndex; i++) // for each group
    // {
    //     ESP_LOGI(LINKY_TAG, "Group %d: %d - %d", i, startOfGroup[i], endOfGroup[i]);
    // }

    //------------------------------------------
    // Third step: Find fields in each group
    //------------------------------------------
    for (int i = 0; i < startOfGroupIndex; i++) // for each group
    {
        unsigned int separators[startOfGroupIndex + 1] = {0}; // store index of separators
        uint8_t separatorIndex = 0;                           // store the current index of separators array
        for (int j = startOfGroup[i]; j < endOfGroup[i]; j++) // for each character in group
        {
            if (frame[j] == GROUP_SEPARATOR) // if the character is a separator
            {
                separators[separatorIndex++] = j; // store the index of the separator
            }
        }

        char label[10] = {0};   // store the label as a string
        char value[100] = {0};  // store the data as a string
        char time[15] = {0};    // store the time as a string (H081225223518)
        char checksum[5] = {0}; // store the checksum as a string

        //-----------------------------------------------------------------------------------------------------------------replace to MEMCOPY
        memcpy(label, frame + startOfGroup[i] + 1, separators[0] - startOfGroup[i] - 1); // copy the label from the group
        memcpy(value, frame + separators[0] + 1, separators[1] - separators[0] - 1);     // copy the data from the group
        if (linky.mode == MODE_STANDARD && separatorIndex == 3)                          // if the mode is standard and the number of separators is 3
        {
            memcpy(time, frame + separators[1] + 1, separators[2] - separators[1] - 1);     // copy the time from the group
            memcpy(checksum, frame + separators[2] + 1, endOfGroup[i] - separators[2] - 1); // copy the checksum from the group
        }
        else
        {
            memcpy(checksum, frame + separators[1] + 1, endOfGroup[i] - separators[1] - 1); // copy the checksum from the group
        }
        // ESP_LOGI(LINKY_TAG, "label: %s value: %s checksum: %s", label, value, checksum);

        if (this->checksum(label, value, time) != checksum[0]) // check the checksum with the label, data and time
        {
            // error: checksum is not correct, skip the field
            ESP_LOGI(LINKY_TAG, "ERROR: %s checksum is not correct (%c != %c)", label, this->checksum(label, value, time), checksum[0]);
            continue;
        }
        else
        {
            //------------------------------------------------------------
            // Fourth step: Copy values from each field to the variables
            //------------------------------------------------------------
            for (uint32_t j = 0; j < LinkyLabelListSize; j++)
            {
                if (mode != LinkyLabelList[j].mode)
                    continue;
                if (strcmp(LinkyLabelList[j].label, label) == 0)
                {
                    switch (LinkyLabelList[j].type)
                    {
                    case STRING:
                        strcpy((char *)LinkyLabelList[j].data, value);
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
                        timeLabel.timestamp = decodeTime(time);
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
char Linky::update()
{
    read();       // read the UART
    if (decode()) // decode the frame
    {
        return 1;
    }
    return 0;
}

/**
 * @brief Print the data
 *
 */
void Linky::print()
{
    for (uint32_t i = 0; i < LinkyLabelListSize; i++)
    {
        if (mode != LinkyLabelList[i].mode)
            continue;
        switch (LinkyLabelList[i].type)
        {
        case STRING:
            if (strlen((char *)LinkyLabelList[i].data) > 0) // print only if we have a value
                ESP_LOGI(LINKY_TAG, "%s: %s", LinkyLabelList[i].label, (char *)LinkyLabelList[i].data);
            break;
        case UINT8:
            if (*(uint8_t *)LinkyLabelList[i].data != UINT8_MAX) // print only if we have a value
                ESP_LOGI(LINKY_TAG, "%s: %u", LinkyLabelList[i].label, *(uint8_t *)LinkyLabelList[i].data);
            break;
        case UINT16:
            if (*(uint16_t *)LinkyLabelList[i].data != UINT16_MAX) // print only if we have a value
                ESP_LOGI(LINKY_TAG, "%s: %u", LinkyLabelList[i].label, *(uint16_t *)LinkyLabelList[i].data);
            break;
        case UINT32:
            if (*(uint32_t *)LinkyLabelList[i].data != UINT32_MAX) // print only if we have a value
                ESP_LOGI(LINKY_TAG, "%s: %lu", LinkyLabelList[i].label, *(uint32_t *)LinkyLabelList[i].data);
            break;
        case UINT64:
            if (*(uint64_t *)LinkyLabelList[i].data != UINT64_MAX) // print only if we have a value
                ESP_LOGI(LINKY_TAG, "%s: %llu", LinkyLabelList[i].label, *(uint64_t *)LinkyLabelList[i].data);
            break;
        case UINT32_TIME:
        {
            TimeLabel timeLabel = *(TimeLabel *)LinkyLabelList[i].data;
            if (timeLabel.value != 0) // print only if we have a value
            {
                struct tm *timeinfo = localtime(&timeLabel.timestamp);
                char timeString[20];
                strftime(timeString, sizeof(timeString), "%d/%m/%Y %H:%M:%S", timeinfo);
                ESP_LOGI(LINKY_TAG, "%s: %s %lu", LinkyLabelList[i].label, timeString, timeLabel.value);
            }
            break;
        }
        default:
            break;
        }
    }
}

/**
 * @brief Calculate the checksum
 *
 * @param label name of the field
 * @param data value of the field
 * @return return the character of the checksum
 */
char Linky::checksum(char *label, char *data, char *time)
{
    int S1 = 0;                                // sum of the ASCII codes of the characters in the label
    for (int i = 0; i < strlen(label); i++)    // for each character in the label
    {                                          //
        S1 += label[i];                        // add the ASCII code of the label character to the sum
    }                                          //
    S1 += GROUP_SEPARATOR;                     // add the ASCII code of the separator to the sum
    for (int i = 0; i < strlen(data); i++)     // for each character in the data
    {                                          //
        S1 += data[i];                         // add the ASCII code of the data character to the sum
    }                                          //
    if (linky.mode == MODE_STANDARD)           // if the mode is standard
    {                                          //
        S1 += GROUP_SEPARATOR;                 // add the ASCII code of the separator to the sum
        for (int i = 0; i < strlen(time); i++) // for each character in the time
        {                                      //
            S1 += time[i];                     // add the ASCII code of the time character to the sum
        }                                      //
    }                                          //
    return (S1 & 0x3F) + 0x20;                 // return the checksum
}

time_t Linky::decodeTime(char *time)
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
        ESP_LOGE(LINKY_TAG, "Error: Time format is not correct");
        return 0;
    }
    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));
    tm.tm_year = (time[1] - '0') * 10 + (time[2] - '0') + 100; // year since 1900
    tm.tm_mon = (time[3] - '0') * 10 + (time[4] - '0') - 1;    // month since January [0-11]
    tm.tm_mday = (time[5] - '0') * 10 + (time[6] - '0');       // day of the month [1-31]
    tm.tm_hour = (time[7] - '0') * 10 + (time[8] - '0');       // hours since midnight [0-23]
    tm.tm_min = (time[9] - '0') * 10 + (time[10] - '0');       // minutes after the hour [0-59]
    tm.tm_sec = (time[11] - '0') * 10 + (time[12] - '0');      // seconds after the minute [0-60]
    return mktime(&tm);
}

uint8_t Linky::presence()
{
    switch (mode)
    {
    case MODE_HISTORIQUE:
        if (strlen(data.hist.ADCO) > 0)
            return 1;
        else
            return 0;
        break;
    case MODE_STANDARD:
        if (strlen(data.std.ADSC) > 0)
            return 1;
        else
            return 0;
        break;
    default:
        break;
    }
    return 0;
}

void Linky::debugFrame()
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
    debugGroup debugGroups[16] = {
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
    sprintf(debugGroups[3].value, "%lld", getTimestamp());
    debugGroups[3].checksum = checksum(debugGroups[3].name, debugGroups[3].value, "");
    const uint16_t debugGroupCount = sizeof(debugGroups) / sizeof(debugGroups[0]);
    rxBytes = 0;
    buffer[rxBytes++] = START_OF_FRAME;
    for (uint16_t i = 0; i < debugGroupCount; i++)
    {
        buffer[rxBytes++] = START_OF_GROUP;
        for (uint16_t j = 0; j < strlen(debugGroups[i].name); j++)
        {
            buffer[rxBytes++] = debugGroups[i].name[j];
        }
        buffer[rxBytes++] = GROUP_SEPARATOR;
        for (uint16_t j = 0; j < strlen(debugGroups[i].value); j++)
        {
            buffer[rxBytes++] = debugGroups[i].value[j];
        }
        buffer[rxBytes++] = GROUP_SEPARATOR;
        buffer[rxBytes++] = debugGroups[i].checksum;
        buffer[rxBytes++] = END_OF_GROUP;
    }
    buffer[rxBytes++] = END_OF_FRAME;
    // ESP_LOG_BUFFER_HEXDUMP(LINKY_TAG, buffer, rxBytes + 1, ESP_LOG_INFO);
}
