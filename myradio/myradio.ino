#include <Arduino.h>

// Forward decl (vagy egy app_impl.h headerből)
void app_setup();
void app_loop();

void setup() {
  app_setup();
}

void loop() {
  app_loop();
}
