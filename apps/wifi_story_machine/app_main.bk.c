#include "system/includes.h"
#include "asm/system_reset_reason.h"
#include "app_config.h"
#include "event/key_event.h"
#include "event/bt_event.h"
#include "event/device_event.h"
#include "event/net_event.h"

/*
 * Minimal QYAI app:
 * - WiFi networking is started by wireless_net_init().
 * - BLE mini-program provisioning is started inside wifi_app_task.c.
 * - Online dialogue and QYAI LLM access run in ai_speaker mode.
 * - Offline preset voice commands stay in the local ASR callbacks.
 */

extern int sys_net_channel_read(void);
extern int sys_net_channel_write(int channel);
extern int sys_net_mode_read(void);
extern int sys_net_mode_write(int mode);
extern int wireless_net_init(void);
extern int flash_protect_check_match(void);
extern int production_io_is_enter(void);
extern void product_test_task_init(int mode);
extern int vbat_check_percent(void);
extern int user_is_soft_power_off_read(void);
extern void sys_power_poweroff(void);
extern void dac_mute_control(char enable, char force);
extern int audio_app_mode_switch(char *name);

#if (defined TCFG_LED_STATUES_PORT && TCFG_LED_STATUES_PORT != -1)
extern void led_init(void);
#endif

const struct irq_info irq_info_table[] = {
#ifdef CONFIG_IPMASK_ENABLE
    { IRQ_SOFT5_IDX,      6,   0    },
    { IRQ_SOFT4_IDX,      6,   1    },
#endif
#if CPU_CORE_NUM == 1
    { IRQ_SOFT5_IDX,      7,   0    },
    { IRQ_SOFT4_IDX,      7,   1    },
    { -2,               -2,   -2   },
#endif
    { -1,     -1,   -1    },
};

#if defined CONFIG_NO_SDRAM_ENABLE
#define SEC_IN_CACHRAM  SEC_USED(.cache_ram)
#define SEC_IN_SRAM
#elif defined CONFIG_SFC_ENABLE
#define SEC_IN_CACHRAM  SEC_USED(.sram)
#define SEC_IN_SRAM  SEC_USED(.sram)
#else
#define SEC_IN_CACHRAM
#define SEC_IN_SRAM
#endif

#ifdef CONFIG_NO_SDRAM_ENABLE
#define SYS_TIMER_STK_SIZE 800
#define APP_CORE_STK_SIZE 800
#define WIFI_TASKLET_STK_SIZE 800
#else
#define SYS_TIMER_STK_SIZE 3000
#define APP_CORE_STK_SIZE 2600
#define WIFI_TASKLET_STK_SIZE 1400
#endif

#define SYS_TIMER_Q_SIZE 128
#define APP_CORE_Q_SIZE 1024
#define SYS_EVENT_STK_SIZE 512
#define SYSTIMER_STK_SIZE 256
#define WIFI_CMDQ_STK_SIZE 300
#define WIFI_MLME_STK_SIZE 800
#define WIFI_RX_STK_SIZE 256

static SEC_IN_SRAM u8 sys_timer_tcb_stk_q[sizeof(StaticTask_t) + SYS_TIMER_STK_SIZE * 4 + sizeof(struct task_queue) + SYS_TIMER_Q_SIZE] ALIGNE(4);
static SEC_IN_SRAM u8 systimer_tcb_stk_q[sizeof(StaticTask_t) + SYSTIMER_STK_SIZE * 4] ALIGNE(4);
static SEC_IN_SRAM u8 sys_event_tcb_stk_q[sizeof(StaticTask_t) + SYS_EVENT_STK_SIZE * 4] ALIGNE(4);
static u8 app_core_tcb_stk_q[sizeof(StaticTask_t) + APP_CORE_STK_SIZE * 4 + sizeof(struct task_queue) + APP_CORE_Q_SIZE] ALIGNE(4);

#ifdef CONFIG_WIFI_ENABLE
static SEC_IN_SRAM u8 wifi_tasklet_tcb_stk_q[sizeof(struct thread_parm) + WIFI_TASKLET_STK_SIZE * 4] ALIGNE(4);
static SEC_IN_CACHRAM u8 wifi_cmdq_tcb_stk_q[sizeof(struct thread_parm) + WIFI_CMDQ_STK_SIZE * 4] ALIGNE(4);
static u8 wifi_mlme_tcb_stk_q[sizeof(struct thread_parm) + WIFI_MLME_STK_SIZE * 4] ALIGNE(4);
static SEC_IN_SRAM u8 wifi_rx_tcb_stk_q[sizeof(struct thread_parm) + WIFI_RX_STK_SIZE * 4] ALIGNE(4);
#endif

const struct task_info task_info_table[] = {
    {"thread_fork_kill",    25,      256,   0     },
    {"app_core",            15,     APP_CORE_STK_SIZE,    APP_CORE_Q_SIZE,       app_core_tcb_stk_q },
    {"sys_event",           29,     SYS_EVENT_STK_SIZE,    0,                    sys_event_tcb_stk_q },
    {"systimer",            14,     SYSTIMER_STK_SIZE,     0,                    systimer_tcb_stk_q },
    {"sys_timer",            9,     SYS_TIMER_STK_SIZE,    SYS_TIMER_Q_SIZE,     sys_timer_tcb_stk_q },

    {"audio_server",        16,      512,   64    },
    {"audio_mix",           28,      512,   0     },
    {"audio_encoder",       25,      384,   64    },
    {"speex_encoder",       13,      512,   0     },
    {"mp3_encoder",         13,      768,   0     },
    {"opus_encoder",        13,     1536,   0     },
    {"vad_encoder",         14,      768,   0     },
    {"aec_encoder",         13,     1024,   0     },
    {"dns_encoder",         13,      512,   0     },

    {"tcpip_thread",        16,      800,   0     },

#ifdef CONFIG_WIFI_ENABLE
    {"tasklet",             15,     WIFI_TASKLET_STK_SIZE,   0,      wifi_tasklet_tcb_stk_q  },
    {"RtmpMlmeTask",        17,     WIFI_MLME_STK_SIZE,      0,      wifi_mlme_tcb_stk_q     },
    {"RtmpCmdQTask",        17,     WIFI_CMDQ_STK_SIZE,      0,      wifi_cmdq_tcb_stk_q     },
    {"wl_rx_irq_thread",    16,     WIFI_RX_STK_SIZE,        0,      wifi_rx_tcb_stk_q       },
#endif

#ifdef CONFIG_BT_ENABLE
#if CPU_CORE_NUM > 1
    {"#C0btencry",          14,      512,   128   },
    {"#C0btctrler",         19,      512,   384   },
    {"#C0btstack",          18,     1024,   384   },
#else
    {"btencry",             14,      512,   128   },
    {"btctrler",            19,      512,   384   },
    {"btstack",             18,      768,   384   },
#endif
#endif

    {"aiui_net_task",       11,     1600,   256   },
    {0, 0},
};

void app_default_event_handler(struct sys_event *event)
{
    switch (event->type) {
    case SYS_KEY_EVENT:
    case SYS_TOUCH_EVENT:
    case SYS_DEVICE_EVENT:
    case SYS_NET_EVENT:
    case SYS_BT_EVENT:
        break;
    default:
        ASSERT(0, "unknow event type: %s\n", __func__);
        break;
    }
}

char *app_user_product_name(void)
{
    return "QYAIChatBot";
}

char *app_user_product_uuid(void)
{
    return NULL;
}

int app_user_product_batch(void)
{
    return 1;
}

int app_user_product_batch_num(void)
{
    return 1;
}

static void qyai_network_init(void)
{
    int net_ch = sys_net_channel_read();
    int net_mode = sys_net_mode_read();

    if (net_mode < 0) {
        sys_net_mode_write(NET_MODE_MANUAL_SET);
    }

    if (net_ch < 0) {
        sys_net_channel_write(NET_CH_SELECT_WIFI);
    }

#ifdef CONFIG_WIFI_ENABLE
    wireless_net_init();
#endif
}

static void qyai_local_init(void)
{
#if (defined TCFG_LED_STATUES_PORT && TCFG_LED_STATUES_PORT != -1)
    led_init();
#endif
}

static void qyai_voice_init(void)
{
    audio_app_mode_switch(NULL);
}

void app_main()
{
    puts("------------- qyai chatbot minimal app main -------------\n");

    while (!flash_protect_check_match()) {
        os_time_dly(50);
    }

#if (defined PRODUCTION_TEST_ENABLE || defined PRODUCTION_ALL_TEST_ENABLE)
    if (production_io_is_enter() &&
        system_reset_reason_get() != SYS_RST_ALM_WKUP &&
        system_reset_reason_get() != SYS_RST_PORT_WKUP &&
        system_reset_reason_get() != SYS_RST_LONG_PRESS) {
        product_test_task_init(2);
        return;
    }
#endif

#if (defined TCFG_VBAT_CHECK_EN && TCFG_VBAT_CHECK_EN == 1)
#if (!defined TCFG_SOFT_POWER_OFF_ALL_ELECTRY || TCFG_SOFT_POWER_OFF_ALL_ELECTRY == 0)
    if (
#if (defined TCFG_FIRST_POWER_OFF_EN)
        (system_reset_reason_get() == SYS_RST_VDDIO_PWR_ON) ||
#endif
        (user_is_soft_power_off_read() &&
         system_reset_reason_get() != SYS_RST_ALM_WKUP &&
         system_reset_reason_get() != SYS_RST_PORT_WKUP &&
         system_reset_reason_get() != SYS_RST_LONG_PRESS)) {
        sys_power_poweroff();
    }
#endif

    int vbat_perc = vbat_check_percent();
    if (vbat_perc <= 20) {
        os_time_dly(10);
        vbat_perc = vbat_check_percent();
        if (vbat_perc <= 20) {
            os_time_dly(50);
            sys_power_poweroff();
        }
    }
#endif

    dac_mute_control(0, 0);

    qyai_network_init();
    qyai_local_init();
    qyai_voice_init();

    puts("------------- qyai chatbot minimal app main end -------------\n");
}
