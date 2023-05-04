#include "config.h"

Config::Config()
{
}

int16_t Config::calculateChecksum()
{
    int16_t checksum = 0;
    for (int i = 0; i < sizeof(config_t) - sizeof(this->values.checksum); i++)
    {
        checksum += ((uint8_t *)&this->values)[i];
    }
    return checksum;
}

int8_t Config::erase()
{
    Config blank_config;
    this->values = blank_config.values;
    return 0;
}

int8_t Config::begin()
{
    EEPROM.begin(EEPROM_SIZE); // init eeprom
    this->read();
    Serial.printf("\nConfig read checksum: %x\n", this->values.checksum);
    Serial.printf("Config calculated checksum: %x \n", this->calculateChecksum());
    if (this->values.checksum != this->calculateChecksum())
    {
        Serial.printf("Config checksum error: %x != %x\n", this->values.checksum, this->calculateChecksum());
        this->erase();
        Serial.println("Config erased");
        this->write();
        for (int i = 0; i < sizeof(config_t); i++)
        {
            Serial.printf("%x ", ((uint8_t *)&this->values)[i]);
        }
        Serial.println();
        return 1;
    }
    Serial.println("Config OK");
    return 0;
}

int8_t Config::read()
{
    EEPROM.get(0, values);
    return 0;
}

int8_t Config::write()
{
    this->values.checksum = this->calculateChecksum();
    EEPROM.put(0, this->values); // Write config to EEPROM
    EEPROM.commit();             // Commit the EEPROM
    return 0;
}