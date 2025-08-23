uint16_t get_last_location_time(void) {
    // Retourne le temps écoulé en minutes
    return (uint16_t)((system_time - last_update) / 60);
}

uint8_t altitude_to_code(double altitude) {
    // Conversion altitude -> code 10 bits
    // ... implémentation spécifique ...
}