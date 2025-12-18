#ifndef __HEAP_MONITOR_H__
#define __HEAP_MONITOR_H__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Heap statistics structure
 */
typedef struct {
    uint32_t total_free;           // Total free heap bytes
    uint32_t minimum_free_ever;    // Minimum free heap since boot
    uint32_t largest_free_block;   // Largest contiguous free block
    uint32_t dma_free;             // Free DMA-capable memory
    uint32_t internal_free;        // Free internal RAM
    uint32_t psram_free;           // Free PSRAM (if available)
} heap_stats_t;

/**
 * @brief Initialize heap monitor
 * 
 * Call this early in app_main() before any significant allocations.
 * Records the initial heap state for comparison.
 */
void heap_monitor_init(void);

/**
 * @brief Print current heap statistics to log
 * 
 * @param label A descriptive label for this checkpoint (e.g., "After UI init")
 */
void heap_monitor_print(const char *label);

/**
 * @brief Get current heap statistics
 * 
 * @param stats Pointer to structure to fill with current stats
 */
void heap_monitor_get_stats(heap_stats_t *stats);

/**
 * @brief Print heap usage comparison since init or last checkpoint
 * 
 * Shows how much memory has been allocated/freed since the reference point.
 * 
 * @param label A descriptive label for this checkpoint
 */
void heap_monitor_print_diff(const char *label);

/**
 * @brief Set current heap state as new reference point for diff calculations
 */
void heap_monitor_set_checkpoint(void);

/**
 * @brief Check if heap is critically low
 * 
 * @param threshold_bytes Warning threshold in bytes (e.g., 10000)
 * @return true if free heap is below threshold
 */
bool heap_monitor_is_low(uint32_t threshold_bytes);

/**
 * @brief Enable/disable periodic heap monitoring
 * 
 * When enabled, prints heap stats every interval_ms milliseconds.
 * 
 * @param enable true to enable, false to disable
 * @param interval_ms Interval between prints in milliseconds (min 1000)
 */
void heap_monitor_periodic(bool enable, uint32_t interval_ms);

/**
 * @brief Print LVGL memory usage (if LV_USE_MEM_MONITOR is enabled)
 * 
 * @param label A descriptive label for this checkpoint
 */
void heap_monitor_print_lvgl(const char *label);

/**
 * @brief Macro for quick heap checkpoint with file and line info
 */
#define HEAP_CHECKPOINT() heap_monitor_print(__FILE__ ":" STRINGIFY(__LINE__))

#define STRINGIFY(x) STRINGIFY2(x)
#define STRINGIFY2(x) #x

/**
 * @brief Macro to check heap and warn if low
 */
#define HEAP_CHECK_LOW(threshold) \
    do { \
        if (heap_monitor_is_low(threshold)) { \
            heap_monitor_print("LOW MEMORY WARNING"); \
        } \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif // __HEAP_MONITOR_H__