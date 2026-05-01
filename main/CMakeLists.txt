idf_component_register(
  SRCS "main.c" "util.c"
       "render.c" "framebuffer.c" "chart.c" "draw_text.c"
       "elm327.c" "obd2.c"
       "serial_uart.c" "serial_bluetooth.c"
  INCLUDE_DIRS "."
  REQUIRES driver esp_lcd esp_lcd_ili9341 bt
)
