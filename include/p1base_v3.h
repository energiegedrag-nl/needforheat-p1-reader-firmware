#ifndef _P1BASE_V3_H
#define _P1BASE_V3_H

/**
 * Support for the NeedForHeat P1-BASE v3 on an M5Stack CoreInk.
 * Hardware: https://github.com/energiegedrag-nl/needforheat-p1-base-hardware (docs/design-v3.md, § 4–5)
 *
 *   G13 P1 data (UART RX, not inverted)      G26 data request (high = request)
 *   G25 P1 charging on (high)                G14 charge current 120 mA (high) / 60 mA (low)
 *   G36 meter 5 V x 68/168 (ADC1_CH0)        G34 existing reader requests x 68/115 (ADC1_CH6)
 *   G35 CoreInk cell voltage x 5.1/25.1 (ADC1_CH7, CoreInk's own divider)
 *   G21/G22 I2C: LED dimmer PCA9633 at 0x62 (P1-BASE), BatteryBASE ADS1115 at 0x48
 *
 * Charging only happens while the CoreInk is on: between measurements it switches itself off (the RTC wakes it),
 * the ESP32 is then unpowered and the pull-down on the BASE switches G25 off. To charge anyway, the P1 task keeps
 * the CoreInk on for a charge window (p1base_charge_window()) while the cell is low and the meter allows it.
 *
 * This logic belongs in the generic power manager of needforheat-generic-firmware once that exists; it lives here
 * until then.
 *
 * Status: written against the hardware design, NOT yet tested on hardware.
 */

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <time.h>

#ifdef NFH_BOARD_P1BASE_V3

#define P1BASE_GPIO_CHG_EN GPIO_NUM_25
#define P1BASE_GPIO_CHG_HI GPIO_NUM_14

/** Diagnosis per design-v3 § 5 ("Diagnosis the firmware can show on the e-ink screen"). */
typedef enum
{
    P1BASE_OK = 0,           // meter 5 V present and telegrams received (DSMR >= 4)
    P1BASE_NO_DATA,          // meter 5 V present, no telegrams: request not reaching the meter, or wrong socket
    P1BASE_DSMR23,           // no meter 5 V, but telegrams: DSMR 2/3 meter (no P1 supply), needs mains power
    P1BASE_NO_METER,         // no meter 5 V and no telegrams: no meter, crossed cable, or wrong socket
    P1BASE_METER_VOLTAGE_LOW, // meter 5 V between 0.5 V and 4.5 V: bad contact or a very weak supply
    P1BASE_SUPPLY_SAGGING    // meter 5 V below 4.6 V: P1 supply overloaded, charging stepped down
} p1base_status_t;

typedef enum
{
    P1BASE_CHARGE_OFF = 0,
    P1BASE_CHARGE_60MA,
    P1BASE_CHARGE_120MA
} p1base_charge_t;

/**
 * Call first thing at boot: switches charging off (G14 before G25) and sets up the ADC inputs.
 * Safe to call more than once.
 */
void p1base_init(void);

/**
 * Call after the generic firmware set up I2C (GenericESP32Firmware::Initialize): detects a BatteryBASE and sets
 * up the LED dimmer. Safe to call more than once.
 */
void p1base_init_i2c(void);

/** Meter 5 V in volts, measured on G36. */
float p1base_meter_voltage(void);

/** CoreInk cell voltage in volts, measured on G35. */
float p1base_cell_voltage(void);

/** True when the existing P1 reader on the lower socket requests data (G34). */
bool p1base_external_reader_requests(void);

/** Switch P1 charging of the CoreInk battery. */
void p1base_set_charging(p1base_charge_t level);

/** Current charge level. */
p1base_charge_t p1base_charging(void);

/**
 * Call after every P1 read attempt. Updates the charging level (only on DSMR >= 5, below the cell voltage limit,
 * and while the meter voltage stays above the thresholds) and returns a diagnosis for logging and the screen.
 */
p1base_status_t p1base_after_read(bool telegram_ok, float dsmr_version);

/**
 * Keeps charging until the cell is full, the meter supply sags, or `deadline` (system time) is reached; returns
 * at once when no charge is useful. Blocks the calling task, so the scheduler keeps the CoreInk on meanwhile.
 */
void p1base_charge_window(time_t deadline);

/** Short Dutch text for the screen and log, e.g. "geen data". */
const char *p1base_status_text(p1base_status_t status);

/** Supply indicator text: "P1" while the meter charges the cell, else "accu" (USB cannot be detected yet). */
const char *p1base_supply_text(void);

#endif // NFH_BOARD_P1BASE_V3

#ifdef __cplusplus
}
#endif

#endif // _P1BASE_V3_H
