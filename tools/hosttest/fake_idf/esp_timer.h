// Finto esp_timer.h per le prove sul computer: il tempo lo comanda la prova (g_fake_us)
#pragma once
extern long long g_fake_us;
inline long long esp_timer_get_time() { return g_fake_us; }
