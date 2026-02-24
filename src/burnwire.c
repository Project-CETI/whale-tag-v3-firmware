#include <main.h>

void burnwire_init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    HAL_GPIO_WritePin(BURNWIRE_EN_GPIO_Output_GPIO_Port, BURNWIRE_EN_GPIO_Output_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pins : BURNWIRE_EN_GPIO_Output_Pin */
    GPIO_InitStruct.Pin = BURNWIRE_EN_GPIO_Output_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BURNWIRE_EN_GPIO_Output_GPIO_Port, &GPIO_InitStruct);
}

void burnwire_on(void) {
    HAL_GPIO_WritePin(BURNWIRE_EN_GPIO_Output_GPIO_Port, BURNWIRE_EN_GPIO_Output_Pin, GPIO_PIN_SET);
}

void burnwire_off(void) {
    HAL_GPIO_WritePin(BURNWIRE_EN_GPIO_Output_GPIO_Port, BURNWIRE_EN_GPIO_Output_Pin, GPIO_PIN_RESET);
}
