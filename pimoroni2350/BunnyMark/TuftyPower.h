// The Tufty 2350's power management, the way Pimoroni's Badgeware firmware does it (as the
// znax_tufty2350 port has it). RESET resets the chip, so
// tuftyPowerStartup runs at every start, before the display:
//  - every pin set up, and GPIO 41 high: it switches the RTC's power, and with it the rail the
//    display is on (without it the display stays black)
//  - after a reset by the button, RESET still held: the rear LEDs sweep on and fade out. Held until
//    they are dark the Tufty goes to sleep, a front button or the RTC wakes it up again. With UP and
//    DOWN held as well it goes into shipping mode instead, where only RESET or USB power bring it
//    back. Let go earlier BunnyMark just starts
//  - after a reset by the button the double tap flag is set for a second, so a second tap of RESET
//    within it is seen as a double tap
//  - the RTC's timer interrupt is switched off, it would otherwise keep waking the Tufty
// Only the Tufty build includes this.

#pragma once
#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include <hardware/clocks.h>
// arduino-pico builds hardware_powman into its libpico but leaves its folder off the include path,
// it is found from the hardware_gpio include folder that is on it
#include <../../hardware_powman/include/hardware/powman.h>
#include <hardware/watchdog.h>
#include <hardware/i2c.h>
#include <hardware/resets.h>
#include <hardware/structs/usb.h>
#include <pico/stdlib.h>
#include <pico/time.h>

#define TUFTY_BUTTON_A_PIN 7
#define TUFTY_BUTTON_B_PIN 9
#define TUFTY_BUTTON_C_PIN 10
#define TUFTY_BUTTON_UP_PIN 11
#define TUFTY_BUTTON_DOWN_PIN 6
#define TUFTY_BUTTON_HOME_PIN 22
// the rear LEDs
#define TUFTY_LED_0_PIN 0
#define TUFTY_LED_1_PIN 1
#define TUFTY_LED_2_PIN 2
#define TUFTY_LED_3_PIN 3
// the RESET button's level, low while it is held, and the line all the front buttons pull low
#define TUFTY_RESET_BUTTON_PIN 14
#define TUFTY_BUTTONS_INT_PIN 15
#define TUFTY_VBUS_DETECT_PIN 12
#define TUFTY_RTC_ALARM_PIN 13
#define TUFTY_PSRAM_CS_PIN 8
#define TUFTY_RTC_I2C i2c0
#define TUFTY_RTC_I2C_ADDR 0x51
#define TUFTY_RTC_SDA_PIN 4
#define TUFTY_RTC_SCL_PIN 5
#define TUFTY_RTC_POWER_PIN 41

#define TUFTY_DOUBLE_TAP_MS 1000
// the long press LED sweep: peak brightness, the delay from one LED to the next and while fading
// out, how much faster they fade out, the gamma, the time per step and the steps before they start
#define TUFTY_LED_PEAK_BRIGHTNESS 150
#define TUFTY_LED_IN_PHASE 30
#define TUFTY_LED_OUT_PHASE 0
#define TUFTY_LED_TOTAL (TUFTY_LED_PEAK_BRIGHTNESS + TUFTY_LED_IN_PHASE * 3)
#define TUFTY_LED_FADEOUT_SPEED 5
#define TUFTY_LED_GAMMA 1.8f
#define TUFTY_LED_DELAY_MS 5
#define TUFTY_LED_ON_DELAY (200 / TUFTY_LED_DELAY_MS)

static const uint8_t tuftyLedPins[4] = { TUFTY_LED_1_PIN, TUFTY_LED_2_PIN, TUFTY_LED_3_PIN, TUFTY_LED_0_PIN };
static const uint32_t tuftyLedMask = (1u << TUFTY_LED_0_PIN) | (1u << TUFTY_LED_1_PIN) |
                                     (1u << TUFTY_LED_2_PIN) | (1u << TUFTY_LED_3_PIN);
static powman_power_state tuftyPowerOffState;
static powman_power_state tuftyPowerOnState;


static void tuftySetDoubleTapFlag(bool set)
{
    if (set)
        powman_set_bits(&powman_hw->chip_reset, POWMAN_CHIP_RESET_DOUBLE_TAP_BITS);
    else
        powman_clear_bits(&powman_hw->chip_reset, POWMAN_CHIP_RESET_DOUBLE_TAP_BITS);
}

static int64_t tuftyClearDoubleTapFlag(alarm_id_t id, void *user)
{
    (void)id;
    (void)user;
    tuftySetDoubleTapFlag(false);
    return 0;
}

// the RTC's I2C, its power comes up slowly
static void tuftyRtcI2CEnable(void)
{
    gpio_init(TUFTY_RTC_POWER_PIN);
    gpio_set_dir(TUFTY_RTC_POWER_PIN, GPIO_OUT);
    gpio_put(TUFTY_RTC_POWER_PIN, 1);
    sleep_ms(500);
    i2c_init(TUFTY_RTC_I2C, 400 * 1000);
    gpio_set_function(TUFTY_RTC_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(TUFTY_RTC_SCL_PIN, GPIO_FUNC_I2C);
}

static void tuftyRtcI2CDisable(void)
{
    gpio_init(TUFTY_RTC_POWER_PIN);
    gpio_init(TUFTY_RTC_SDA_PIN);
    gpio_init(TUFTY_RTC_SCL_PIN);
}

static void tuftyRtcDisableInterrupt(void)
{
    // Timer_mode: timer and its interrupt off
    const uint8_t data[2] = { 0x11, 0x00 };
    i2c_write_blocking(TUFTY_RTC_I2C, TUFTY_RTC_I2C_ADDR, data, 2, false);
}

// the front buttons pulled up, and with everything also HOME, the interrupt lines, the LEDs, the RESET
// level and the RTC's power
static void tuftyPowerSetupGpio(bool buttonsOnly)
{
    const uint32_t buttonMask = (1u << TUFTY_BUTTON_A_PIN) | (1u << TUFTY_BUTTON_B_PIN) |
                                (1u << TUFTY_BUTTON_C_PIN) | (1u << TUFTY_BUTTON_UP_PIN) |
                                (1u << TUFTY_BUTTON_DOWN_PIN);
    gpio_init_mask(buttonMask);
    gpio_set_dir_in_masked(buttonMask);
    gpio_set_pulls(TUFTY_BUTTON_A_PIN, true, false);
    gpio_set_pulls(TUFTY_BUTTON_B_PIN, true, false);
    gpio_set_pulls(TUFTY_BUTTON_C_PIN, true, false);
    gpio_set_pulls(TUFTY_BUTTON_UP_PIN, true, false);
    gpio_set_pulls(TUFTY_BUTTON_DOWN_PIN, true, false);
    if (buttonsOnly)
        return;

    gpio_init(TUFTY_BUTTON_HOME_PIN);
    gpio_set_dir(TUFTY_BUTTON_HOME_PIN, GPIO_IN);
    gpio_set_pulls(TUFTY_BUTTON_HOME_PIN, true, false);
    gpio_init(TUFTY_BUTTONS_INT_PIN);
    gpio_set_dir(TUFTY_BUTTONS_INT_PIN, GPIO_IN);
    gpio_set_pulls(TUFTY_BUTTONS_INT_PIN, true, false);
    gpio_init(TUFTY_RTC_ALARM_PIN);
    gpio_set_dir(TUFTY_RTC_ALARM_PIN, GPIO_IN);
    gpio_set_pulls(TUFTY_RTC_ALARM_PIN, true, false);
    gpio_init(TUFTY_VBUS_DETECT_PIN);
    gpio_set_dir(TUFTY_VBUS_DETECT_PIN, GPIO_IN);
    gpio_set_pulls(TUFTY_VBUS_DETECT_PIN, false, false);

    // the LEDs off
    gpio_init_mask(tuftyLedMask);
    gpio_set_dir_out_masked(tuftyLedMask);
    gpio_put_masked(tuftyLedMask, 0);

    gpio_init(TUFTY_RESET_BUTTON_PIN);
    gpio_set_dir(TUFTY_RESET_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(TUFTY_RESET_BUTTON_PIN);

    gpio_init(TUFTY_RTC_POWER_PIN);
    gpio_set_dir(TUFTY_RTC_POWER_PIN, GPIO_OUT);
    gpio_put(TUFTY_RTC_POWER_PIN, 1);
}

// everything that draws power set up for powering off: clocks from the USB PLL, every pin an input
// with a pull that keeps it quiet, the USB PHY off, the power manager's timer running and the power
// states to switch between
static void tuftyPowerPrepareOff(void)
{
    tuftySetDoubleTapFlag(false);
    set_sys_clock_48mhz();

    for (int i = 0; i < (int)NUM_BANK0_GPIOS; ++i)
    {
        gpio_set_function(i, GPIO_FUNC_SIO);
        gpio_set_dir(i, GPIO_IN);
        gpio_set_input_enabled(i, false);
        switch (i)
        {
            case TUFTY_PSRAM_CS_PIN:
                gpio_set_pulls(i, true, false);
                break;
            case TUFTY_RESET_BUTTON_PIN:
            case TUFTY_BUTTON_HOME_PIN:
            case 40:
            case TUFTY_RTC_POWER_PIN:
            case 42:
                gpio_disable_pulls(i);
                break;
            case TUFTY_BUTTON_A_PIN:
            case TUFTY_BUTTON_B_PIN:
            case TUFTY_BUTTON_C_PIN:
            case TUFTY_BUTTON_UP_PIN:
            case TUFTY_BUTTON_DOWN_PIN:
                // pulled up, a press has to reach the button interrupt line
                break;
            default:
                gpio_set_pulls(i, false, true);
                break;
        }
    }

    hw_set_bits(&powman_hw->vreg_ctrl, POWMAN_PASSWORD_BITS | POWMAN_VREG_CTRL_UNLOCK_BITS);

    reset_block_mask(RESETS_RESET_USBCTRL_BITS);
    unreset_block_mask_wait_blocking(RESETS_RESET_USBCTRL_BITS);
    usb_hw->muxing = USB_USB_MUXING_TO_PHY_BITS | USB_USB_MUXING_SOFTCON_BITS;
    usb_hw->main_ctrl = USB_MAIN_CTRL_CONTROLLER_EN_BITS;
    usb_hw->sie_ctrl = USB_SIE_CTRL_EP0_INT_1BUF_BITS;
    usb_hw->inte = USB_INTS_BUFF_STATUS_BITS | USB_INTS_BUS_RESET_BITS | USB_INTS_SETUP_REQ_BITS |
                   USB_INTS_DEV_SUSPEND_BITS | USB_INTS_DEV_RESUME_FROM_HOST_BITS | USB_INTS_DEV_CONN_DIS_BITS;
    usb_hw->phy_direct = USB_USBPHY_DIRECT_TX_PD_BITS | USB_USBPHY_DIRECT_RX_PD_BITS |
                         USB_USBPHY_DIRECT_DM_PULLDN_EN_BITS | USB_USBPHY_DIRECT_DP_PULLDN_EN_BITS;
    usb_hw->phy_direct_override = USB_USBPHY_DIRECT_RX_DM_BITS | USB_USBPHY_DIRECT_RX_DP_BITS | USB_USBPHY_DIRECT_RX_DD_BITS |
        USB_USBPHY_DIRECT_OVERRIDE_TX_DIFFMODE_OVERRIDE_EN_BITS | USB_USBPHY_DIRECT_OVERRIDE_DM_PULLUP_OVERRIDE_EN_BITS |
        USB_USBPHY_DIRECT_OVERRIDE_TX_FSSLEW_OVERRIDE_EN_BITS | USB_USBPHY_DIRECT_OVERRIDE_TX_PD_OVERRIDE_EN_BITS |
        USB_USBPHY_DIRECT_OVERRIDE_RX_PD_OVERRIDE_EN_BITS | USB_USBPHY_DIRECT_OVERRIDE_TX_DM_OVERRIDE_EN_BITS |
        USB_USBPHY_DIRECT_OVERRIDE_TX_DP_OVERRIDE_EN_BITS | USB_USBPHY_DIRECT_OVERRIDE_TX_DM_OE_OVERRIDE_EN_BITS |
        USB_USBPHY_DIRECT_OVERRIDE_TX_DP_OE_OVERRIDE_EN_BITS | USB_USBPHY_DIRECT_OVERRIDE_DM_PULLDN_EN_OVERRIDE_EN_BITS |
        USB_USBPHY_DIRECT_OVERRIDE_DP_PULLDN_EN_OVERRIDE_EN_BITS | USB_USBPHY_DIRECT_OVERRIDE_DP_PULLUP_EN_OVERRIDE_EN_BITS |
        USB_USBPHY_DIRECT_OVERRIDE_DM_PULLUP_HISEL_OVERRIDE_EN_BITS | USB_USBPHY_DIRECT_OVERRIDE_DP_PULLUP_HISEL_OVERRIDE_EN_BITS;

    // 2025/05/01 in milliseconds since the epoch
    powman_timer_start();
    powman_timer_set_ms(1746057600000ULL);
    powman_set_debug_power_request_ignored(true);

    // off: every domain down. On: the switched core and the XIP cache
    tuftyPowerOffState = POWMAN_POWER_STATE_NONE;
    tuftyPowerOnState = powman_power_state_with_domain_on(POWMAN_POWER_STATE_NONE, POWMAN_POWER_DOMAIN_SWITCHED_CORE);
    tuftyPowerOnState = powman_power_state_with_domain_on(tuftyPowerOnState, POWMAN_POWER_DOMAIN_XIP_CACHE);
}

// Powers off, a wake up starts the program from the beginning. In RAM: the flash goes off with the rest
static int __no_inline_not_in_flash_func(tuftyPowerOff)(void)
{
    if (!powman_configure_wakeup_state(tuftyPowerOffState, tuftyPowerOnState))
        return PICO_ERROR_INVALID_STATE;
    powman_hw->boot[0] = 0;
    powman_hw->boot[1] = 0;
    powman_hw->boot[2] = 0;
    powman_hw->boot[3] = 0;
    const int rc = powman_set_power_state(tuftyPowerOffState);
    if (rc != PICO_OK)
        return rc;
    while (true)
        __wfi();
}

// gpio waking the Tufty on channel, once it has been idle, for at most a second
static void tuftyPowerWakeOnGpio(int channel, int gpio, bool edge, bool high)
{
    gpio_init(gpio);
    gpio_set_dir(gpio, false);
    gpio_set_input_enabled(gpio, true);
    gpio_set_pulls(gpio, !high, high);
    const absolute_time_t timeout = make_timeout_time_ms(1000);
    while ((gpio_get(gpio) == high) && !time_reached(timeout))
        sleep_ms(10);
    powman_enable_gpio_wakeup(channel, gpio, edge, high);
}

// sleep until a front button or the RTC's alarm
static void tuftyPowerSleep(void)
{
    tuftyPowerPrepareOff();
    tuftyPowerSetupGpio(true);
    tuftyPowerWakeOnGpio(1, TUFTY_RTC_ALARM_PIN, true, false);
    tuftyPowerWakeOnGpio(3, TUFTY_BUTTONS_INT_PIN, true, false);
    tuftyRtcI2CDisable();
    tuftyPowerOff();
}

// off until RESET or USB power
static void tuftyPowerShip(void)
{
    tuftyPowerPrepareOff();
    tuftyRtcI2CDisable();
    tuftyPowerOff();
}

// RESET is still held after the reset: the LED sweep, and sleep or shipping mode when it is held
// until the LEDs are dark again
static void tuftyPowerLongPress(void)
{
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / 2048.0f);
    pwm_config_set_wrap(&config, 1024);
    for (uint8_t i = 0; i < 4; i++)
    {
        gpio_set_function(tuftyLedPins[i], GPIO_FUNC_PWM);
        pwm_init(pwm_gpio_to_slice_num(tuftyLedPins[i]), &config, true);
    }
    int step = 0;
    // let go early, BunnyMark starts
    while (!gpio_get(TUFTY_RESET_BUTTON_PIN))
    {
        const int sweep = (step < TUFTY_LED_ON_DELAY) ? 0 : step - TUFTY_LED_ON_DELAY;
        int brightness = (sweep >= TUFTY_LED_TOTAL)
                             ? TUFTY_LED_TOTAL - (sweep - TUFTY_LED_TOTAL) * TUFTY_LED_FADEOUT_SPEED : sweep;
        const int phase = (sweep >= TUFTY_LED_TOTAL) ? TUFTY_LED_OUT_PHASE : TUFTY_LED_IN_PHASE;
        int level = 0;
        for (uint8_t i = 0; i < 4; i++)
        {
            const int v = (brightness < 0) ? 0
                        : ((brightness > TUFTY_LED_PEAK_BRIGHTNESS) ? TUFTY_LED_PEAK_BRIGHTNESS : brightness);
            pwm_set_gpio_level(tuftyLedPins[i], (uint16_t)(v * TUFTY_LED_GAMMA));
            level += (int)(v * TUFTY_LED_GAMMA);
            brightness -= phase;
        }
        if ((sweep > 0) && (level == 0))
        {
            tuftySetDoubleTapFlag(false);
            if (!gpio_get(TUFTY_BUTTON_UP_PIN) && !gpio_get(TUFTY_BUTTON_DOWN_PIN))
                tuftyPowerShip();
            else
                tuftyPowerSleep();
            break;
        }
        step++;
        sleep_ms(TUFTY_LED_DELAY_MS);
    }
    for (uint8_t i = 0; i < 4; i++)
        pwm_set_enabled(pwm_gpio_to_slice_num(tuftyLedPins[i]), false);
    gpio_init_mask(tuftyLedMask);
    gpio_set_dir_out_masked(tuftyLedMask);
    gpio_put_masked(tuftyLedMask, 0);
}

static void tuftyPowerStartup(void)
{
    tuftyPowerSetupGpio(false);
    // a reset by the button, not a power on or the watchdog of an upload
    if ((powman_hw->chip_reset & POWMAN_CHIP_RESET_HAD_RUN_LOW_BITS) && !watchdog_caused_reboot())
    {
        if (!(powman_hw->chip_reset & POWMAN_CHIP_RESET_DOUBLE_TAP_BITS))
        {
            tuftySetDoubleTapFlag(true);
            add_alarm_in_ms(TUFTY_DOUBLE_TAP_MS, tuftyClearDoubleTapFlag, nullptr, false);
            if (!gpio_get(TUFTY_RESET_BUTTON_PIN))
                tuftyPowerLongPress();
        }
        else
            tuftySetDoubleTapFlag(false);
    }
    tuftyRtcI2CEnable();
    tuftyRtcDisableInterrupt();
}
