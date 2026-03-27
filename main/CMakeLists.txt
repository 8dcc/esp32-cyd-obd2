idf_component_register(
  SRCS "main.c" "render.c" "framebuffer.c" "chart.c"
       "serial_uart.c" "serial_bluetooth.c"
       "elm327.c" "obd2.c"
  INCLUDE_DIRS "."
  REQUIRES driver esp_lcd esp_lcd_ili9341 bt
)
