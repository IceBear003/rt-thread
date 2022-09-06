/*
 * Copyright (c) 2022 hpmicro
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "board.h"
#include "hpm_uart_drv.h"
#include "hpm_gptmr_drv.h"
#include "hpm_lcdc_drv.h"
#include "hpm_i2c_drv.h"
#include "hpm_gpio_drv.h"
#include "hpm_debug_console.h"
#include "hpm_dram_drv.h"
#include "pinmux.h"
#include "hpm_pmp_drv.h"
#include "assert.h"
#include "hpm_clock_drv.h"
#include "hpm_sysctl_drv.h"
#include "hpm_sdxc_drv.h"
#include "hpm_pwm_drv.h"
#include "hpm_trgm_drv.h"
#include "hpm_pllctlv2_drv.h"
#include "hpm_pcfg_drv.h"

static board_timer_cb timer_cb;

/**
 * @brief FLASH configuration option definitions:
 * option[0]:
 *    [31:16] 0xfcf9 - FLASH configuration option tag
 *    [15:4]  0 - Reserved
 *    [3:0]   option words (exclude option[0])
 * option[1]:
 *    [31:28] Flash probe type
 *      0 - SFDP SDR / 1 - SFDP DDR
 *      2 - 1-4-4 Read (0xEB, 24-bit address) / 3 - 1-2-2 Read(0xBB, 24-bit address)
 *      4 - HyperFLASH 1.8V / 5 - HyperFLASH 3V
 *      6 - OctaBus DDR (SPI -> OPI DDR)
 *      8 - Xccela DDR (SPI -> OPI DDR)
 *      10 - EcoXiP DDR (SPI -> OPI DDR)
 *    [27:24] Command Pads after Power-on Reset
 *      0 - SPI / 1 - DPI / 2 - QPI / 3 - OPI
 *    [23:20] Command Pads after Configuring FLASH
 *      0 - SPI / 1 - DPI / 2 - QPI / 3 - OPI
 *    [19:16] Quad Enable Sequence (for the device support SFDP 1.0 only)
 *      0 - Not needed
 *      1 - QE bit is at bit 6 in Status Register 1
 *      2 - QE bit is at bit1 in Status Register 2
 *      3 - QE bit is at bit7 in Status Register 2
 *      4 - QE bit is at bit1 in Status Register 2 and should be programmed by 0x31
 *    [15:8] Dummy cycles
 *      0 - Auto-probed / detected / default value
 *      Others - User specified value, for DDR read, the dummy cycles should be 2 * cycles on FLASH datasheet
 *    [7:4] Misc.
 *      0 - Not used
 *      1 - SPI mode
 *      2 - Internal loopback
 *      3 - External DQS
 *    [3:0] Frequency option
 *      1 - 30MHz / 2 - 50MHz / 3 - 66MHz / 4 - 80MHz / 5 - 100MHz / 6 - 120MHz / 7 - 133MHz / 8 - 166MHz
 *
 * option[2] (Effective only if the bit[3:0] in option[0] > 1)
 *    [31:20]  Reserved
 *    [19:16] IO voltage
 *      0 - 3V / 1 - 1.8V
 *    [15:12] Pin group
 *      0 - 1st group / 1 - 2nd group
 *    [11:8] Connection selection
 *      0 - CA_CS0 / 1 - CB_CS0 / 2 - CA_CS0 + CB_CS0 (Two FLASH connected to CA and CB respectively)
 *    [7:0] Drive Strength
 *      0 - Default value
 * option[3] (Effective only if the bit[3:0] in option[0] > 2, required only for the QSPI NOR FLASH that not supports
 *              JESD216)
 *    [31:16] reserved
 *    [15:12] Sector Erase Command Option, not required here
 *    [11:8]  Sector Size Option, not required here
 *    [7:0] Flash Size Option
 *      0 - 4MB / 1 - 8MB / 2 - 16MB
 */
#if defined(FLASH_XIP) && FLASH_XIP
__attribute__ ((section(".nor_cfg_option"))) const uint32_t option[4] = {0xfcf90001, 0x00000007, 0x0, 0x0};
#endif

#if defined(FLASH_UF2) && FLASH_UF2
ATTR_PLACE_AT(".uf2_signature") const uint32_t uf2_signature = BOARD_UF2_SIGNATURE;
#endif

void board_init_console(void)
{
#if console_type_uart == BOARD_CONSOLE_TYPE
    console_config_t cfg;

    /* Configure the UART clock to 24MHz */
    clock_set_source_divider(BOARD_CONSOLE_CLK_NAME, clk_src_osc24m, 1U);

    cfg.type = BOARD_CONSOLE_TYPE;
    cfg.base = (uint32_t) BOARD_CONSOLE_BASE;
    cfg.src_freq_in_hz = clock_get_frequency(BOARD_CONSOLE_CLK_NAME);
    cfg.baudrate = BOARD_CONSOLE_BAUDRATE;

    init_uart_pins((UART_Type *) cfg.base);

    if (status_success != console_init(&cfg)) {
        /* failed to  initialize debug console */
        while (1) {
        }
    }
#else
    while (1) {
    }
#endif
}

void board_print_clock_freq(void)
{
    printf("==============================\n");
    printf(" %s clock summary\n", BOARD_NAME);
    printf("==============================\n");
    printf("cpu0:\t\t %luHz\n", clock_get_frequency(clock_cpu0));
    printf("axi:\t\t %luHz\n", clock_get_frequency(clock_axi));
    printf("ahb:\t\t %luHz\n", clock_get_frequency(clock_ahb));
    printf("mchtmr0:\t %luHz\n", clock_get_frequency(clock_mchtmr0));
    printf("xpi0:\t\t %luHz\n", clock_get_frequency(clock_xpi0));
    printf("xpi1:\t\t %luHz\n", clock_get_frequency(clock_xpi1));
    printf("dram:\t\t %luHz\n", clock_get_frequency(clock_dram));
    printf("==============================\n");
}

void board_init_uart(UART_Type *ptr)
{
    init_uart_pins(ptr);
}

void board_init_ahb(void)
{
    clock_set_source_divider(clock_ahb, clk_src_pll1_clk1, 2);/*200m hz*/
}

void board_print_banner(void)
{
    const uint8_t banner[] = {"\n\
----------------------------------------------------------------------\n\
$$\\   $$\\ $$$$$$$\\  $$\\      $$\\ $$\\\n\
$$ |  $$ |$$  __$$\\ $$$\\    $$$ |\\__|\n\
$$ |  $$ |$$ |  $$ |$$$$\\  $$$$ |$$\\  $$$$$$$\\  $$$$$$\\   $$$$$$\\\n\
$$$$$$$$ |$$$$$$$  |$$\\$$\\$$ $$ |$$ |$$  _____|$$  __$$\\ $$  __$$\\\n\
$$  __$$ |$$  ____/ $$ \\$$$  $$ |$$ |$$ /      $$ |  \\__|$$ /  $$ |\n\
$$ |  $$ |$$ |      $$ |\\$  /$$ |$$ |$$ |      $$ |      $$ |  $$ |\n\
$$ |  $$ |$$ |      $$ | \\_/ $$ |$$ |\\$$$$$$$\\ $$ |      \\$$$$$$  |\n\
\\__|  \\__|\\__|      \\__|     \\__|\\__| \\_______|\\__|       \\______/\n\
----------------------------------------------------------------------\n"};
    printf("%s", banner);
}

void board_ungate_mchtmr_at_lp_mode(void)
{
    /* Keep cpu clock on wfi, so that mchtmr irq can still work after wfi */
    sysctl_set_cpu_lp_mode(HPM_SYSCTL, BOARD_RUNNING_CORE, cpu_lp_mode_ungate_cpu_clock);
}

void board_init(void)
{
    pcfg_dcdc_set_voltage(HPM_PCFG, 1100);
    board_init_clock();
    board_init_console();
    board_init_pmp();
    board_init_ahb();
#if BOARD_SHOW_CLOCK
    board_print_clock_freq();
#endif
#if BOARD_SHOW_BANNER
    board_print_banner();
#endif
}

void board_init_sdram_pins(void)
{
    init_sdram_pins();
}

uint32_t board_init_dram_clock(void)
{
    clock_add_to_group(clock_dram, 0);
    /* Configure the SDRAM to 133MHz */
    clock_set_source_divider(clock_dram, clk_src_pll0_clk1, 2U);

    return clock_get_frequency(clock_dram);
}

void board_delay_us(uint32_t us)
{
    clock_cpu_delay_us(us);
}

void board_delay_ms(uint32_t ms)
{
    clock_cpu_delay_ms(ms);
}

void board_timer_isr(void)
{
    if (gptmr_check_status(BOARD_CALLBACK_TIMER, GPTMR_CH_RLD_STAT_MASK(BOARD_CALLBACK_TIMER_CH))) {
        gptmr_clear_status(BOARD_CALLBACK_TIMER, GPTMR_CH_RLD_STAT_MASK(BOARD_CALLBACK_TIMER_CH));
        timer_cb();
    }
}
SDK_DECLARE_EXT_ISR_M(BOARD_CALLBACK_TIMER_IRQ, board_timer_isr);

void board_timer_create(uint32_t ms, board_timer_cb cb)
{
    uint32_t gptmr_freq;
    gptmr_channel_config_t config;

    timer_cb = cb;
    gptmr_channel_get_default_config(BOARD_CALLBACK_TIMER, &config);

    clock_add_to_group(BOARD_CALLBACK_TIMER_CLK_NAME, 0);
    gptmr_freq = clock_get_frequency(BOARD_CALLBACK_TIMER_CLK_NAME);

    config.reload = gptmr_freq / 1000 * ms;
    gptmr_channel_config(BOARD_CALLBACK_TIMER, BOARD_CALLBACK_TIMER_CH, &config, false);
    gptmr_enable_irq(BOARD_CALLBACK_TIMER, GPTMR_CH_RLD_IRQ_MASK(BOARD_CALLBACK_TIMER_CH));
    intc_m_enable_irq_with_priority(BOARD_CALLBACK_TIMER_IRQ, 1);

    gptmr_start_counter(BOARD_CALLBACK_TIMER, BOARD_CALLBACK_TIMER_CH);
}

void board_i2c_bus_clear(I2C_Type *ptr)
{
    init_i2c_pins_as_gpio(ptr);
}

void board_init_i2c(I2C_Type *ptr)
{
}

uint32_t board_init_spi_clock(SPI_Type *ptr)
{
    if (ptr == HPM_SPI3) {
        /* SPI3 clock configure */
        clock_add_to_group(clock_spi3, 0);
        clock_set_source_divider(clock_spi3, clk_src_osc24m, 1U);

        return clock_get_frequency(clock_spi3);
    }
    return 0;
}

void board_init_gpio_pins(void)
{
    init_gpio_pins();
}

void board_init_spi_pins(SPI_Type *ptr)
{
    init_spi_pins(ptr);
}

void board_init_spi_pins_with_gpio_as_cs(SPI_Type *ptr)
{
    init_spi_pins_with_gpio_as_cs(ptr);
    gpio_set_pin_output_with_initial(BOARD_SPI_CS_GPIO_CTRL, GPIO_GET_PORT_INDEX(BOARD_SPI_CS_PIN),
                                    GPIO_GET_PIN_INDEX(BOARD_SPI_CS_PIN), !BOARD_SPI_CS_ACTIVE_LEVEL);
}

void board_write_spi_cs(uint32_t pin, uint8_t state)
{
    gpio_write_pin(BOARD_SPI_CS_GPIO_CTRL, GPIO_GET_PORT_INDEX(pin), GPIO_GET_PIN_INDEX(pin), state);
}

void board_init_led_pins(void)
{
    init_led_pins();
    gpio_set_pin_output(BOARD_LED_GPIO_CTRL, BOARD_LED_GPIO_INDEX, BOARD_LED_GPIO_PIN);
}

void board_led_toggle(void)
{
    gpio_toggle_pin(BOARD_LED_GPIO_CTRL, BOARD_LED_GPIO_INDEX, BOARD_LED_GPIO_PIN);
}

void board_led_write(uint8_t state)
{
    gpio_write_pin(BOARD_LED_GPIO_CTRL, BOARD_LED_GPIO_INDEX, BOARD_LED_GPIO_PIN, state);
}

void board_init_usb_pins(void)
{
    /* set pull-up for USBx ID pin */
    init_usb_pins();

    /* configure USBx ID pin as input function */
    gpio_set_pin_input(BOARD_USB0_ID_PORT, BOARD_USB0_ID_GPIO_INDEX, BOARD_USB0_ID_GPIO_PIN);
}

uint8_t board_get_usb_id_status(void)
{
    return gpio_read_pin(BOARD_USB0_ID_PORT, BOARD_USB0_ID_GPIO_INDEX, BOARD_USB0_ID_GPIO_PIN);
}

void board_usb_vbus_ctrl(uint8_t usb_index, uint8_t level)
{
}

void board_init_pmp(void)
{
    extern uint32_t __noncacheable_start__[];
    extern uint32_t __noncacheable_end__[];

    uint32_t start_addr = (uint32_t) __noncacheable_start__;
    uint32_t end_addr = (uint32_t) __noncacheable_end__;
    uint32_t length = end_addr - start_addr;

    if (length == 0) {
        return;
    }

    /* Ensure the address and the length are power of 2 aligned */
    assert((length & (length - 1U)) == 0U);
    assert((start_addr & (length - 1U)) == 0U);

    pmp_entry_t pmp_entry[3] = {0};
    pmp_entry[0].pmp_addr = PMP_NAPOT_ADDR(0x0000000, 0x80000000);
    pmp_entry[0].pmp_cfg.val = PMP_CFG(READ_EN, WRITE_EN, EXECUTE_EN, ADDR_MATCH_NAPOT, REG_UNLOCK);


    pmp_entry[1].pmp_addr = PMP_NAPOT_ADDR(0x80000000, 0x80000000);
    pmp_entry[1].pmp_cfg.val = PMP_CFG(READ_EN, WRITE_EN, EXECUTE_EN, ADDR_MATCH_NAPOT, REG_UNLOCK);

    pmp_entry[2].pmp_addr = PMP_NAPOT_ADDR(start_addr, length);
    pmp_entry[2].pmp_cfg.val = PMP_CFG(READ_EN, WRITE_EN, EXECUTE_EN, ADDR_MATCH_NAPOT, REG_UNLOCK);
    pmp_entry[2].pma_addr = PMA_NAPOT_ADDR(start_addr, length);
    pmp_entry[2].pma_cfg.val = PMA_CFG(ADDR_MATCH_NAPOT, MEM_TYPE_MEM_NON_CACHE_BUF, AMO_EN);
    pmp_config(&pmp_entry[0], ARRAY_SIZE(pmp_entry));
}

void board_init_clock(void)
{
    uint32_t cpu0_freq = clock_get_frequency(clock_cpu0);
    if (cpu0_freq == PLLCTL_SOC_PLL_REFCLK_FREQ) {
        /* Configure the External OSC ramp-up time: ~9ms */
        pllctlv2_xtal_set_rampup_time(HPM_PLLCTLV2, 32UL * 1000UL * 9U);

        /* Select clock setting preset1 */
        sysctl_clock_set_preset(HPM_SYSCTL, 2);
    }
    /* Add most Clocks to group 0 */
    clock_add_to_group(clock_cpu0, 0);
    clock_add_to_group(clock_ahbp, 0);
    clock_add_to_group(clock_axic, 0);
    clock_add_to_group(clock_axis, 0);

    clock_add_to_group(clock_mchtmr0, 0);
    clock_add_to_group(clock_dram, 0);
    clock_add_to_group(clock_xpi0, 0);
    clock_add_to_group(clock_xpi1, 0);
    clock_add_to_group(clock_gptmr0, 0);
    clock_add_to_group(clock_gptmr1, 0);
    clock_add_to_group(clock_gptmr2, 0);
    clock_add_to_group(clock_gptmr3, 0);
    clock_add_to_group(clock_uart0, 0);
    clock_add_to_group(clock_uart1, 0);
    clock_add_to_group(clock_uart2, 0);
    clock_add_to_group(clock_uart3, 0);
    clock_add_to_group(clock_i2c0, 0);
    clock_add_to_group(clock_i2c1, 0);
    clock_add_to_group(clock_i2c2, 0);
    clock_add_to_group(clock_i2c3, 0);
    clock_add_to_group(clock_spi0, 0);
    clock_add_to_group(clock_spi1, 0);
    clock_add_to_group(clock_spi2, 0);
    clock_add_to_group(clock_spi3, 0);
    clock_add_to_group(clock_can0, 0);
    clock_add_to_group(clock_can1, 0);
    clock_add_to_group(clock_sdxc0, 0);
    clock_add_to_group(clock_ptpc, 0);
    clock_add_to_group(clock_ref0, 0);
    clock_add_to_group(clock_ref1, 0);
    clock_add_to_group(clock_watchdog0, 0);
    clock_add_to_group(clock_eth0, 0);
    clock_add_to_group(clock_sdp, 0);
    clock_add_to_group(clock_xdma, 0);
    clock_add_to_group(clock_ram0, 0);
    clock_add_to_group(clock_usb0, 0);
    clock_add_to_group(clock_kman, 0);
    clock_add_to_group(clock_gpio, 0);
    clock_add_to_group(clock_mbx0, 0);
    clock_add_to_group(clock_hdma, 0);
    clock_add_to_group(clock_rng, 0);
    clock_add_to_group(clock_mot0, 0);
    clock_add_to_group(clock_mot1, 0);
    clock_add_to_group(clock_acmp, 0);
    clock_add_to_group(clock_dao, 0);
    clock_add_to_group(clock_msyn, 0);
    clock_add_to_group(clock_lmm0, 0);
    clock_add_to_group(clock_pdm, 0);

    clock_add_to_group(clock_adc0, 0);
    clock_add_to_group(clock_adc1, 0);
    clock_add_to_group(clock_adc2, 0);

    clock_add_to_group(clock_dac0, 0);

    clock_add_to_group(clock_i2s0, 0);
    clock_add_to_group(clock_i2s1, 0);

    clock_add_to_group(clock_ffa0, 0);
    clock_add_to_group(clock_tsns, 0);

    /* Connect Group0 to CPU0 */
    clock_connect_group_to_cpu(0, 0);
    /* Configure CPU0 to 480MHz */
    clock_set_source_divider(clock_cpu0, clk_src_pll1_clk0, 1);

    clock_update_core_clock();
}

uint32_t board_init_adc12_clock(ADC16_Type *ptr)
{
    uint32_t freq = 0;
    switch ((uint32_t) ptr) {
    case HPM_ADC0_BASE:
        /* Configure the ADC clock to 200MHz */
        clock_set_adc_source(clock_adc0, clk_adc_src_ana);
        clock_set_source_divider(clock_ana0, clk_src_pll1_clk1, 2U);
        freq = clock_get_frequency(clock_adc0);
        break;
    case HPM_ADC1_BASE:
        /* Configure the ADC clock to 200MHz */
        clock_set_adc_source(clock_adc1, clk_adc_src_ana);
        clock_set_source_divider(clock_ana0, clk_src_pll1_clk1, 2U);
        freq = clock_get_frequency(clock_adc1);
        break;
    case HPM_ADC2_BASE:
        /* Configure the ADC clock to 200MHz */
        clock_set_adc_source(clock_adc2, clk_adc_src_ana);
        clock_set_source_divider(clock_ana0, clk_src_pll1_clk1, 2U);
        freq = clock_get_frequency(clock_adc2);
        break;
    default:
        /* Invalid ADC instance */
        break;
    }

    return freq;
}

uint32_t board_init_dao_clock(void)
{
    return clock_get_frequency(clock_dao);
}

uint32_t board_init_pdm_clock(void)
{
    return clock_get_frequency(clock_pdm);
}

uint32_t board_init_i2s_clock(I2S_Type *ptr)
{
    return 0;
}

uint32_t board_init_adc16_clock(ADC16_Type *ptr)
{
    return 0;
}

uint32_t board_init_dac_clock(DAC_Type *ptr, bool clk_src_ahb)
{
    uint32_t freq = 0;

    if (ptr == HPM_DAC) {
        if (clk_src_ahb == true) {
            /* Configure the DAC clock to 133MHz */
            clock_set_dac_source(clock_dac0, clk_dac_src_ahb);
        } else {
            /* Configure the DAC clock to 166MHz */
            clock_set_dac_source(clock_dac0, clk_dac_src_ana);
            clock_set_source_divider(clock_ana3, clk_src_pll0_clk1, 2);
        }

        freq = clock_get_frequency(clock_dac0);
    }

    return freq;
}

void board_init_can(CAN_Type *ptr)
{
    init_can_pins(ptr);
}

uint32_t board_init_can_clock(CAN_Type *ptr)
{
    uint32_t freq = 0;
    if (ptr == HPM_CAN0) {
        /* Set the CAN0 peripheral clock to 80MHz */
        clock_set_source_divider(clock_can0, clk_src_pll0_clk0, 5);
        freq = clock_get_frequency(clock_can0);
    } else if (ptr == HPM_CAN1) {
        /* Set the CAN1 peripheral clock to 80MHz */
        clock_set_source_divider(clock_can1, clk_src_pll0_clk0, 5);
        freq = clock_get_frequency(clock_can1);
    } else {
        /* Invalid CAN instance */
    }
    return freq;
}

#ifdef INIT_EXT_RAM_FOR_DATA
/*
 * this function will be called during startup to initialize external memory for data use
 */
void _init_ext_ram(void)
{
    uint32_t dram_clk_in_hz;
    board_init_sdram_pins();
    dram_clk_in_hz = board_init_dram_clock();

    dram_config_t config = {0};
    dram_sdram_config_t sdram_config = {0};

    dram_default_config(HPM_DRAM, &config);
    config.dqs = DRAM_DQS_INTERNAL;
    dram_init(HPM_DRAM, &config);

    sdram_config.bank_num = DRAM_SDRAM_BANK_NUM_4;
    sdram_config.prescaler = 0x3;
    sdram_config.burst_len_in_byte = 8;
    sdram_config.auto_refresh_count_in_one_burst = 1;
    sdram_config.col_addr_bits = DRAM_SDRAM_COLUMN_ADDR_9_BITS;
    sdram_config.cas_latency = DRAM_SDRAM_CAS_LATENCY_3;

    sdram_config.precharge_to_act_in_ns = 18;   /* Trp */
    sdram_config.act_to_rw_in_ns = 18;          /* Trcd */
    sdram_config.refresh_recover_in_ns = 70;     /* Trfc/Trc */
    sdram_config.write_recover_in_ns = 12;      /* Twr/Tdpl */
    sdram_config.cke_off_in_ns = 42;             /* Trcd */
    sdram_config.act_to_precharge_in_ns = 42;   /* Tras */

    sdram_config.self_refresh_recover_in_ns = 66;   /* Txsr */
    sdram_config.refresh_to_refresh_in_ns = 66;     /* Trfc/Trc */
    sdram_config.act_to_act_in_ns = 12;             /* Trrd */
    sdram_config.idle_timeout_in_ns = 6;
    sdram_config.cs_mux_pin = DRAM_IO_MUX_NOT_USED;

    sdram_config.cs = BOARD_SDRAM_CS;
    sdram_config.base_address = BOARD_SDRAM_ADDRESS;
    sdram_config.size_in_byte = BOARD_SDRAM_SIZE;
    sdram_config.port_size = BOARD_SDRAM_PORT_SIZE;
    sdram_config.refresh_count = BOARD_SDRAM_REFRESH_COUNT;
    sdram_config.refresh_in_ms = BOARD_SDRAM_REFRESH_IN_MS;
    sdram_config.data_width_in_byte = BOARD_SDRAM_DATA_WIDTH_IN_BYTE;
    sdram_config.delay_cell_value = 29;

    dram_config_sdram(HPM_DRAM, dram_clk_in_hz, &sdram_config);
}
#endif

void board_init_sd_pins(SDXC_Type *ptr)
{
    init_sdxc_pins(ptr, false);
}

uint32_t board_sd_configure_clock(SDXC_Type *ptr, uint32_t freq)
{
    uint32_t actual_freq = 0;
    do {
        if (ptr != HPM_SDXC0) {
            break;
        }
        clock_name_t sdxc_clk = clock_sdxc0;
        sdxc_enable_sd_clock(ptr, false);
        /* Configure the SDXC Frequency to 200MHz */
        clock_set_source_divider(sdxc_clk, clk_src_pll0_clk0, 2);
        sdxc_enable_freq_selection(ptr);

        /* Configure the clock below 400KHz for the identification state */
        if (freq <= 400000UL) {
            sdxc_set_clock_divider(ptr, 600);
        }
            /* configure the clock to 24MHz for the SDR12/Default speed */
        else if (freq <= 25000000UL) {
            sdxc_set_clock_divider(ptr, 8);
        }
            /* Configure the clock to 50MHz for the SDR25/High speed/50MHz DDR/50MHz SDR */
        else if (freq <= 50000000UL) {
            sdxc_set_clock_divider(ptr, 4);
        }
            /* Configure the clock to 100MHz for the SDR50 */
        else if (freq <= 100000000UL) {
            sdxc_set_clock_divider(ptr, 2);
        }
            /* Configure the clock to 166MHz for SDR104/HS200/HS400  */
        else if (freq <= 208000000UL) {
            sdxc_set_clock_divider(ptr, 1);
        }
            /* For other unsupported clock ranges, configure the clock to 24MHz */
        else {
            sdxc_set_clock_divider(ptr, 8);
        }
        sdxc_enable_sd_clock(ptr, true);
        actual_freq = clock_get_frequency(sdxc_clk) / sdxc_get_clock_divider(ptr);
    } while (false);

    return actual_freq;
}

void board_sd_switch_pins_to_1v8(SDXC_Type *ptr)
{
    /* This feature is not supported */
}

bool board_sd_detect_card(SDXC_Type *ptr)
{
    return sdxc_is_card_inserted(ptr);
}

hpm_stat_t board_init_enet_ptp_clock(ENET_Type *ptr)
{
    /* set clock source */
    if (ptr == HPM_ENET0) {
        /* make sure pll0_clk0 output clock at 400MHz to get a clock at 100MHz for ent0 ptp clock */
        clock_set_source_divider(clock_ptp0, clk_src_pll0_clk0, 4); /* 100MHz */
    } else {
        return status_invalid_argument;
    }

    return status_success;
}

hpm_stat_t board_init_enet_rmii_reference_clock(ENET_Type *ptr, bool internal)
{
    if (internal == false) {
        return status_success;
    }

    /* Configure Enet clock to output reference clock */
    if (ptr == HPM_ENET0) {
        /* make sure pll0_clk2 output clock at 250MHz then set 50MHz for enet0 */
        clock_set_source_divider(clock_eth0, clk_src_pll0_clk2, 5);
    } else {
        return status_invalid_argument;
    }
    return status_success;
}

void board_init_adc16_pins(void)
{
    init_adc_pins();
}

hpm_stat_t board_init_enet_pins(ENET_Type *ptr)
{
    init_enet_pins(ptr);

    return status_success;
}

void board_init_dac_pins(DAC_Type *ptr)
{
   init_dac_pins(ptr);
}

uint32_t board_init_uart_clock(UART_Type *ptr)
{
    uint32_t freq = 0U;
    if (ptr == HPM_UART0) {
        clock_set_source_divider(clock_uart0, clk_src_osc24m, 1);
        freq = clock_get_frequency(clock_uart0);
    } else if (ptr == HPM_UART1) {
        clock_set_source_divider(clock_uart1, clk_src_osc24m, 1);
        freq = clock_get_frequency(clock_uart1);
    } else if (ptr == HPM_UART2) {
        clock_set_source_divider(clock_uart2, clk_src_osc24m, 1);
        freq = clock_get_frequency(clock_uart2);
    } else {
        /* Not supported */
    }
    return freq;
}
