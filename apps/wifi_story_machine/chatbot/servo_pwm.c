#include "asm/pwm.h"
#include "asm/gpio.h"
#include "device/device.h"
#include "system/includes.h"
#include "app_config.h"
// #include "board_7916A_develop_cfg.h"
#include "board_7911B_develop_cfg.h"

#ifdef TCFG_SERVO_ENABLE

#ifndef SERVO_PWM_PORT
#define SERVO_PWM_PORT IO_PORTC_08
#endif

#ifndef SERVO_PWM_CH
#define SERVO_PWM_CH PWMCH2_L
#endif

static void *servo_dev = NULL;
static struct pwm_platform_data pwm = {0};

static float servo_duty_normalize(float duty, unsigned char point_bit)
{
    while (point_bit && duty > 100.0f) {
        duty /= 10.0f;
        point_bit--;
    }
    return duty;
}

int servo_init(void)
{
    if (servo_dev) {
        return 0;
    }

    servo_dev = dev_open("servo_pwm", &pwm);
    if (!servo_dev) {
        printf("[pwm] init fail\n");
        return -1;
    }

    pwm.duty = servo_duty_normalize(pwm.duty, pwm.point_bit);

    printf("[pwm] init ok, ch=0x%x freq=%d duty=%.2f%% pbit=%d\n",
           pwm.pwm_ch, pwm.freq, pwm.duty, pwm.point_bit);
    return 0;
}

int servo_set_pulse_us(unsigned short pulse_us)
{
    int ret;

    if (!servo_dev) {
        return -1;
    }

    if (pulse_us < 1000) {
        pulse_us = 1000;
    } else if (pulse_us > 2000) {
        pulse_us = 2000;
    }

    pwm.port = SERVO_PWM_PORT;
    pwm.pwm_ch = SERVO_PWM_CH;
    pwm.freq = 50;
    pwm.point_bit = 2;

    // 20ms 周期下：
    // duty = pulse_us / 20000 * 100 * 10^2
    // 所以 500us->250, 1500us->750, 2500us->1250
    pwm.duty = pulse_us / 200.0f;

    ret = dev_ioctl(servo_dev, PWM_SET_FREQ, (u32)&pwm);
    if (ret) {
        printf("[pwm] set freq fail: %d\n", ret);
        return ret;
    }

    ret = dev_write(servo_dev, (void *)&pwm, 0);
    if (ret) {
        printf("[pwm] write fail: %d\n", ret);
        return ret;
    }

    ret = dev_ioctl(servo_dev, PWM_RUN, (u32)&pwm);
    if (ret) {
        printf("[pwm] run fail: %d\n", ret);
    }
    printf("[pwm] set pulse %dus, duty=%.2f%% ret=%d\n", pulse_us, pwm.duty, ret);
    return ret;
}

int servo_set_angle(unsigned char angle)
{
    unsigned short pulse_us;

    if (angle > 180) {
        angle = 180;
    }

    // 先用保守范围 1000us~2000us
    pulse_us = 1000 + angle * 1000 / 180;
    return servo_set_pulse_us(pulse_us);
}

#ifdef SERVO_PWM_TEST
static void pwm_test(void)
{
    if (servo_init()) {
        printf("[pwm_test] servo init fail\n");
        return;
    }

    printf("[pwm_test] start on port=%d ch=0x%x\n", SERVO_PWM_PORT, SERVO_PWM_CH);

    while (1) {
        servo_set_pulse_us(1000);
        printf("[pwm_test] angle=0 pulse=1000us duty=%.2f%%\n", pwm.duty);
        os_time_dly(100);

        printf("[pwm_test] angle=90 pulse=1500us duty=%.2f%%\n", pwm.duty);
        servo_set_pulse_us(1500);
        os_time_dly(100);

        printf("[pwm_test] angle=180 pulse=2000us duty=%.2f%%\n", pwm.duty);
        servo_set_pulse_us(2000);
        os_time_dly(100);
    }
}
static int c_main(void)
{
    os_task_create(pwm_test, NULL, 12, 1000, 0, "pwm_test");
    return 0;
}
late_initcall(c_main);
#endif
#endif
