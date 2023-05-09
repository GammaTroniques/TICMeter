#ifndef HTTP_H
#define HTTP_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h> //Requires by memset
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include <esp_http_server.h>
#include "esp_spiffs.h"
#include "esp_log.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#include "esp_err.h"
#include "esp_log.h"

#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"

void initi_web_page_buffer(void);
httpd_handle_t setup_server(void);
/* Scratch buffer size */
#define SCRATCH_BUFSIZE 8192
/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

struct file_server_data
{
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];

    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};
#endif // HTTP_H