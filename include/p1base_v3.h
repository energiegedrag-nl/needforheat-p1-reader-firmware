#ifndef _P1BASE_V3_H
#define _P1BASE_V3_H

/**
 * Support for the NeedForHeat P1-BASE v3 on an M5Stack CoreInk.
 * Hardware: https://github.com/energiegedrag-nl/needforheat-p1-base-hardware (docs/design-v3.md)
 *
 *   G13 P1 data (UART RX, not inverted)      G26 data request (high = request)
 *   G25 P1 charging on (high)                G14 charge current 120 mA (high) / 60 mA (low)
 *   G36 meter 5 V x 68/168 (ADC1_CH0)        G34 existing reader requests x 68/115 (ADC1_CH6)
 *
 * Status: written against the hardware design, NOT yet tested on hardware.
 */

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>

#ifdef NFH_BOARD_P1BASE_V3

#define P1BASE_GPIO_CHG_EN GPIO_NUM_25
#define P1BASE_GPIO_CHG_HI GPIO_NUM_14

typedef enum
{
    P1BASE_OK = 0,            // meter 5 V present and telegrams received
    P1BASE_NO_DATA,           // meter 5 V present, no telegrams (request not arriving, cable in the wrong socket)
    P1BASE_NO_METER_VOLTAGE,  // no 5 V: DSMR 2/3 meter, crossed cable, wrong socket or not connected
    P1BASE_SUPPLY_SAGGING     // meter 5 V below 4.6 V: P1 supply overloaded
} p1base_status_t;

typedef enum
{
    P1BASE_CHARGE_OFF = 0,
    P1BASE_CHARGE_60MA,
    P1BASE_CHARGE_120MA
} p1base_charge_t;

/** Configure the charge-control outputs (off) and the two ADC inputs. */
void p1base_init(void);

/** Meter 5 V in volts, measured on G36. */
float p1base_meter_voltage(void);

/** True when the existing P1 reader on the lower socket requests data (G34). */
bool p1base_external_reader_requests(void);

/** Switch P1 charging of the CoreInk battery. */
void p1base_set_charging(p1base_charge_t level);

/**
 * Call after every P1 read attempt. Updates the charging level (only on DSMR >= 5 and
 * when the meter voltage stays above the thresholds) and returns a diagnosis for logging
 * and the e-ink screen.
 */
p1base_status_t p1base_after_read(bool telegram_ok, float dsmr_version);

#endif // NFH_BOARD_P1BASE_V3

#ifdef __cplusplus
}
#endif

#endif // _P1BASE_V3_H
