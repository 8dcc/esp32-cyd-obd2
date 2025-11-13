idf_component_register(
  SRCS "main.c" "render.c" "chart.c" "serial_uart.c"
  INCLUDE_DIRS "."
  REQUIRES driver esp_lcd esp_lcd_ili9341
)
