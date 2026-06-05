#include "system/includes.h"
#include "app_config.h"
#include "update/update.h"
#include "fs/fs.h"
#include "update/update_loader_download.h"
#include "update/net_update.h"

#ifdef CONFIG_NET_ENABLE
#ifndef CONFIG_NO_SDRAM_ENABLE
#include "http/http_cli.h"

#define PER_RECV_SIZE   (8 * 1024)

extern char *app_user_product_uuid(void);
extern char *user_product_batch_num(void);
extern void bt_ble_get_mac(char *mac);
extern char *qyai_ota_req_url_get(void);
extern char *qyai_ota_res_req_url_get(void);

static char http_update_status = 0;
static int __httpcli_cb(void *ctx, void *buf, unsigned int size, void *priv, httpin_status status)
{
    return 0;
}

int http_is_update(void)
{
    return http_update_status;
}

int get_update_data(const char *url, char res_ota)
{
    int error = 0;
    int ret = 0;
    int offset = 0;
    int remain = 0;
    void *fd = NULL;
    u8 *buffer = NULL;
    int data_offset = 0;
    int total_len = 0;
    const struct net_download_ops *ops = &http_ops;
    void *update_fd = NULL;

    httpcli_ctx *ctx = (httpcli_ctx *)calloc(1, sizeof(httpcli_ctx));
    if (NULL == ctx) {
        return -1;
    }

    ctx->url = url;
    ctx->connection = "close";
    ctx->timeout_millsec = 10000;
    ctx->cb = __httpcli_cb;

    error = ops->init(ctx);
    if (error != HERROR_OK) {
        goto __exit;
    }
    error = -1;

    total_len = ctx->content_length;
    if (total_len <= 0) {
        goto __exit;
    }

    if (res_ota) {
        update_fd = net_fopen(CONFIG_UPGRADE_OTA_FILE_NAME_RES, "w");
    } else {
        update_fd = net_fopen(CONFIG_UPGRADE_OTA_FILE_NAME, "w");
    }
    if (!update_fd) {
        goto __exit;
    }

    buffer = (u8 *)malloc(PER_RECV_SIZE);
    if (!buffer) {
        goto __exit;
    }
    http_update_status = true;
    while (total_len > 0) {
        if (total_len >= PER_RECV_SIZE) {
            remain = PER_RECV_SIZE;
            total_len -= PER_RECV_SIZE;
        } else {
            remain = total_len;
            total_len = 0;
        }

        do {
            ret = ops->read(ctx, (char *)buffer + offset, remain - offset);
            if (ret < 0) {
                goto __exit;
            }
            offset += ret;
        } while (remain != offset);

        if (data_offset == 0) {
            os_time_dly(500);   //此处延时是为了避免播放提示音时刷写flash导致卡音问题
        }

        ret = net_fwrite(update_fd, buffer, offset, 0);
        if (ret != offset) {
            log_e("upgrade core error : 0x%x\n", ret);
            goto __exit;
        }
        data_offset += offset;
        offset = 0;
    }

    error = 0;

__exit:
    if (buffer) {
        free(buffer);
    }
    if (update_fd) {
        net_fclose(update_fd, (char)error);
    }
    ops->close(ctx);
    free(ctx);
    http_update_status = false;
    return error;
}

static int http_ota_task(void *res_ota)
{
    unsigned char mac[6];
    char *url = malloc(1024);
    if (url) {
        extern int sys_connect_net_success(void);
        int to = 60 * 60;
        while (!sys_connect_net_success() && --to) {
            os_time_dly(100);
        }
#if (defined TCFG_VBAT_CHECK_EN && TCFG_VBAT_CHECK_EN == 1)
        extern int vbat_check_percent(void);
        int vbat_perc = vbat_check_percent();
        if (vbat_perc < 40) { //电压过低
            os_time_dly(10);
            vbat_perc = vbat_check_percent();
            if (vbat_perc < 40) { //电压过低不升级
                return 0;
            }
        }
#endif
        os_time_dly(500);//5秒钟后升级
        if (to) {
            bt_ble_get_mac(&mac);//使用蓝牙mac
            sprintf(url, "%s%02X%02X%02X%02X%02X%02X", qyai_ota_req_url_get(), mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
            char *uuid = app_user_product_uuid();
            if (!uuid) {
                printf("attention: use default product uuid!!!\r\n");
                //use product uuid of qy-ai
                uuid = user_product_uuid_default();
            }

            char *batch_num = user_product_batch_num();
            if (uuid && batch_num) {
                sprintf(url, "%s-%s-%s", url, uuid, batch_num);
                printf("-> OTA : %s-%s-%s\n", uuid, batch_num, OTA_VERSON);
            }
            sprintf(url, "%s-%s", url, OTA_VERSON);
            get_update_data(url, 0);

            sprintf(url, "%s%02X%02X%02X%02X%02X%02X", qyai_ota_res_req_url_get(), mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
            if (uuid && batch_num) {
                sprintf(url, "%s-%s-%s", url, uuid, batch_num);
                printf("-> OTA-RES : %s-%s-%s\n", uuid, batch_num, OTA_VERSON);
            }
            sprintf(url, "%s-%s", url, OTA_VERSON);
            get_update_data(url, 1);
        }
        free(url);
    }
    return 0;
}

int http_ota_init(void)
{
    if (production_io_is_enter() || is_production_test_enter(0) || system_is_alarm_wakeup()) {
        return 0;
    }
    return thread_fork("http_ota", 10, 2400, 0, 0, http_ota_task, NULL);
}

late_initcall(http_ota_init);
#endif
#endif
