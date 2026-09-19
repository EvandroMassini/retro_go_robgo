idf_component_register(SRC_DIRS src
                                src/comdrivers
                                src/devdrivers
                                src/dispdrivers
                                src/emudevs
                                src/network
                       INCLUDE_DIRS src
                       PRIV_INCLUDE_DIRS
                                src/fonts
                                src/images
                                src/comdrivers
                                src/devdrivers
                                src/dispdrivers
                                src/emudevs
                                src/network
                       REQUIRES fatfs
                                spiffs
                                esp_adc_cal
                                ulp
                                nvs_flash
                                soc)
