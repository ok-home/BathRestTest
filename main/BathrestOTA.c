
#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "protocol_examples_common.h"

#include "Bath.h"


#define BUFFSIZE 1024
static const char *TAG = "native_ota_example";
/*an ota data write buffer ready to write to the flash*/
//static char ota_write_data[BUFFSIZE + 1] = {0};
static const esp_partition_t *update_partition = NULL;
static bool image_header_was_checked = false;
static int binary_file_length = 0;
static esp_ota_handle_t update_handle = 0;

int start_ota(void)
{
    return ESP_OK;
    esp_err_t err;
    ESP_LOGI(TAG, "Starting OTA example");

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (configured != running)
    {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
                 configured->address, running->address);
        ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
             running->type, running->subtype, running->address);

    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);

    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK)
    {
        ESP_LOGI(TAG, "esp_ota_begin failed ");
        return -1;
    }
    ESP_LOGI(TAG, "esp_ota_begin succeeded");

    image_header_was_checked = false;
    return ESP_OK;
}

int write_ota(int data_read, uint8_t *ota_write_data)
{
    return data_read;
    if (image_header_was_checked == false) // first segment
    {
        esp_app_desc_t new_app_info;
        if (data_read > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))
        {
            // check current version with downloading
            memcpy(&new_app_info, &ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
            ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);

            image_header_was_checked = true;
        }
        else
        {
            ESP_LOGI(TAG, "received package is not fit len");
            return -1;
        }
    }
    esp_err_t err = esp_ota_write(update_handle, (const void *)ota_write_data, data_read);
    if (err != ESP_OK)
    {
        return -1;
    }
    binary_file_length += data_read;
    ESP_LOGD(TAG, "Written image length %d", binary_file_length);
    return data_read;
}

int end_ota(void)
{
    return ESP_OK;
    esp_err_t err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        }
        ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
        return -1;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        return -1;
    }
    ESP_LOGI(TAG, "Prepare to restart system!");
    esp_restart();
    return  ESP_OK;
}
