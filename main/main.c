/**
 * @file main.c
 * @brief Módulo de lectura ADC para sensores de Humedad (H_IN), pH (PH_SENSE)
 *        y Temperatura (T_SENSE).
 *
 * Los sensores se organizan en GRUPOS según el dispositivo físico:
 *   - Grupo PH_TEMP: PH_SENSE (GPIO7) y T_SENSE (GPIO8) vienen del mismo sensor
 *   - Grupo HUMIDITY: H_IN (GPIO6) es un sensor independiente
 *
 * Este módulo expone funciones para leer grupos de sensores con estabilización.
 * Las "tareas" (conjuntos de acciones recibidas por MQTT) se definirán aparte;
 * cada tarea llamará a las funciones de lectura que necesite.
 *
 * Asignación de pines (ADC1 — compatible con WiFi/MQTT):
 *   H_IN      → GPIO6  (ADC1_CH5)
 *   PH_SENSE  → GPIO7  (ADC1_CH6)
 *   T_SENSE   → GPIO8  (ADC1_CH7)
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

/* ─── Etiqueta de log ─── */
static const char *TAG = "ADC_SENSORS";

/* ─── Configuración de canales ─── */
#define H_IN_CHANNEL      ADC_CHANNEL_5   // GPIO6
#define PH_SENSE_CHANNEL  ADC_CHANNEL_6   // GPIO7
#define T_SENSE_CHANNEL   ADC_CHANNEL_7   // GPIO8
#define ADC_ATTEN         ADC_ATTEN_DB_12 // Rango ~0–3.1 V
#define ADC_BITWIDTH      ADC_BITWIDTH_DEFAULT

/* ─── Parámetros de estabilización ─── */
#define STABILIZATION_SAMPLES    5       // Muestras descartadas para estabilizar
#define STABILIZATION_DELAY_MS   50      // Pausa entre muestras descartadas
#define READING_SAMPLES          10      // Muestras promediadas para la lectura final
#define READING_DELAY_MS         20      // Pausa entre muestras de lectura

/* ─── Conversión de pH ─── */
#define PH_NEUTRAL_VOLTAGE  2.5f    // Voltaje a pH 7 (punto neutro del sensor)
#define PH_STEP             0.05916f // Voltaje por unidad de pH (Nernst a 25°C)

/* ─── Conversión de humedad de suelo ───
 * Valores típicos para sensor capacitivo. AJUSTAR con calibración real:
 *   1. Pon el sensor al aire (seco) y anota el voltaje → H_DRY_MV
 *   2. Pon el sensor en agua o suelo saturado → H_WET_MV
 */
#define H_DRY_MV    2800    // mV cuando el sensor está seco (al aire)
#define H_WET_MV    1200    // mV cuando el sensor está en suelo saturado/agua

/* ─── Conversión de Temperatura (NTC) ───
 * Ecuación Beta Steinhart-Hart simplificada: 1/T = 1/T0 + 1/B * ln(R/R0)
 */
#define NTC_R0      10000.0f  // Resistencia a 25°C (10k)
#define NTC_BETA    3950.0f   // Coeficiente Beta del termistor
#define NTC_R_SERIE 10000.0f  // Resistencia en serie del divisor (10k)
#define VCC_MV      3300.0f   // Voltaje de alimentación (3.3V)
#define T0_KELVIN   298.15f   // 25°C en Kelvin

/* ─── Intervalo del loop de demo ─── */
#define DEMO_INTERVAL_MS  3000

/* ─── Handle del ADC ─── */
static adc_oneshot_unit_handle_t adc1_handle = NULL;

/* ════════════════════════════════════════════════════════════
 *  SENSORES INDIVIDUALES
 * ════════════════════════════════════════════════════════════ */

/** Información de un sensor individual (incluye su propia calibración) */
typedef struct {
    const char        *name;
    adc_channel_t      channel;
    adc_cali_handle_t  cali_handle;   // Handle de calibración propio
    bool               calibrated;    // ¿Se calibró exitosamente?
} sensor_info_t;

/** Resultado de lectura de un sensor */
typedef struct {
    const char *sensor_name;
    int         raw_avg;          // Promedio del valor crudo
    int         voltage_mv;       // Promedio en milivolts (si hay calibración)
    float       converted_value;  // Valor convertido a unidad real (pH, °C, etc.)
    const char *unit;             // Unidad del valor convertido ("pH", "°C", etc.)
    bool        has_conversion;   // ¿Tiene conversión a unidad real?
    bool        valid;            // Lectura exitosa
} sensor_reading_t;

/* Definición de todos los sensores individuales.
 * NO son const porque la calibración se inicializa en runtime. */
static sensor_info_t sensor_h_in     = { "H_IN",      H_IN_CHANNEL,     NULL, false };
static sensor_info_t sensor_ph_sense = { "PH_SENSE",  PH_SENSE_CHANNEL, NULL, false };
static sensor_info_t sensor_t_sense  = { "T_SENSE",   T_SENSE_CHANNEL,  NULL, false };

/* Array con todos los sensores (para inicialización) */
static sensor_info_t *all_sensors[] = {
    &sensor_h_in,
    &sensor_ph_sense,
    &sensor_t_sense,
};
#define NUM_SENSORS (sizeof(all_sensors) / sizeof(all_sensors[0]))

/* ════════════════════════════════════════════════════════════
 *  GRUPOS DE SENSORES
 *
 *  Un grupo agrupa sensores que pertenecen al mismo dispositivo
 *  físico y deben leerse juntos.
 * ════════════════════════════════════════════════════════════ */

/** Identificadores de grupo */
typedef enum {
    SENSOR_GROUP_PH_TEMP,     // pH + Temperatura (mismo sensor físico)
    SENSOR_GROUP_HUMIDITY,    // Humedad (sensor independiente)
    SENSOR_GROUP_MAX
} sensor_group_t;

/** Nombres legibles para log */
static const char *group_names[] = {
    [SENSOR_GROUP_PH_TEMP]  = "PH+TEMPERATURA",
    [SENSOR_GROUP_HUMIDITY] = "HUMEDAD",
};

/** Definición de un grupo: qué sensores contiene */
typedef struct {
    const char    *name;
    sensor_info_t *sensors[3];   // Max 3 sensores por grupo
    int            sensor_count;
} sensor_group_info_t;

static sensor_group_info_t groups[] = {
    [SENSOR_GROUP_PH_TEMP] = {
        .name         = "PH+TEMPERATURA",
        .sensors      = { &sensor_ph_sense, &sensor_t_sense },
        .sensor_count = 2,
    },
    [SENSOR_GROUP_HUMIDITY] = {
        .name         = "HUMEDAD",
        .sensors      = { &sensor_h_in },
        .sensor_count = 1,
    },
};

/* ════════════════════════════════════════════════════════════
 *  Calibración ADC
 * ════════════════════════════════════════════════════════════ */
static bool adc_calibration_init_channel(adc_unit_t unit, adc_channel_t channel,
                                         adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Calibración: Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id  = unit,
            .chan     = channel,
            .atten   = atten,
            .bitwidth = ADC_BITWIDTH,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Calibración: Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id  = unit,
            .atten   = atten,
            .bitwidth = ADC_BITWIDTH,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibración exitosa");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse no soporta calibración, lecturas sin calibrar");
    } else {
        ESP_LOGE(TAG, "Calibración inválida");
    }

    return calibrated;
}

static void adc_calibration_deinit(adc_cali_handle_t handle)
{
    if (handle == NULL) return;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "Desregistrando Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "Desregistrando Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}

/* ════════════════════════════════════════════════════════════
 *  Inicialización del ADC
 * ════════════════════════════════════════════════════════════ */
static void adc_init(void)
{
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id  = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc1_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH,
        .atten    = ADC_ATTEN,
    };

    for (int i = 0; i < NUM_SENSORS; i++) {
        /* Configurar el canal ADC */
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle,
                                                    all_sensors[i]->channel,
                                                    &chan_cfg));

        /* Crear calibración individual para este canal */
        all_sensors[i]->calibrated = adc_calibration_init_channel(
            ADC_UNIT_1,
            all_sensors[i]->channel,
            ADC_ATTEN,
            &all_sensors[i]->cali_handle);

        ESP_LOGI(TAG, "Canal %s (CH%d) configurado — calibración: %s",
                 all_sensors[i]->name, all_sensors[i]->channel,
                 all_sensors[i]->calibrated ? "SÍ" : "NO");
    }
}

/* ════════════════════════════════════════════════════════════
 *  Lectura estabilizada de un sensor individual
 *
 *  Proceso:
 *  1. ESTABILIZACIÓN: Se toman STABILIZATION_SAMPLES lecturas
 *     y se descartan. Esto permite que el circuito del sensor
 *     se estabilice tras un cambio de canal o tras despertar.
 *  2. MUESTREO: Se toman READING_SAMPLES lecturas y se
 *     promedian para reducir el ruido.
 * ════════════════════════════════════════════════════════════ */
static sensor_reading_t adc_read_sensor_stabilized(const sensor_info_t *sensor)
{
    sensor_reading_t result = {
        .sensor_name = sensor->name,
        .raw_avg     = 0,
        .voltage_mv  = 0,
        .valid       = false,
    };
    int raw_value = 0;

    /* ── Fase 1: Estabilización (descartar lecturas iniciales) ── */
    for (int i = 0; i < STABILIZATION_SAMPLES; i++) {
        esp_err_t ret = adc_oneshot_read(adc1_handle, sensor->channel, &raw_value);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "  [%s] Error en estabilización %d: %s",
                     sensor->name, i, esp_err_to_name(ret));
            return result;
        }
        vTaskDelay(pdMS_TO_TICKS(STABILIZATION_DELAY_MS));
    }

    /* ── Fase 2: Muestreo y promediado ── */
    int64_t raw_sum = 0;
    int valid_samples = 0;

    for (int i = 0; i < READING_SAMPLES; i++) {
        esp_err_t ret = adc_oneshot_read(adc1_handle, sensor->channel, &raw_value);
        if (ret == ESP_OK) {
            raw_sum += raw_value;
            valid_samples++;
        } else {
            ESP_LOGW(TAG, "  [%s] Error en muestra %d: %s",
                     sensor->name, i, esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(READING_DELAY_MS));
    }

    if (valid_samples == 0) {
        ESP_LOGE(TAG, "  [%s] Ninguna muestra válida", sensor->name);
        return result;
    }

    result.raw_avg = (int)(raw_sum / valid_samples);
    result.valid = true;

    /* Usar la calibración PROPIA de este sensor */
    if (sensor->calibrated) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(sensor->cali_handle,
                                                 result.raw_avg,
                                                 &result.voltage_mv));
    }

    return result;
}

/* ════════════════════════════════════════════════════════════
 *  Conversión de voltaje a pH
 *
 *  Fórmula: pH = 7 + (2.5 - probeVoltage) / PH_STEP
 *
 *  - 2.5V es el voltaje de referencia a pH 7 (neutro)
 *  - PH_STEP (0.05916 V) es la pendiente de Nernst a 25°C:
 *    cada unidad de pH cambia el voltaje ~59.16 mV
 *  - Si el voltaje < 2.5V → pH > 7 (básico/alcalino)
 *  - Si el voltaje > 2.5V → pH < 7 (ácido)
 * ════════════════════════════════════════════════════════════ */
static void adc_convert_ph(sensor_reading_t *reading)
{
    if (!reading->valid || reading->voltage_mv == 0) return;

    float probe_voltage = reading->voltage_mv / 1000.0f;  // mV → V
    reading->converted_value = 7.0f + (PH_NEUTRAL_VOLTAGE - probe_voltage) / PH_STEP;
    reading->unit = "pH";
    reading->has_conversion = true;
}

/* ════════════════════════════════════════════════════════════
 *  Conversión de voltaje a humedad de suelo (%)
 *
 *  Fórmula: %H = (V_seco - V_medido) / (V_seco - V_mojado) × 100
 *
 *  Los sensores capacitivos de suelo producen MENOS voltaje cuando
 *  hay más humedad (el agua aumenta la capacitancia, lo cual reduce
 *  la frecuencia de oscilación del sensor, y el circuito convierte
 *  eso a un voltaje menor).
 *
 *  - V_seco (~2800 mV): sensor al aire, 0% humedad
 *  - V_mojado (~1200 mV): sensor en suelo saturado, 100% humedad
 *  - Se limita el resultado a 0–100% por seguridad
 *
 *  IMPORTANTE: Los valores H_DRY_MV y H_WET_MV deben calibrarse
 *  con el sensor real. Los valores por defecto son aproximados.
 * ════════════════════════════════════════════════════════════ */
static void adc_convert_humidity(sensor_reading_t *reading)
{
    if (!reading->valid || reading->voltage_mv == 0) return;

    /* Evitar división por cero si la calibración es incorrecta */
    if (H_DRY_MV == H_WET_MV) {
        ESP_LOGE(TAG, "Error: H_DRY_MV y H_WET_MV son iguales");
        return;
    }

    float humidity = (float)(H_DRY_MV - reading->voltage_mv)
                   / (float)(H_DRY_MV - H_WET_MV) * 100.0f;

    /* Limitar a rango válido */
    if (humidity < 0.0f)   humidity = 0.0f;
    if (humidity > 100.0f) humidity = 100.0f;

    reading->converted_value = humidity;
    reading->unit = "%RH";
    reading->has_conversion = true;
}

/* ════════════════════════════════════════════════════════════
 *  Conversión de voltaje a Temperatura (°C)
 *
 *  Modelo: Divisor de voltaje con NTC a GND (R_serie a VCC)
 *  Vout = Vcc * R_ntc / (R_serie + R_ntc)
 *  => R_ntc = R_serie * Vout / (Vcc - Vout)
 * ════════════════════════════════════════════════════════════ */
static void adc_convert_temperature(sensor_reading_t *reading)
{
    if (!reading->valid || reading->voltage_mv == 0) return;

    /* Evitar división por cero si Vout >= VCC (corto a VCC) */
    if (reading->voltage_mv >= VCC_MV) {
        ESP_LOGW(TAG, "Voltaje NTC saturado (posible corto o desconexión)");
        return;
    }

    /* Calcular resistencia del termistor */
    float v_out = (float)reading->voltage_mv;
    float r_ntc = NTC_R_SERIE * v_out / (VCC_MV - v_out);

    /* Ecuación Beta: 1/T = 1/T0 + (1/B) * ln(R/R0) */
    float inv_t = (1.0f / T0_KELVIN) + (1.0f / NTC_BETA) * log(r_ntc / NTC_R0);
    float t_kelvin = 1.0f / inv_t;
    float t_celsius = t_kelvin - 273.15f;

    reading->converted_value = t_celsius;
    reading->unit = "°C";
    reading->has_conversion = true;
}

/* ════════════════════════════════════════════════════════════
 *  Imprimir resultado de un sensor
 * ════════════════════════════════════════════════════════════ */
static void adc_print_reading(const sensor_reading_t *reading)
{
    if (!reading->valid) {
        ESP_LOGW(TAG, "  %-10s : LECTURA INVÁLIDA", reading->sensor_name);
        return;
    }

    if (reading->voltage_mv != 0) {
        if (reading->has_conversion) {
            ESP_LOGI(TAG, "  %-10s : Raw = %4d  |  %4d mV  |  %.2f %s",
                     reading->sensor_name, reading->raw_avg,
                     reading->voltage_mv, reading->converted_value,
                     reading->unit);
        } else {
            ESP_LOGI(TAG, "  %-10s : Raw = %4d  |  Voltaje = %4d mV",
                     reading->sensor_name, reading->raw_avg,
                     reading->voltage_mv);
        }
    } else {
        ESP_LOGI(TAG, "  %-10s : Raw = %4d  |  (sin calibrar)",
                 reading->sensor_name, reading->raw_avg);
    }
}

/* ════════════════════════════════════════════════════════════
 *  API PÚBLICA: Leer un grupo de sensores
 *
 *  Lee todos los sensores del grupo con estabilización completa.
 *  Retorna las lecturas en el array 'out_readings'.
 *
 *  Esta función será llamada por las tareas MQTT a futuro.
 *  Ejemplo de uso futuro:
 *    sensor_reading_t readings[2];
 *    int n = adc_read_group(SENSOR_GROUP_PH_TEMP, readings, 2);
 * ════════════════════════════════════════════════════════════ */
int adc_read_group(sensor_group_t group, sensor_reading_t *out_readings,
                   int max_readings)
{
    if (group >= SENSOR_GROUP_MAX) {
        ESP_LOGE(TAG, "Grupo inválido: %d", group);
        return 0;
    }

    const sensor_group_info_t *grp = &groups[group];

    ESP_LOGI(TAG, "──── Leyendo grupo: %s (%d sensores) ────",
             grp->name, grp->sensor_count);

    int count = (grp->sensor_count < max_readings)
                ? grp->sensor_count : max_readings;

    for (int i = 0; i < count; i++) {
        out_readings[i] = adc_read_sensor_stabilized(grp->sensors[i]);

        /* Aplicar conversión según el sensor */
        if (grp->sensors[i]->channel == PH_SENSE_CHANNEL) {
            adc_convert_ph(&out_readings[i]);
        } else if (grp->sensors[i]->channel == H_IN_CHANNEL) {
            adc_convert_humidity(&out_readings[i]);
        } else if (grp->sensors[i]->channel == T_SENSE_CHANNEL) {
            adc_convert_temperature(&out_readings[i]);
        }

        adc_print_reading(&out_readings[i]);
    }

    return count;
}

/* ════════════════════════════════════════════════════════════
 *  API PÚBLICA: Leer todos los sensores
 *
 *  Conveniencia para leer todos los grupos de una vez.
 * ════════════════════════════════════════════════════════════ */
int adc_read_all(sensor_reading_t *out_readings, int max_readings)
{
    ESP_LOGI(TAG, "════════ Lectura completa de todos los sensores ════════");

    int total = 0;

    for (int g = 0; g < SENSOR_GROUP_MAX && total < max_readings; g++) {
        int n = adc_read_group((sensor_group_t)g,
                               &out_readings[total],
                               max_readings - total);
        total += n;
    }

    return total;
}

/* ════════════════════════════════════════════════════════════
 *  Punto de entrada
 * ════════════════════════════════════════════════════════════ */
void app_main(void)
{
    ESP_LOGI(TAG, "=== Iniciando módulo de lectura ADC ===");
    ESP_LOGI(TAG, "Grupo PH+TEMP: PH_SENSE (GPIO7), T_SENSE (GPIO8)");
    ESP_LOGI(TAG, "Grupo HUMEDAD: H_IN (GPIO6)");
    ESP_LOGI(TAG, "Unidad: ADC1 (compatible con WiFi/MQTT)");

    adc_init();

    /*
     * DEMO: Para pruebas sin MQTT.
     * Cicla entre leer cada grupo individualmente y luego todos juntos.
     * A futuro, cada "tarea" MQTT llamará a adc_read_group() o
     * adc_read_all() según lo que necesite.
     */
    sensor_reading_t readings[NUM_SENSORS];
    int demo_step = 0;

    while (1) {
        switch (demo_step) {
            case 0:
                ESP_LOGI(TAG, "\n>>> DEMO: Leyendo grupo PH+TEMPERATURA <<<");
                adc_read_group(SENSOR_GROUP_PH_TEMP, readings, NUM_SENSORS);
                break;
            case 1:
                ESP_LOGI(TAG, "\n>>> DEMO: Leyendo grupo HUMEDAD <<<");
                adc_read_group(SENSOR_GROUP_HUMIDITY, readings, NUM_SENSORS);
                break;
            case 2:
                ESP_LOGI(TAG, "\n>>> DEMO: Leyendo TODOS los sensores <<<");
                adc_read_all(readings, NUM_SENSORS);
                break;
        }

        demo_step = (demo_step + 1) % 3;
        vTaskDelay(pdMS_TO_TICKS(DEMO_INTERVAL_MS));
    }

    /* Limpieza (no se alcanza en este loop, pero buena práctica) */
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
    for (int i = 0; i < NUM_SENSORS; i++) {
        adc_calibration_deinit(all_sensors[i]->cali_handle);
    }
}