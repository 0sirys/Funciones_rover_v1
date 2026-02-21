idf_component_register(SRCS "motor.c" "mcpwm_bdc_control_example_main.c"
                       PRIV_REQUIRES "esp_driver_pcnt" "esp_timer"
                       INCLUDE_DIRS ".")
