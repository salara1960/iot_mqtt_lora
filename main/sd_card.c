#include "hdr.h"
#ifdef SET_SD_CARD

#include "mqtt.h"
//#include "sd_card.h"

const char *TAGSD = "CARD";
const char *bname = "/sdcard";
const uint32_t blen = 1024;

//-----------------------------------------------------------------------------------
void sdcard_info(const sdmmc_card_t *card)
{
    uint64_t szm = (uint64_t)card->csd.capacity; szm *= card->csd.sector_size; szm /= (1024 * 1024);
    ets_printf("%sName: %s\nType: %s\n",
		GREEN_COLOR, card->cid.name, (card->ocr & SD_OCR_SDHC_CAP) ? "SDHC/SDXC" : "SDSC", WHITE_COLOR);
    ets_printf("%sSpeed: %u MHz\nSize: %llu MB%s\n",
		GREEN_COLOR,card->csd.tr_speed/1000000, szm, WHITE_COLOR);
    ets_printf("%sCSD: ver=%d, sector_size=%d, capacity=%d read_bl_len=%d%s\n",
		GREEN_COLOR, card->csd.csd_ver, card->csd.sector_size, card->csd.capacity, card->csd.read_block_len, WHITE_COLOR);
    ets_printf("%sSCR: sd_spec=%d, bus_width=%d%s\n",
		GREEN_COLOR, card->scr.sd_spec, card->scr.bus_width, WHITE_COLOR);
}
//-----------------------------------------------------------------------------------
// print <= 16 line from file
void print_file(const char *dir, char *filename)
{
    char *fname = (char *)calloc(1, strlen(dir) + strlen(filename) + 3);
    if (fname) {
	sprintf(fname, "%s/%s", dir, filename);
	FILE *ff = fopen(fname, RDONLY);
	if (ff) {
	    char *stx = (char *)calloc(1, blen);
	    if (stx) {
		uint32_t cnt = 0;
		while (fgets(stx, blen-1, ff)) {
		    cnt++;
		    if (cnt <= 16) ets_printf(stx); else break;
		    memset(stx, 0, blen);
		}
		free(stx);
	    }
	    fclose(ff);
	}
	free(fname);
    }
}
//-----------------------------------------------------------------------------------
void sdcard_task(void *arg)
{
    total_task++;

    ets_printf("%s[%s] Start sdcard_task (SPI mode) | FreeMem %u%s\n", GREEN_COLOR, TAGSD, xPortGetFreeHeapSize(), STOP_COLOR);

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
//    slot_config.gpio_miso = 2;
//    slot_config.gpio_mosi = 15;
//    slot_config.gpio_sck  = 14;
//    slot_config.gpio_cs   = 13;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5
    };

    sdmmc_card_t *card = NULL;
    esp_err_t mnt = esp_vfs_fat_sdmmc_mount(bname, &host, &slot_config, &mount_config, &card);

    if (mnt == ESP_OK) {

	sdcard_info(card);

	char line[128] = {0}, nm[256];
	sprintf(line, "%s/", bname);
	ets_printf("%s[%s] Open DIR '%s':%s\n", GREEN_COLOR, TAGSD, line, WHITE_COLOR);
	struct dirent *de = NULL;
	DIR *dir = opendir(line);
	if (dir) {
	    uint32_t kol = 0;
	    struct stat sta;
	    while ( (de = readdir(dir)) != NULL ) {
		memset(nm, 0, 256);
		sprintf(nm, "%s%s", line, de->d_name);
		stat(nm, &sta);
		ets_printf("%s\tfile: type=%d name='%s' size=%u%s\n", GREEN_COLOR, de->d_type, de->d_name, sta.st_size, WHITE_COLOR);
		if ( (strstr(de->d_name, ".plt")) || (strstr(de->d_name, ".PLT")) ||
		     (strstr(de->d_name, ".txt")) || (strstr(de->d_name, ".TXT")) ) print_file(bname, de->d_name);
		seekdir(dir, ++kol);
	    }
	} else {
	    ESP_LOGE(TAGSD, "Open DIR '%s' Error.", line);
	}

	esp_vfs_fat_sdmmc_unmount();// All done, unmount partition and disable SDMMC or SPI peripheral

    } else {

	ESP_LOGE(TAGSD, "Failed to mount filesystem on card '%s' (%d)\n", bname, mnt);

    }

    ets_printf("%s[%s] Stop sdcard_task | FreeMem %u%s\n", GREEN_COLOR, TAGSD, xPortGetFreeHeapSize(), STOP_COLOR);
    if (total_task) total_task--;

    vTaskDelete(NULL);
}
//-----------------------------------------------------------------------------------
#endif
