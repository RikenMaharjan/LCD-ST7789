#include "heap_monitor.h"

#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

// Optional LVGL support
#if __has_include("lvgl.h")
#include "lvgl.h"
#define LVGL_AVAILABLE 1
#else
#define LVGL_AVAILABLE 0
#endif

#define TAG "HEAP_MON"

// Reference point for diff calculations
static heap_stats_t s_reference_stats = {0};
static bool s_initialized = false;

// Periodic monitoring
static TimerHandle_t s_periodic_timer = NULL;

/**
 * @brief Internal function to gather all heap statistics
 */
static void gather_stats(heap_stats_t *stats)
{
    if (!stats) return;
    
    memset(stats, 0, sizeof(heap_stats_t));
    
    stats->total_free = esp_get_free_heap_size();
    stats->minimum_free_ever = esp_get_minimum_free_heap_size();
    stats->largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    stats->dma_free = heap_caps_get_free_size(MALLOC_CAP_DMA);
    stats->internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    
#if CONFIG_SPIRAM
    stats->psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
#else
    stats->psram_free = 0;
#endif
}

void heap_monitor_init(void)
{
    gather_stats(&s_reference_stats);
    s_initialized = true;
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Heap Monitor Initialized");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Initial Free Heap:    %lu bytes", s_reference_stats.total_free);
    ESP_LOGI(TAG, "Largest Free Block:   %lu bytes", s_reference_stats.largest_free_block);
    ESP_LOGI(TAG, "DMA Capable:          %lu bytes", s_reference_stats.dma_free);
    ESP_LOGI(TAG, "Internal RAM:         %lu bytes", s_reference_stats.internal_free);
#if CONFIG_SPIRAM
    ESP_LOGI(TAG, "PSRAM:                %lu bytes", s_reference_stats.psram_free);
#endif
    ESP_LOGI(TAG, "========================================");
}

void heap_monitor_print(const char *label)
{
    heap_stats_t stats;
    gather_stats(&stats);
    
    const char *display_label = label ? label : "Heap Status";
    
    ESP_LOGI(TAG, "-------- %s --------", display_label);
    ESP_LOGI(TAG, "Free Heap:      %6lu bytes", stats.total_free);
    ESP_LOGI(TAG, "Min Free Ever:  %6lu bytes", stats.minimum_free_ever);
    ESP_LOGI(TAG, "Largest Block:  %6lu bytes", stats.largest_free_block);
    ESP_LOGI(TAG, "DMA Free:       %6lu bytes", stats.dma_free);
    
    // Calculate fragmentation indicator
    if (stats.total_free > 0) {
        uint32_t frag_percent = 100 - ((stats.largest_free_block * 100) / stats.total_free);
        ESP_LOGI(TAG, "Fragmentation:  %6lu %%", frag_percent);
    }
    
    // Warning if heap is getting low
    if (stats.total_free < 20000) {
        ESP_LOGW(TAG, "WARNING: Heap is critically low!");
    } else if (stats.total_free < 40000) {
        ESP_LOGW(TAG, "CAUTION: Heap is getting low");
    }
}

void heap_monitor_get_stats(heap_stats_t *stats)
{
    if (stats) {
        gather_stats(stats);
    }
}

void heap_monitor_print_diff(const char *label)
{
    if (!s_initialized) {
        ESP_LOGW(TAG, "Heap monitor not initialized. Call heap_monitor_init() first.");
        heap_monitor_init();
    }
    
    heap_stats_t current;
    gather_stats(&current);
    
    const char *display_label = label ? label : "Heap Diff";
    
    int32_t total_diff = (int32_t)current.total_free - (int32_t)s_reference_stats.total_free;
    int32_t dma_diff = (int32_t)current.dma_free - (int32_t)s_reference_stats.dma_free;
    int32_t internal_diff = (int32_t)current.internal_free - (int32_t)s_reference_stats.internal_free;
    
    ESP_LOGI(TAG, "======== %s ========", display_label);
    ESP_LOGI(TAG, "Current Free:    %6lu bytes", current.total_free);
    ESP_LOGI(TAG, "Reference Free:  %6lu bytes", s_reference_stats.total_free);
    ESP_LOGI(TAG, "--------------------------------");
    
    // Color-coded diff (negative means memory was allocated)
    if (total_diff < 0) {
        ESP_LOGW(TAG, "Total Change:    %6ld bytes (ALLOCATED)", total_diff);
    } else if (total_diff > 0) {
        ESP_LOGI(TAG, "Total Change:   +%6ld bytes (FREED)", total_diff);
    } else {
        ESP_LOGI(TAG, "Total Change:        0 bytes (NO CHANGE)");
    }
    
    if (dma_diff != 0) {
        ESP_LOGI(TAG, "DMA Change:     %+6ld bytes", dma_diff);
    }
    if (internal_diff != 0) {
        ESP_LOGI(TAG, "Internal Change:%+6ld bytes", internal_diff);
    }
    
    // Show minimum free ever (useful for detecting peak usage)
    ESP_LOGI(TAG, "Min Free Ever:   %6lu bytes", current.minimum_free_ever);
    uint32_t peak_usage = s_reference_stats.total_free - current.minimum_free_ever;
    ESP_LOGI(TAG, "Peak Usage:      %6lu bytes", peak_usage);
}

void heap_monitor_set_checkpoint(void)
{
    gather_stats(&s_reference_stats);
    ESP_LOGI(TAG, "New checkpoint set: %lu bytes free", s_reference_stats.total_free);
}

bool heap_monitor_is_low(uint32_t threshold_bytes)
{
    return esp_get_free_heap_size() < threshold_bytes;
}

/**
 * @brief Timer callback for periodic monitoring
 */
static void periodic_timer_callback(TimerHandle_t timer)
{
    heap_monitor_print("Periodic Check");
}

void heap_monitor_periodic(bool enable, uint32_t interval_ms)
{
    if (enable) {
        // Ensure minimum interval
        if (interval_ms < 1000) {
            interval_ms = 1000;
        }
        
        if (s_periodic_timer == NULL) {
            s_periodic_timer = xTimerCreate(
                "heap_mon",
                pdMS_TO_TICKS(interval_ms),
                pdTRUE,  // Auto-reload
                NULL,
                periodic_timer_callback
            );
            
            if (s_periodic_timer == NULL) {
                ESP_LOGE(TAG, "Failed to create periodic timer");
                return;
            }
        } else {
            // Update period if timer already exists
            xTimerChangePeriod(s_periodic_timer, pdMS_TO_TICKS(interval_ms), 0);
        }
        
        if (xTimerStart(s_periodic_timer, 0) != pdPASS) {
            ESP_LOGE(TAG, "Failed to start periodic timer");
        } else {
            ESP_LOGI(TAG, "Periodic heap monitoring enabled (every %lu ms)", interval_ms);
        }
    } else {
        if (s_periodic_timer != NULL) {
            xTimerStop(s_periodic_timer, 0);
            ESP_LOGI(TAG, "Periodic heap monitoring disabled");
        }
    }
}

void heap_monitor_print_lvgl(const char *label)
{
#if LVGL_AVAILABLE && defined(LV_USE_BUILTIN_MALLOC) && LV_USE_BUILTIN_MALLOC
    const char *display_label = label ? label : "LVGL Memory";
    
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    
    ESP_LOGI(TAG, "-------- %s --------", display_label);
    ESP_LOGI(TAG, "LVGL Total:     %6lu bytes", (uint32_t)mon.total_size);
    ESP_LOGI(TAG, "LVGL Used:      %6lu bytes (%d%%)", 
             (uint32_t)(mon.total_size - mon.free_size), mon.used_pct);
    ESP_LOGI(TAG, "LVGL Free:      %6lu bytes", (uint32_t)mon.free_size);
    ESP_LOGI(TAG, "LVGL Frag:      %6d %%", mon.frag_pct);
    ESP_LOGI(TAG, "LVGL Max Used:  %6lu bytes", (uint32_t)mon.max_used);
#else
    ESP_LOGI(TAG, "LVGL uses system heap (LV_MEM_CUSTOM=1) or monitor unavailable");
    // Fall back to regular heap print
    heap_monitor_print(label);
#endif
}