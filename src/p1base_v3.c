#include "p1base_v3.h"

#ifdef NFH_BOARD_P1BASE_V3

#include <driver/adc.h>
#include <driver/gpio.h>
#include <esp_adc_cal.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "P1-BASE";

#define ADC_METER ADC1_CHANNEL_0   // G36
#define ADC_EXTREQ ADC1_CHANNEL_6  // G34
#define METER_DIVIDER (168.0f / 68.0f)
#define EXTREQ_THRESHOLD_MV 1500

#define V_PRESENT 4.5f   // below this the meter is not supplying (DSMR 2/3 or not connected)
#define V_SAGGING 4.6f   // P1 supply overloaded: back off
#define V_STEP_UP 4.75f  // enough headroom to go to the next charge level

static esp_adc_cal_characteristics_t adc_chars;
static p1base_charge_t charge_level = P1BASE_CHARGE_OFF;

void p1base_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << P1BASE_GPIO_CHG_EN) | (1ULL << P1BASE_GPIO_CHG_HI),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    gpio_set_level(P1BASE_GPIO_CHG_EN, 0);
    gpio_set_level(P1BASE_GPIO_CHG_HI, 0);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_METER, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC_EXTREQ, ADC_ATTEN_DB_11);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
}

static uint32_t read_mv(adc1_channel_t ch)
{
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

bool p1base_external_reader_requests(void)
{
    return read_mv(ADC_EXTREQ) > EXTREQ_THRESHOLD_MV;
}

void p1base_set_charging(p1base_charge_t level)
{
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

p1base_status_t p1base_after_read(bool telegram_ok, float dsmr_version)
{
    static bool initialised = false;
    if (!initialised)
    {
        p1base_init();
        initialised = true;
    }
    float v = p1base_meter_voltage();
    bool ext = p1base_external_reader_requests();
    p1base_status_t status;

    if (v < V_PRESENT)
    {
        status = P1BASE_NO_METER_VOLTAGE;
        p1base_set_charging(P1BASE_CHARGE_OFF);
    }
    else if (v < V_SAGGING)
    {
        status = P1BASE_SUPPLY_SAGGING;
        p1base_set_charging(charge_level == P1BASE_CHARGE_120MA ? P1BASE_CHARGE_60MA : P1BASE_CHARGE_OFF);
    }
    else
    {
        status = telegram_ok ? P1BASE_OK : P1BASE_NO_DATA;
        if (telegram_ok && dsmr_version >= 5.0f && v >= V_STEP_UP)
        {
            // step up one level per read, so a sagging supply is noticed before the next step
            p1base_set_charging(charge_level == P1BASE_CHARGE_OFF ? P1BASE_CHARGE_60MA : P1BASE_CHARGE_120MA);
            vTaskDelay(pdMS_TO_TICKS(50));
            if (p1base_meter_voltage() < V_SAGGING)
            {
                p1base_set_charging(charge_level == P1BASE_CHARGE_120MA ? P1BASE_CHARGE_60MA : P1BASE_CHARGE_OFF);
            }
        }
        else if (!telegram_ok || dsmr_version < 5.0f)
        {
            p1base_set_charging(P1BASE_CHARGE_OFF);
        }
    }
    ESP_LOGI(TAG, "meter 5 V: %.2f V; existing reader requests: %s; status: %d", v, ext ? "yes" : "no", status);
    // TODO: show status, meter voltage and charging on the e-ink screen once the e-ink GUI
    // (needforheat-generic-firmware, DisplayStatus) is available.
    return status;
}

#endif // NFH_BOARD_P1BASE_V3
