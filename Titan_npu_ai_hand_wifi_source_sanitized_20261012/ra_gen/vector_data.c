/* generated vector source file - do not edit */
        #include "bsp_api.h"
        /* Do not build these data structures if no interrupts are currently allocated because IAR will have build errors. */
        #if VECTOR_DATA_IRQ_COUNT > 0
        BSP_DONT_REMOVE const fsp_vector_t g_vector_table[BSP_ICU_VECTOR_NUM_ENTRIES] BSP_PLACE_IN_SECTION(BSP_SECTION_APPLICATION_VECTORS) =
        {
                        [0] = ceu_isr, /* CEU CEUI (CEU interrupt) */
            [1] = sci_b_uart_rxi_isr, /* SCI8 RXI (Receive data full) */
            [2] = sci_b_uart_txi_isr, /* SCI8 TXI (Transmit data empty) */
            [3] = sci_b_uart_tei_isr, /* SCI8 TEI (Transmit end) */
            [4] = sci_b_uart_eri_isr, /* SCI8 ERI (Receive error) */
            [5] = glcdc_line_detect_isr, /* GLCDC LINE DETECT (Specified line) */
            [6] = drw_int_isr, /* DRW INT (DRW interrupt) */
            [7] = rm_ethosu_isr, /* NPU IRQ (NPU IRQ) */
            [8] = sci_b_uart_rxi_isr, /* SCI5 RXI (Receive data full) */
            [9] = sci_b_uart_txi_isr, /* SCI5 TXI (Transmit data empty) */
            [10] = sci_b_uart_tei_isr, /* SCI5 TEI (Transmit end) */
            [11] = sci_b_uart_eri_isr, /* SCI5 ERI (Receive error) */
            [12] = sdhimmc_accs_isr, /* SDHIMMC1 ACCS (Card access) */
            [13] = sdhimmc_card_isr, /* SDHIMMC1 CARD (Card detect) */
            [14] = dmac_int_isr, /* DMAC2 INT (DMAC2 transfer end) */
            [15] = sci_b_uart_rxi_isr, /* SCI9 RXI (Receive data full) */
            [16] = sci_b_uart_txi_isr, /* SCI9 TXI (Transmit data empty) */
            [17] = sci_b_uart_tei_isr, /* SCI9 TEI (Transmit end) */
            [18] = sci_b_uart_eri_isr, /* SCI9 ERI (Receive error) */
            [19] = sci_b_uart_rxi_isr, /* SCI6 RXI (Receive data full) */
            [20] = sci_b_uart_txi_isr, /* SCI6 TXI (Transmit data empty) */
            [21] = sci_b_uart_tei_isr, /* SCI6 TEI (Transmit end) */
            [22] = sci_b_uart_eri_isr, /* SCI6 ERI (Receive error) */
        };
        #if BSP_FEATURE_ICU_HAS_IELSR
        const bsp_interrupt_event_t g_interrupt_event_link_select[BSP_ICU_VECTOR_NUM_ENTRIES] =
        {
            [0] = BSP_PRV_VECT_ENUM(EVENT_CEU_CEUI,GROUP0), /* CEU CEUI (CEU interrupt) */
            [1] = BSP_PRV_VECT_ENUM(EVENT_SCI8_RXI,GROUP1), /* SCI8 RXI (Receive data full) */
            [2] = BSP_PRV_VECT_ENUM(EVENT_SCI8_TXI,GROUP2), /* SCI8 TXI (Transmit data empty) */
            [3] = BSP_PRV_VECT_ENUM(EVENT_SCI8_TEI,GROUP3), /* SCI8 TEI (Transmit end) */
            [4] = BSP_PRV_VECT_ENUM(EVENT_SCI8_ERI,GROUP4), /* SCI8 ERI (Receive error) */
            [5] = BSP_PRV_VECT_ENUM(EVENT_GLCDC_LINE_DETECT,GROUP5), /* GLCDC LINE DETECT (Specified line) */
            [6] = BSP_PRV_VECT_ENUM(EVENT_DRW_INT,GROUP6), /* DRW INT (DRW interrupt) */
            [7] = BSP_PRV_VECT_ENUM(EVENT_NPU_IRQ,GROUP7), /* NPU IRQ (NPU IRQ) */
            [8] = BSP_PRV_VECT_ENUM(EVENT_SCI5_RXI,GROUP0), /* SCI5 RXI (Receive data full) */
            [9] = BSP_PRV_VECT_ENUM(EVENT_SCI5_TXI,GROUP1), /* SCI5 TXI (Transmit data empty) */
            [10] = BSP_PRV_VECT_ENUM(EVENT_SCI5_TEI,GROUP2), /* SCI5 TEI (Transmit end) */
            [11] = BSP_PRV_VECT_ENUM(EVENT_SCI5_ERI,GROUP3), /* SCI5 ERI (Receive error) */
            [12] = BSP_PRV_VECT_ENUM(EVENT_SDHIMMC1_ACCS,GROUP4), /* SDHIMMC1 ACCS (Card access) */
            [13] = BSP_PRV_VECT_ENUM(EVENT_SDHIMMC1_CARD,GROUP5), /* SDHIMMC1 CARD (Card detect) */
            [14] = BSP_PRV_VECT_ENUM(EVENT_DMAC2_INT,GROUP6), /* DMAC2 INT (DMAC2 transfer end) */
            [15] = BSP_PRV_VECT_ENUM(EVENT_SCI9_RXI,GROUP7), /* SCI9 RXI (Receive data full) */
            [16] = BSP_PRV_VECT_ENUM(EVENT_SCI9_TXI,GROUP0), /* SCI9 TXI (Transmit data empty) */
            [17] = BSP_PRV_VECT_ENUM(EVENT_SCI9_TEI,GROUP1), /* SCI9 TEI (Transmit end) */
            [18] = BSP_PRV_VECT_ENUM(EVENT_SCI9_ERI,GROUP2), /* SCI9 ERI (Receive error) */
            [19] = BSP_PRV_VECT_ENUM(EVENT_SCI6_RXI,GROUP3), /* SCI6 RXI (Receive data full) */
            [20] = BSP_PRV_VECT_ENUM(EVENT_SCI6_TXI,GROUP4), /* SCI6 TXI (Transmit data empty) */
            [21] = BSP_PRV_VECT_ENUM(EVENT_SCI6_TEI,GROUP5), /* SCI6 TEI (Transmit end) */
            [22] = BSP_PRV_VECT_ENUM(EVENT_SCI6_ERI,GROUP6), /* SCI6 ERI (Receive error) */
        };
        #endif
        #endif