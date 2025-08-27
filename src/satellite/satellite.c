#include "main.h"

#include "usart.h"

#include <string.h>

#include "arribada_secret.h"

#define AT_CMD(cmd, value) "AT+" cmd "=" value "\r\n"

#define SMD_CMD_PING            AT_CMD("PING", "?")
#define SMD_CMD_RCONF_LDA2      AT_CMD("RCONF", "3d678af16b5a572078f3dbc95a1104e7")
#define SMD_CMD_RCONF_LDA2L     AT_CMD("RCONF", "bd176535b394a665bd86f354c5f424fb")
#define SMD_CMD_RCONF_VLDA4     AT_CMD("RCONF", "efd2412f8570581457f2d982e76d44d7")
#define SMD_CMD_RCONF_LDK       AT_CMD("RCONF", "03921fb104b92859209b18abd009de96")
#define SMD_CMD_SAVE_RCONF      AT_CMD("SAVE_RCONF", "")
#define SMD_CMD_SET_SECKEY      AT_CMD("SECKEY", ARRIBADA_SECRET_KEY)
#define SMD_CMD_SET_MAC_ADDR    AT_CMD("ADDR", ARRIBADA_MAC_ADDRESS)
#define SMD_CMD_INIT_MAC_CONFIG AT_CMD("KMAC", "0")
#define SMD_CMD_SAVE_MAC_CONFIG AT_CMD("KMAC", "1")

typedef enum {
    ARRIBADA_RCONF_LDA2,
    ARRIBADA_RCONF_LDA2L,
    ARRIBADA_RCONF_VLDA4,
    ARRIBADA_RCONF_LDK,
} ArribadaRadioConfig;

/******************************************************************************
 *  HARDWARE CODE (replace for given )
 */
static int __satellite_read(char *pResponse, uint16_t response_capacity, uint16_t *response_len) {
    return HAL_UARTEx_ReceiveToIdle(&huart2, (uint8_t *)pResponse, response_capacity, response_len, 5000);

} 

static int __satellite_write(char *pCommand, uint16_t command_len) {
    return HAL_UART_Transmit(&huart2, (uint8_t *)pCommand, command_len, 1000);
}

static void __satellite_hw_init(void) { 

    /* configure gpios */
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    HAL_GPIO_WritePin(GPIOE, SAT_PWR_EN_GPIO_Output_Pin | SAT_PM_2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, SAT_NRST_GPIO_Output_Pin | SAT_PM_1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOD, SAT_RF_NRST_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = SAT_PWR_EN_GPIO_Output_Pin | SAT_PM_2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = SAT_NRST_GPIO_Output_Pin|SAT_PM_1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = SAT_RF_NRST_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = SAT_RF_BUSY_GPIO_Input_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* configure uart*/
    MX_USART2_UART_Init();

    /* Reset module */
    HAL_GPIO_WritePin(SAT_PWR_EN_GPIO_Output_GPIO_Port, SAT_PWR_EN_GPIO_Output_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, SAT_NRST_GPIO_Output_Pin | SAT_PM_1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOD, SAT_RF_NRST_Pin, GPIO_PIN_SET);
    HAL_Delay(1000);
}

void satellite_wait_for_programming(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // Enable power to module
    HAL_GPIO_WritePin(SAT_PWR_EN_GPIO_Output_GPIO_Port, SAT_PWR_EN_GPIO_Output_Pin, GPIO_PIN_SET);
    
    GPIO_InitStruct.Pin = SAT_PWR_EN_GPIO_Output_Pin | SAT_PM_2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SAT_PWR_EN_GPIO_Output_GPIO_Port, &GPIO_InitStruct);

    //set NRST as input so that stlink can take control
    GPIO_InitStruct.Pin = SAT_NRST_GPIO_Output_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(SAT_NRST_GPIO_Output_GPIO_Port, &GPIO_InitStruct);

    while(1) {
        // wait for user to do whatever;
        __NOP();
    }
}


/******************************************************************************
 *  HIGH LEVEL ARRIBADA CONTROL 
 */

int satellite_ping(void) {
    char response[128] = {0};
    uint16_t response_len = 0;
    __satellite_write(SMD_CMD_PING, strlen(SMD_CMD_PING));
    __satellite_read(response, sizeof(response), &response_len);
    return (memcmp(response, "+OK", 3) == 0);
}

void satellite_configure_radio(ArribadaRadioConfig protocol) {
    switch(protocol){
        case ARRIBADA_RCONF_LDA2:
            __satellite_write(SMD_CMD_RCONF_LDA2, strlen(SMD_CMD_RCONF_LDA2));
            break;

        case ARRIBADA_RCONF_LDA2L:
            __satellite_write(SMD_CMD_RCONF_LDA2L, strlen(SMD_CMD_RCONF_LDA2L));
            break;

        case ARRIBADA_RCONF_VLDA4:
            __satellite_write(SMD_CMD_RCONF_VLDA4, strlen(SMD_CMD_RCONF_VLDA4));
            break;

        case ARRIBADA_RCONF_LDK:
            __satellite_write(SMD_CMD_RCONF_LDK, strlen(SMD_CMD_RCONF_LDK));
            break;
    }
    __satellite_write(SMD_CMD_SAVE_RCONF, strlen(SMD_CMD_SAVE_RCONF));
}

void satellite_transmit(char *message, size_t message_len) {
    // ToDo: check message_len based on current RCONF
    char tx_buffer[128] = "AT+TX=";
    uint16_t len = 6;
    memcpy(&tx_buffer[len], message, message_len);
    len += message_len;
    tx_buffer[len++] = '\r';
    tx_buffer[len++] = '\n';
    __satellite_write(tx_buffer, len);
}

void satellite_init(void) {
    char response[128] = {};
    uint16_t response_len = 0;
    __satellite_hw_init();
    
    //read version
    __satellite_read(response, sizeof(response), &response_len);

    // ping board to verify comms
    int established_comms = satellite_ping();
    
    if (!established_comms) {
        // error
    	__NOP();
    }

    //set radio protocol
    satellite_configure_radio(ARRIBADA_RCONF_LDA2);

    // ToDo: set Secret
    __satellite_write(SMD_CMD_SET_SECKEY, strlen(SMD_CMD_SET_SECKEY));

    // update MAC profile
    __satellite_write(SMD_CMD_SET_MAC_ADDR, strlen(SMD_CMD_SET_MAC_ADDR));
    __satellite_write(SMD_CMD_INIT_MAC_CONFIG, strlen(SMD_CMD_INIT_MAC_CONFIG));
    __satellite_write(SMD_CMD_SAVE_MAC_CONFIG, strlen(SMD_CMD_SAVE_MAC_CONFIG));
}