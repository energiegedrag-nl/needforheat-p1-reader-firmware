// To create the JSON and read the P1 port
#include <string.h>
#include <sstream>
#include <utility>
#include <vector>
#include <scheduler.hpp>
#include <p1.hpp>
#include <P1Config.h>
#include <p1base_v3.h>
#include <dsmr_timestamp.hpp>
#include <dsmr_timestampTest.hpp>

// ESP-IDF drivers:
#include <generic_esp_32.hpp>
#include <scheduler.hpp>
#include <secure_upload.hpp>
#include <measurements.hpp>
#include <util/error.hpp>

auto secureUploadQueue = SecureUpload::Queue::GetInstance();

std::string hexToNormalString(const char *hexString)
{
	std::string normalString;

	size_t len = strlen(hexString);
	ESP_LOGD("P1", "hex-encoded string length: %d", len);
	if (len % 2 != 0)
	{
		ESP_LOGE("P1", "Invalid hex-encoded string length");
		return "";
	}

	for (size_t i = 0; i < len / 2; ++i)
	{
		int byte;
		if (std::istringstream(std::string(hexString + 2 * i, 2)) >> std::hex >> byte)
		{
			normalString += static_cast<char>(byte);
		}
		else
		{
			// Handle conversion error
			ESP_LOGE("P1", "Error converting hex to byte");
			return "";
		}
	}

	return normalString;
}

#ifdef NFH_BOARD_P1BASE_V3
namespace
{
	/** Decimal comma, as on the other NeedForHeat screens. */
	std::string Decimal(double value, int decimals)
	{
		char text[16];
		snprintf(text, sizeof(text), "%.*f", decimals, value);
		for (char *c = text; *c; c++)
			if (*c == '.')
				*c = ',';
		return text;
	}

	/** DSMR version as it is written in the standard: with a dot, e.g. "5.0". */
	std::string DsmrVersion(float version)
	{
		char text[8];
		snprintf(text, sizeof(text), "%.1f", version);
		return text;
	}

	/**
	 * Screen 8 (P1 overview) when telegrams arrive, screen 9 (P1 cable diagnosis) otherwise. Sizes follow the
	 * readable 1-bit layout of the generic e-ink GUI: a main value with its unit, at most three short tiles, and the
	 * supply indicator (P1 / accu) on the right of the footer. Shown on the e-ink screen only with a generic library
	 * that has GenericESP32Firmware::DisplayStatus (build flag NFH_EINK_STATUS); logged in any case.
	 */
	void ShowP1Status(p1base_status_t status, const P1Data *data)
	{
		const char *reader = p1base_external_reader_requests() ? "aan" : "uit";
		const char *charging = p1base_charging() == P1BASE_CHARGE_OFF ? "uit" : p1base_charging() == P1BASE_CHARGE_60MA ? "60mA" : "120mA";
		std::string bigLabel, bigValue, bigUnit;
		std::vector<std::pair<std::string, std::string>> tiles;

		if (data != nullptr && (status == P1BASE_OK || status == P1BASE_DSMR23 || status == P1BASE_SUPPLY_SAGGING))
		{
			// screen 8: P1 overview
			bigLabel = "stroom";
			if (data->e_use__kW >= 0 && data->e_ret__kW >= 0)
			{
				bigValue = Decimal(data->e_use__kW - data->e_ret__kW, 2);
				bigUnit = "kW";
			}
			else
			{
				bigValue = "-";
			}
			// DSMR 2/3 shows in the DSMR tile, a sagging supply in the charging tile. A DSMR 2/3 meter cannot charge
			// the cell: on battery, ask for USB there.
			if (status == P1BASE_DSMR23 && strcmp(p1base_supply_text(), "USB") != 0)
				charging = "USB!";
			tiles = {{"DSMR", DsmrVersion(data->dsmrVersion)}, {"lezer", reader}, {"laden", charging}};
		}
		else
		{
			// screen 9: P1 cable diagnosis; the main value is the meter's 5 V, the label says what it means
			bigLabel = p1base_status_text(status);
			bigValue = Decimal(p1base_meter_voltage(), 1);
			bigUnit = "V";
			tiles = {{"lezer", reader}, {"laden", charging}};
		}

		ESP_LOGI("P1", "screen: %s %s %s; tiles: %s=%s %s=%s; supply: %s", bigLabel.c_str(), bigValue.c_str(), bigUnit.c_str(),
				 tiles[0].first.c_str(), tiles[0].second.c_str(), tiles[1].first.c_str(), tiles[1].second.c_str(), p1base_supply_text());
#ifdef NFH_EINK_STATUS
		Screen::Status screen;
		screen.device = "P1";
		screen.bigLabel = bigLabel;
		screen.bigValue = bigValue;
		screen.bigUnit = bigUnit;
		screen.tiles = tiles;
		screen.footerRight = p1base_supply_text();
		GenericESP32Firmware::DisplayStatus(screen);
#endif
	}

	/**
	 * Keep the CoreInk on to charge from the P1 port until 70 s before the next read; the scheduler only switches
	 * the CoreInk off after all tasks have finished. The 70 s make sure the scheduler still tries to power off: if
	 * that fails, the CoreInk is on USB (see p1base_read_done()). The extra boot costs about 0.1 mAh, a charge window
	 * gains about 10 mAh.
	 */
	void P1ChargeWindow(uint16_t interval__s)
	{
		time_t now = time(nullptr);
		time_t nextRead = (now / interval__s + 1) * interval__s;
		p1base_charge_window(nextRead - 70);
		p1base_read_done();
	}
} // namespace
#endif // NFH_BOARD_P1BASE_V3

// function to read P1 port and store message in a buffer
void readP1Task(void *taskInfo)
{
	// Add formatters for all the measurements.
	Measurements::Measurement::AddFormatter("e_use_lo_cum__kWh", "%.3f");
	Measurements::Measurement::AddFormatter("e_use_hi_cum__kWh", "%.3f");
	Measurements::Measurement::AddFormatter("e_ret_lo_cum__kWh", "%.3f");
	Measurements::Measurement::AddFormatter("e_ret_hi_cum__kWh", "%.3f");
	Measurements::Measurement::AddFormatter("g_use_cum__m3", "%.3f");
	Measurements::Measurement::AddFormatter("dsmr_version__0", "%.1f");
	Measurements::Measurement::AddFormatter("meter_code__str", "%s");

	uint16_t e_meter_interval__s = 10;			// 10 [s]
	uint16_t g_meter_interval__s = 1 * 60 * 60; // 1 [h] * 60 [min/h] * 60 [s/min]
#ifdef CONFIG_TWOMES_STRESS_TEST
	uint16_t parse_interval__s = static_cast<int>(Scheduler::Interval::MINUTES_2);
#else
	uint16_t parse_interval__s = static_cast<int>(Scheduler::Interval::MINUTES_10);
#endif

	P1Data result = p1Read();

	if (result.dsmrVersion == P1_UNKNOWN)
	{
		ESP_LOGI("P1", "incorrect p1 message");
#if P1_HAS_LEDS
		GenericESP32Firmware::BlinkLED(RED_LED_D1_ERROR, 2);
#endif
#ifdef NFH_BOARD_P1BASE_V3
		ShowP1Status(p1base_after_read(false, P1_UNKNOWN), nullptr);
		p1base_read_done();
#endif
		// Exit the function early since there is no good data
        return;
	}

	time_t e_time_t = TIME_UNKNOWN;
	if (result.dsmrVersion >= 4.0)
	{
		std::string eTimestampStr(result.dsmrTimestamp_e);
		ESP_LOGD("P1", "eTimestampStr: %s", eTimestampStr.c_str());
		if (result.dsmrVersion >= 5.0)
		{
			e_meter_interval__s = 1; // 1 [s]
		}
		e_time_t = parseDsmrTimestamp(LATEST_E_TIMESTAMP, eTimestampStr, deviceTime(), e_meter_interval__s, parse_interval__s);
	}
	std::string gTimestampStr(result.dsmrTimestamp_g);
	ESP_LOGD("P1", "gTimestampStr: %s", gTimestampStr.c_str());
	if (result.dsmrVersion >= 5.0)
	{
		g_meter_interval__s = 5 * 60; // 5 [min] * 60 [s/min]
	}
	time_t g_time_t = parseDsmrTimestamp(LATEST_G_TIMESTAMP, gTimestampStr, deviceTime(), g_meter_interval__s, parse_interval__s);

	ESP_LOGD("P1", "meter code (hex-encoded): %s", result.meter_code__hex);

	std::string meter_code = hexToNormalString(result.meter_code__hex);
	if (meter_code.empty())
	{
		ESP_LOGE("P1", "meter code (hex-decoded): is empty!");
	}
	else
	{
		ESP_LOGD("P1", "meter code (hex-decoded): %s", meter_code.c_str());
	}

	if (result.dsmrVersion < 4.0 || e_time_t == TIME_UNKNOWN)
	{
		// for smart meters with version DSMR 2 and 3, no timestamps are available for the electricity meter readings;
		// also, if for some reason, timestamp parsing for DSMR4 and up is not going ok, then upload with device timestamps (second best for accuracy)
		// so, make the Measurement use the device timetamp by NOT specifying a timestamp
		Measurements::Measurement e_use_lo_cum__kWh("e_use_lo_cum__kWh", result.e_use_lo_cum__kWh);
		Measurements::Measurement e_use_hi_cum__kWh("e_use_hi_cum__kWh", result.e_use_hi_cum__kWh);
		Measurements::Measurement e_ret_lo_cum__kWh("e_ret_lo_cum__kWh", result.e_ret_lo_cum__kWh);
		Measurements::Measurement e_ret_hi_cum__kWh("e_ret_hi_cum__kWh", result.e_ret_hi_cum__kWh);
		Measurements::Measurement dsmr_version__0("dsmr_version__0", result.dsmrVersion);

		secureUploadQueue.AddMeasurement(e_use_lo_cum__kWh);
		secureUploadQueue.AddMeasurement(e_use_hi_cum__kWh);
		secureUploadQueue.AddMeasurement(e_ret_lo_cum__kWh);
		secureUploadQueue.AddMeasurement(e_ret_hi_cum__kWh);
		secureUploadQueue.AddMeasurement(dsmr_version__0);

		if (!meter_code.empty())
		{
			Measurements::Measurement meter_code__str("meter_code__str", meter_code.c_str());
			secureUploadQueue.AddMeasurement(meter_code__str);
		}
	}
	else
	{
		// for smart meters with version DSMR 4 and up, up timestamps are availeble for the electricity meter readings;
		// so, make the Measurement use the smart meter timetamp by specifying its parsed timestamp
		Measurements::Measurement e_use_lo_cum__kWh("e_use_lo_cum__kWh", result.e_use_lo_cum__kWh, e_time_t);
		Measurements::Measurement e_use_hi_cum__kWh("e_use_hi_cum__kWh", result.e_use_hi_cum__kWh, e_time_t);
		Measurements::Measurement e_ret_lo_cum__kWh("e_ret_lo_cum__kWh", result.e_ret_lo_cum__kWh, e_time_t);
		Measurements::Measurement e_ret_hi_cum__kWh("e_ret_hi_cum__kWh", result.e_ret_hi_cum__kWh, e_time_t);
		Measurements::Measurement dsmr_version__0("dsmr_version__0", result.dsmrVersion, e_time_t);

		secureUploadQueue.AddMeasurement(e_use_lo_cum__kWh);
		secureUploadQueue.AddMeasurement(e_use_hi_cum__kWh);
		secureUploadQueue.AddMeasurement(e_ret_lo_cum__kWh);
		secureUploadQueue.AddMeasurement(e_ret_hi_cum__kWh);
		secureUploadQueue.AddMeasurement(dsmr_version__0);
		if (!meter_code.empty())
		{
			Measurements::Measurement meter_code__str("meter_code__str", meter_code.c_str(), e_time_t);
			secureUploadQueue.AddMeasurement(meter_code__str);
		}
	}
	if (g_time_t != TIME_UNKNOWN)
	{
		// only upload gas meter values if the timestamp is proper (e.g., not when result.timeGasMeasurement == "632525252525S")
		Measurements::Measurement g_use_cum__m3("g_use_cum__m3", result.g_use_cum__m3, g_time_t);
		secureUploadQueue.AddMeasurement(g_use_cum__m3);
	}
#if P1_HAS_LEDS
	GenericESP32Firmware::BlinkLED(GREEN_LED_D2_STATUS, 2);
#endif
#ifdef NFH_BOARD_P1BASE_V3
	ShowP1Status(p1base_after_read(true, result.dsmrVersion), &result);
	P1ChargeWindow(parse_interval__s);
#endif
}
