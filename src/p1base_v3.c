#include "p1base_v3.h"

#ifdef NFH_BOARD_P1BASE_V3

#include <driver/adc.h>
#include <driver/gpio.h>
#include <esp_adc_cal.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <i2c_hal.h>

static const char *TAG = "P1-BASE";

#define ADC_METER ADC1_CHANNEL_0  // G36
#define ADC_EXTREQ ADC1_CHANNEL_6 // G34
#define ADC_CELL ADC1_CHANNEL_7   // G35
#define METER_DIVIDER (168.0f / 68.0f)
#define CELL_DIVIDER (25.1f / 5.1f)
#define EXTREQ_THRESHOLD_MV 1500

#define V_ABSENT 0.5f   // below this the meter does not supply at all (DSMR 2/3, not connected)
#define V_PRESENT 4.5f  // below this (and above V_ABSENT) the meter supply is unusable
#define V_SAGGING 4.6f  // P1 supply overloaded: back off
#define V_STEP_UP 4.75f // enough headroom to go to the next charge level

#define CELL_FULL 4.10f   // stop charging: with the CoreInk load the TP4056 may never terminate
#define CELL_RESUME 4.00f // charge again (and open a charge window) below this

#define CHARGE_WINDOW_STEP_MS 10000
#define USB_GAP_S 60 // a gap this long between reads means the scheduler tried to power off

// I2C addresses on the CoreInk bus (G21/G22). Never write to 0x03 (PCA9633 software reset) or 0x70 (all-call).
#define I2C_ADDR_PCA9633 0x62
#define I2C_ADDR_BATTERYBASE_ADS1115 0x48

// PCA9633 registers and the recipe of design-v3 § 4.7
#define PCA9633_MODE1 0x00
#define PCA9633_MODE2 0x01
#define PCA9633_PWM0 0x02
#define PCA9633_GRPPWM 0x06
#define PCA9633_LEDOUT 0x08
#define PCA9633_MODE1_NORMAL 0x00   // awake, no auto-increment, no sub-addresses, all-call off
#define PCA9633_MODE2_RECIPE 0x14   // INVRT = 1, OUTDRV = 1 (totem pole), DMBLNK = 0 (group dimming)
#define PCA9633_LEDOUT_DIMMED 0x03  // LDR0 = 11: LED0 dimmed by GRPPWM
#define LED_BRIGHTNESS 32           // GRPPWM duty cycle: about 12 %

static esp_adc_cal_characteristics_t adc_chars;
static bool adc_ready = false;
static p1base_charge_t charge_level = P1BASE_CHARGE_OFF;
static bool cell_full = false;
static bool other_charger = false; // a BatteryBASE is fitted: at most 60 mA from P1
static bool usb_power = false;     // USB detected: at most 60 mA from P1
static time_t last_read_done = 0;  // end of the previous P1 task in this boot; 0: none yet
static bool has_dimmer = false;

void p1base_init(void)
{
    // Levels first, then the outputs, so neither pin glitches high. G14 before G25: G14 has an internal pull-up
    // at reset, harmless only while G25 is low.
    gpio_set_level(P1BASE_GPIO_CHG_HI, 0);
    gpio_set_level(P1BASE_GPIO_CHG_EN, 0);
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << P1BASE_GPIO_CHG_HI) | (1ULL << P1BASE_GPIO_CHG_EN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    gpio_set_level(P1BASE_GPIO_CHG_HI, 0);
    gpio_set_level(P1BASE_GPIO_CHG_EN, 0);
    charge_level = P1BASE_CHARGE_OFF;

    if (!adc_ready)
    {
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(ADC_METER, ADC_ATTEN_DB_11);
        adc1_config_channel_atten(ADC_EXTREQ, ADC_ATTEN_DB_11);
        adc1_config_channel_atten(ADC_CELL, ADC_ATTEN_DB_11);
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
        adc_ready = true;
    }
}

static bool i2c_present(uint8_t address)
{
    return twomes_i2c_write(address, NULL, 0, I2C_SEND_STOP) == ESP_OK;
}

static esp_err_t i2c_write_reg(uint8_t address, uint8_t reg, uint8_t value)
{
    uint8_t buffer[2] = {reg, value};
    return twomes_i2c_write(address, buffer, sizeof(buffer), I2C_SEND_STOP);
}

/**
 * Set up the LED dimmer. The PCA9633 is powered from the meter side (or from the CoreInk on DSMR 2/3), so it may
 * have restarted at full brightness since the last call: write the whole recipe every time.
 */
static void setup_leds(void)
{
    if (!has_dimmer)
    {
        return;
    }
    esp_err_t err = i2c_write_reg(I2C_ADDR_PCA9633, PCA9633_MODE1, PCA9633_MODE1_NORMAL);
    vTaskDelay(pdMS_TO_TICKS(1)); // oscillator start-up: 500 µs
    if (err == ESP_OK)
        err = i2c_write_reg(I2C_ADDR_PCA9633, PCA9633_MODE2, PCA9633_MODE2_RECIPE);
    if (err == ESP_OK)
        err = i2c_write_reg(I2C_ADDR_PCA9633, PCA9633_PWM0, 0xFF);
    if (err == ESP_OK)
        err = i2c_write_reg(I2C_ADDR_PCA9633, PCA9633_GRPPWM, LED_BRIGHTNESS);
    if (err == ESP_OK)
        err = i2c_write_reg(I2C_ADDR_PCA9633, PCA9633_LEDOUT, PCA9633_LEDOUT_DIMMED);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "LED dimmer did not respond: %s", esp_err_to_name(err));
    }
}

void p1base_init_i2c(void)
{
    // A P1-BASE without U5 (variant without dimming) does not acknowledge 0x62: the LEDs then stay at full brightness.
    has_dimmer = i2c_present(I2C_ADDR_PCA9633);
    other_charger = i2c_present(I2C_ADDR_BATTERYBASE_ADS1115);
    ESP_LOGI(TAG, "LED dimmer: %s; BatteryBASE: %s", has_dimmer ? "yes" : "no", other_charger ? "yes (P1 charging at most 60 mA)" : "no");
    setup_leds();
}

static uint32_t read_mv(adc1_channel_t ch)
{
    if (!adc_ready)
    {
        p1base_init();
    }
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++)
    {
        sum += adc1_get_raw(ch);
    }
    return esp_adc_cal_raw_to_voltage(sum / 16, &adc_chars);
}

float p1base_meter_voltage(void)
{
    return read_mv(ADC_METER) / 1000.0f * METER_DIVIDER;
}

float p1base_cell_voltage(void)
{
    return read_mv(ADC_CELL) / 1000.0f * CELL_DIVIDER;
}

bool p1base_external_reader_requests(void)
{
    return read_mv(ADC_EXTREQ) > EXTREQ_THRESHOLD_MV;
}

void p1base_set_charging(p1base_charge_t level)
{
    if (level == P1BASE_CHARGE_120MA && (other_charger || usb_power))
    {
        level = P1BASE_CHARGE_60MA;
    }
    // change the current first, then switch: never pass through a higher level than intended
    if (level == P1BASE_CHARGE_OFF)
    {
        gpio_set_level(P1BASE_GPIO_CHG_EN, 0);
        gpio_set_level(P1BASE_GPIO_CHG_HI, 0);
    }
    else
    {
        gpio_set_level(P1BASE_GPIO_CHG_HI, level == P1BASE_CHARGE_120MA);
        gpio_set_level(P1BASE_GPIO_CHG_EN, 1);
    }
    if (level != charge_level)
    {
        ESP_LOGI(TAG, "P1 charging: %s", level == P1BASE_CHARGE_OFF ? "off" : level == P1BASE_CHARGE_60MA ? "60 mA" : "120 mA");
    }
    charge_level = level;
}

p1base_charge_t p1base_charging(void)
{
    return charge_level;
}

static p1base_charge_t step_down(p1base_charge_t level)
{
    return level == P1BASE_CHARGE_120MA ? P1BASE_CHARGE_60MA : P1BASE_CHARGE_OFF;
}

/** Updates the cell-full flag with hysteresis; returns true when charging is allowed by the cell voltage. */
static bool cell_allows_charging(float cell)
{
    if (cell >= CELL_FULL)
    {
        cell_full = true;
    }
    else if (cell < CELL_RESUME)
    {
        cell_full = false;
    }
    return !cell_full;
}

/** One step up (60 → 120 mA at most), then back down at once when the meter voltage sags. */
static void try_step_up(void)
{
    if (charge_level == P1BASE_CHARGE_120MA || (charge_level == P1BASE_CHARGE_60MA && (other_charger || usb_power)))
    {
        return;
    }
    p1base_set_charging(charge_level == P1BASE_CHARGE_OFF ? P1BASE_CHARGE_60MA : P1BASE_CHARGE_120MA);
    vTaskDelay(pdMS_TO_TICKS(50));
    if (p1base_meter_voltage() < V_SAGGING)
    {
        p1base_set_charging(step_down(charge_level));
    }
}

/**
 * The CoreInk cannot sense USB, but it cannot switch itself off on USB either: the generic scheduler then waits
 * instead. So when this boot already finished a read and at least USB_GAP_S passed since (enough for the scheduler
 * to try to power off), the CoreInk stayed on: USB. Unplugging USB lets the next power-off succeed, and the
 * reboot clears the flag. A long upload after the read can make this a false positive; the effect is only
 * 60 mA instead of 120 mA and "USB" on the screen until the next power-off.
 */
static void detect_usb(void)
{
    if (!usb_power && last_read_done != 0 && time(NULL) - last_read_done >= USB_GAP_S)
    {
        usb_power = true;
        ESP_LOGI(TAG, "the CoreInk stayed on between reads: USB power, P1 charging at most 60 mA");
    }
}

void p1base_read_done(void)
{
    last_read_done = time(NULL);
}

p1base_status_t p1base_after_read(bool telegram_ok, float dsmr_version)
{
    detect_usb();
    setup_leds();

    float v = p1base_meter_voltage();
    float cell = p1base_cell_voltage();
    bool ext = p1base_external_reader_requests();
    bool cell_ok = cell_allows_charging(cell);
    p1base_status_t status;

    if (v < V_ABSENT)
    {
        status = telegram_ok ? P1BASE_DSMR23 : P1BASE_NO_METER;
        p1base_set_charging(P1BASE_CHARGE_OFF);
    }
    else if (v < V_PRESENT)
    {
        status = P1BASE_METER_VOLTAGE_LOW;
        p1base_set_charging(P1BASE_CHARGE_OFF);
    }
    else if (v < V_SAGGING)
    {
        status = P1BASE_SUPPLY_SAGGING;
        p1base_set_charging(step_down(charge_level));
    }
    else
    {
        status = telegram_ok ? P1BASE_OK : P1BASE_NO_DATA;
        if (!telegram_ok || dsmr_version < 5.0f || !cell_ok)
        {
            p1base_set_charging(P1BASE_CHARGE_OFF);
        }
        else if (v >= V_STEP_UP)
        {
            // step up one level per read, so a sagging supply is noticed before the next step
            try_step_up();
        }
    }
    ESP_LOGI(TAG, "meter 5 V: %.2f V; cell: %.2f V; existing reader requests: %s; status: %s; supply: %s",
             v, cell, ext ? "yes" : "no", p1base_status_text(status), p1base_supply_text());
    return status;
}

void p1base_charge_window(time_t deadline)
{
    if (charge_level == P1BASE_CHARGE_OFF || p1base_cell_voltage() >= CELL_RESUME)
    {
        return;
    }
    ESP_LOGI(TAG, "charge window: staying on for up to %ld s", (long)(deadline - time(NULL)));
    while (time(NULL) + CHARGE_WINDOW_STEP_MS / 1000 < deadline)
    {
        vTaskDelay(pdMS_TO_TICKS(CHARGE_WINDOW_STEP_MS));

        float v = p1base_meter_voltage();
        if (v < V_SAGGING)
        {
            p1base_set_charging(v < V_PRESENT ? P1BASE_CHARGE_OFF : step_down(charge_level));
        }
        else if (!cell_allows_charging(p1base_cell_voltage()))
        {
            p1base_set_charging(P1BASE_CHARGE_OFF);
        }
        else if (v >= V_STEP_UP)
        {
            try_step_up();
        }
        if (charge_level == P1BASE_CHARGE_OFF)
        {
            break;
        }
    }
    ESP_LOGI(TAG, "charge window ends: cell %.2f V, meter 5 V %.2f V", p1base_cell_voltage(), p1base_meter_voltage());
}

const char *p1base_status_text(p1base_status_t status)
{
    // Short Dutch texts that fit a 200 px wide e-ink line in a 14 px bold font.
    switch (status)
    {
    case P1BASE_OK:
        return "ok";
    case P1BASE_NO_DATA:
        return "geen data";
    case P1BASE_DSMR23:
        return "DSMR 2/3: USB nodig";
    case P1BASE_NO_METER:
        return "geen meter";
    case P1BASE_METER_VOLTAGE_LOW:
        return "meter 5 V te laag";
    case P1BASE_SUPPLY_SAGGING:
        return "P1 overbelast";
    }
    return "?";
}

const char *p1base_supply_text(void)
{
    if (usb_power)
        return "USB";
    return charge_level == P1BASE_CHARGE_OFF ? "accu" : "P1";
}

#endif // NFH_BOARD_P1BASE_V3
