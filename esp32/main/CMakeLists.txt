idf_component_register(SRCS
                       SRC_DIRS "."
                       INCLUDE_DIRS "include"
                       INCLUDE_DIRS "."
                        )
 
                    
spiffs_create_partition_image(storage ../data FLASH_IN_PROJECT)