set(srcs "main.c" "udp_client.c" "bluetooth.c")

idf_component_register(SRCS "${srcs}" INCLUDE_DIRS ".")
