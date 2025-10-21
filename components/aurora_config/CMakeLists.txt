# Component CMakeLists.txt
# Questo componente contiene le configurazioni personalizzate per Aurora-Go ESP32

idf_component_register(SRCS "aurora_config.c"
                       INCLUDE_DIRS "include")
