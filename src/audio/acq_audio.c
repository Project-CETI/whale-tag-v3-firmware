/*****************************************************************************
 *   @file      audio/acq_audio.c
 *   @brief     audio acquisition code. Note this code just gather audio data 
 *              into RAM, but does not perform any analysis, transformation, or
 *              storage of said data.
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [TODO: Add other contributors here]
 *****************************************************************************/

/*****************************************************************************
 * MEMORY STORAGE OPTIONS :
 * 
 * CURRENT
 *  ___      _________      _______________      _______________
 * |SAI| -> |RAM 6K   | -> | Storage RAM   | -> | Flash (exFAT) |
 * |___|    |_3K_|_3K_|    |_3K_|_3K_|_..._|    |_______________|
 * DMA's from the 6K circular buffer to storage buffer allow enough time for
 * the 
 * 
 *  ___      _________      _______________      _______________
 * |SAI| -> |RAM 6K   | -> | Flash (raw)   | -> | Flash (exFAT) |
 * |___|    |_3K_|_3K_|    |_3K_|_3K_|_..._|    |_______________|
 * 
 * 
 *****************************************************************************/
#include "acq_audio.h"

#include "config.h"
#include "error.h"

// Middleware
#include "main.h"
#include <app_filex.h>
#include <sai.h>
#include <spi.h>
#include <stm32u5xx_hal.h>
#include <stm32u5xx_hal_sai.h>

// Standard libraries
#include <stdbool.h>

#define ADC_AD7768 0
#define AUDIO_ADC_PART_NUMBER ADC_AD7768

// Supported Hardware
#if AUDIO_ADC_PART_NUMBER == ADC_AD7768
    #include "ad7768.h" // this hardware is only used here, so no need for a header
#endif

// External Variables =========================================================
#if AUDIO_ADC_PART_NUMBER == ADC_AD7768
    extern SPI_HandleTypeDef AUDIO_hspi;
#endif
extern FX_MEDIA sdio_disk;

// MACROS =====================================================================
#define AUDIO_CHANNEL_COUNT (AUDIO_CH_0_EN + AUDIO_CH_1_EN + AUDIO_CH_2_EN + AUDIO_CH_3_EN)


#if AUDIO_SAMPLE_BITDEPTH == 24
    #define AUDIO_SAI_CHANNEL_MASK (0b1110)
#elif AUDIO_SAMPLE_BITDEPTH == 16
    #define AUDIO_SAI_CHANNEL_MASK (0b0110)
#endif

#define AUDIO_SAI_SLOT_MASK (((AUDIO_CH_0_EN * AUDIO_SAI_CHANNEL_MASK) << 0) \
    | ((AUDIO_CH_1_EN * AUDIO_SAI_CHANNEL_MASK) << 4)                        \
    | ((AUDIO_CH_2_EN * AUDIO_SAI_CHANNEL_MASK) << 8)                        \
    | ((AUDIO_CH_3_EN * AUDIO_SAI_CHANNEL_MASK) << 12))

#define AUDIO_BUFFER_BLOCK_TO_HALF(block) ((block) / (AUDIO_BUFFER_SIZE_BLOCKS / 2))

// Public Variables ===========================================================
DMA_HandleTypeDef s_audio_dma_channel;

// Private Variables ===========================================================
static DMA_QListTypeDef s_audio_dma_queue;
static DMA_NodeTypeDef s_audio_dma_nodes[AUDIO_BUFFER_SIZE_BLOCKS];
static SAI_HandleTypeDef s_audio_hsai;

static union {
    uint8_t block[AUDIO_BUFFER_SIZE_BLOCKS][AUDIO_BLOCK_SIZE]; // accessed for DMA
    uint8_t half[2][AUDIO_BLOCK_SIZE * AUDIO_BUFFER_SIZE_BLOCKS /2]; // access for SD card write
} s_audio_buffer;

static volatile uint8_t s_audio_buffer_read_half = 0;
static volatile uint8_t s_audio_buffer_write_half = 0;

static uint8_t sd_card_writing = 0;
static uint8_t s_audio_enabled = 0;
static uint8_t s_audio_running = 0;

static AcqAudioLogCallback s_acq_audio_log_callback = NULL;

#if AUDIO_ADC_PART_NUMBER == ADC_AD7768
static ad7768_dev audio_adc = {
    .spi_handler = &AUDIO_hspi,
    .spi_cs_port = AUDIO_NCS_GPIO_Output_GPIO_Port,
    .spi_cs_pin = AUDIO_NCS_GPIO_Output_Pin,
    .channel_standby = {
#if AUDIO_CH_0_EN == 1
        .ch[0] = AD7768_ENABLED,
#else
        .ch[0] = AD7768_STANDBY,
#endif
#if AUDIO_CH_1_EN == 1
        .ch[1] = AD7768_ENABLED,
#else 
        .ch[1] = AD7768_STANDBY,
#endif
#if AUDIO_CH_2_EN == 1
        .ch[2] = AD7768_ENABLED,
#else 
        .ch[2] = AD7768_STANDBY,
#endif
#if AUDIO_CH_3_EN == 1
        .ch[3] = AD7768_ENABLED,
#else 
        .ch[3] = AD7768_STANDBY,
#endif
    },
    .channel_mode[AD7768_MODE_A] = {
        .filter_type = AD7768_FILTER_SINC,
#if (AUDIO_SAMPLERATE_SPS == 96000) && (AUDIO_PRIORITY == AUDIO_PRIORITIZE_NOISE)
            .dec_rate = AD7768_DEC_X64,
#elif (AUDIO_SAMPLERATE_SPS == 96000) && (AUDIO_PRIORITY == AUDIO_PRIORITIZE_POWER)
            .dec_rate = AD7768_DEC_X32,
#elif AUDIO_SAMPLERATE_SPS == 192000
            .dec_rate = AD7768_DEC_X32,
#endif
    },
    .channel_mode[AD7768_MODE_B] = {
#if AUDIO_FILTER_TYPE == AUDIO_FILTER_SINC
        .filter_type = AD7768_FILTER_SINC,
#else
		.filter_type = AD7768_FILTER_WIDEBAND,
#endif
#if (AUDIO_SAMPLERATE_SPS == 96000) && (AUDIO_PRIORITY == AUDIO_PRIORITIZE_NOISE)
            .dec_rate = AD7768_DEC_X64,
#elif (AUDIO_SAMPLERATE_SPS == 96000) && (AUDIO_PRIORITY == AUDIO_PRIORITIZE_POWER)
            .dec_rate = AD7768_DEC_X32,
#elif AUDIO_SAMPLERATE_SPS == 192000
            .dec_rate = AD7768_DEC_X32,
#endif
    },
    .channel_mode_select = {
#if AUDIO_CH_0_EN == 1
        .ch[0] = AD7768_MODE_B,
#else 
        .ch[0] = AD7768_MODE_A,
#endif
#if AUDIO_CH_1_EN == 1
        .ch[1] = AD7768_MODE_B,
#else 
        .ch[1] = AD7768_MODE_A,
#endif
#if AUDIO_CH_2_EN == 1
        .ch[2] = AD7768_MODE_B,
#else 
        .ch[2] = AD7768_MODE_A,
#endif
#if AUDIO_CH_3_EN == 1
        .ch[3] = AD7768_MODE_B,
#else 
        .ch[3] = AD7768_MODE_A,
#endif
    },
    .power_mode = {
#if (AUDIO_SAMPLERATE_SPS == 96000) && (AUDIO_PRIORITY == AUDIO_PRIORITIZE_NOISE)
            .sleep_mode = AD7768_ACTIVE,
            .power_mode = AD7768_FAST,
            .lvds_enable = false,
            .mclk_div = AD7768_MCLK_DIV_4,
#elif (AUDIO_SAMPLERATE_SPS == 96000) && (AUDIO_PRIORITY == AUDIO_PRIORITIZE_POWER)
            .sleep_mode = AD7768_ACTIVE,
            .power_mode = AD7768_MEDIAN,
            .lvds_enable = false,
            .mclk_div = AD7768_MCLK_DIV_8,
#elif AUDIO_SAMPLERATE_SPS == 192000
            .sleep_mode = AD7768_ACTIVE,
            .power_mode = AD7768_FAST,
            .lvds_enable = false,
            .mclk_div = AD7768_MCLK_DIV_4,
#endif
        },
    .interface_config = {
#if AUDIO_SAMPLERATE_SPS == 192000
        .crc_select = AD7768_CRC_NONE,
        .dclk_div = AD7768_DCLK_DIV_1,
#elif AUDIO_SAMPLERATE_SPS == 96000
        .crc_select = AD7768_CRC_NONE,
        .dclk_div = AD7768_DCLK_DIV_1,
#endif
        },
    .pin_spi_ctrl = AD7768_SPI_CTRL,
};
#endif

/// @brief Serial Audio Interface link-list item complete callback. This code
/// tracks which audio block we have written to and wakes the tag is a trans
/// @param hsai 
/// @note Expected callback intervals:
///     0xFFF0 B / (96 ksmpl/s * 3 ch/smpl * 2 byte/ch) = 65,520 B /  576,000 B/s = 113.75 ms
///     0xFFF0  B / (96 ksmpl/s * 4 ch/smpl * 3 byte/ch) = 65,520 B / 1152,000 kB/s = 56.875 mS 
void acq_audio_dma_complete_callback(DMA_HandleTypeDef *hdma) {
    static uint8_t s_audio_buffer_write_block = 0;
    s_audio_buffer_write_block = (s_audio_buffer_write_block + 1) % (AUDIO_BUFFER_SIZE_BLOCKS);
    if (s_audio_buffer_read_half != AUDIO_BUFFER_BLOCK_TO_HALF(s_audio_buffer_write_block)) {
        if (!sd_card_writing){
            // signal sd card write
            sd_card_writing = 1;
            s_audio_buffer_write_half = AUDIO_BUFFER_BLOCK_TO_HALF(s_audio_buffer_write_block);
            HAL_PWR_DisableSleepOnExit(); // ensures control transitions to CPU for SD card write
        } else {
            // ToDo: SD card write too slow
        }
    }
}

/// @brief configures Serial Audio Interface of stm32 to interface with the ad7768
/// @param  
HAL_StatusTypeDef acq_audio_sai_init(void) {
    s_audio_hsai.Instance = SAI1_Block_A;
    s_audio_hsai.Init.Protocol = SAI_FREE_PROTOCOL;
    s_audio_hsai.Init.AudioMode = SAI_MODESLAVE_RX;
    s_audio_hsai.Init.DataSize = SAI_DATASIZE_8; // datasize of 8 required for complete control of audio bit mask 
    s_audio_hsai.Init.FirstBit = SAI_FIRSTBIT_MSB;
    s_audio_hsai.Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;
    s_audio_hsai.Init.Synchro = SAI_ASYNCHRONOUS;
    s_audio_hsai.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
    s_audio_hsai.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
    s_audio_hsai.Init.MckOverSampling = SAI_MCK_OVERSAMPLING_DISABLE;
    s_audio_hsai.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
    s_audio_hsai.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
    s_audio_hsai.Init.MckOutput = SAI_MCK_OUTPUT_ENABLE;
    s_audio_hsai.Init.MonoStereoMode = SAI_STEREOMODE;
    s_audio_hsai.Init.CompandingMode = SAI_NOCOMPANDING;
    s_audio_hsai.Init.TriState = SAI_OUTPUT_NOTRELEASED;
    s_audio_hsai.Init.PdmInit.Activation = DISABLE;
    s_audio_hsai.Init.PdmInit.MicPairsNbr = 1;
    s_audio_hsai.Init.PdmInit.ClockEnable = SAI_PDM_CLOCK1_ENABLE;
    s_audio_hsai.FrameInit.FrameLength = 128;
    s_audio_hsai.FrameInit.ActiveFrameLength = 1;
    s_audio_hsai.FrameInit.FSDefinition = SAI_FS_STARTFRAME;
    s_audio_hsai.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
    s_audio_hsai.FrameInit.FSOffset = SAI_FS_FIRSTBIT;
    s_audio_hsai.SlotInit.FirstBitOffset = 0;
    s_audio_hsai.SlotInit.SlotSize = SAI_SLOTSIZE_DATASIZE;
    s_audio_hsai.SlotInit.SlotNumber = 16;
    s_audio_hsai.SlotInit.SlotActive = AUDIO_SAI_SLOT_MASK;
    return HAL_SAI_Init(&s_audio_hsai);
}

/// @brief configures serial audio interfaces link-list DMA and callbacks
/// @param  
/// @return 
HAL_StatusTypeDef acq_audio_sai_dma_init(void) {
    HAL_StatusTypeDef ret = HAL_OK;
    /* DMA node configuration declaration */
    DMA_NodeConfTypeDef pNodeConfig;

    // common link_list node configuration
    pNodeConfig.NodeType = DMA_GPDMA_LINEAR_NODE;
    pNodeConfig.Init.Request = GPDMA1_REQUEST_SAI1_A;
    pNodeConfig.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
    pNodeConfig.Init.Direction = DMA_PERIPH_TO_MEMORY;
    pNodeConfig.Init.SrcInc = DMA_SINC_FIXED;
    pNodeConfig.Init.DestInc = DMA_DINC_INCREMENTED;
    pNodeConfig.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
    pNodeConfig.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
    pNodeConfig.Init.SrcBurstLength = 1;
    pNodeConfig.Init.DestBurstLength = 1;
    pNodeConfig.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0|DMA_DEST_ALLOCATED_PORT0;
    pNodeConfig.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
    pNodeConfig.TriggerConfig.TriggerPolarity = DMA_TRIG_POLARITY_MASKED;
    pNodeConfig.DataHandlingConfig.DataExchange = DMA_EXCHANGE_NONE;
    pNodeConfig.DataHandlingConfig.DataAlignment = DMA_DATA_RIGHTALIGN_ZEROPADDED;
    pNodeConfig.SrcAddress = (uint32_t)&s_audio_hsai.Instance->DR;
    pNodeConfig.DataSize = AUDIO_BLOCK_SIZE;

    // increment transfer address each node
    for(int i = 0; i < AUDIO_BUFFER_SIZE_BLOCKS; i++) {
        pNodeConfig.DstAddress = (uint32_t)s_audio_buffer.block[i];
        ret |= HAL_DMAEx_List_BuildNode(&pNodeConfig, &s_audio_dma_nodes[i]);
        ret |= HAL_DMAEx_List_InsertNode_Tail(&s_audio_dma_queue, &s_audio_dma_nodes[i]);
    }

    HAL_DMAEx_List_SetCircularMode(&s_audio_dma_queue);

    // initialize dma channel
    s_audio_dma_channel.Instance = GPDMA1_Channel0;
    s_audio_dma_channel.InitLinkedList.Priority = DMA_HIGH_PRIORITY;
    s_audio_dma_channel.InitLinkedList.LinkStepMode = DMA_LSM_FULL_EXECUTION;
    s_audio_dma_channel.InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT0;
    s_audio_dma_channel.InitLinkedList.TransferEventMode = DMA_TCEM_EACH_LL_ITEM_TRANSFER;
    s_audio_dma_channel.InitLinkedList.LinkedListMode = DMA_LINKEDLIST_CIRCULAR;
    ret |= HAL_DMAEx_List_Init(&s_audio_dma_channel);
    
    // link link-list queue
    ret |= HAL_DMAEx_List_LinkQ(&s_audio_dma_channel, &s_audio_dma_queue);
    
    // register callbacks
    s_audio_dma_channel.XferCpltCallback = acq_audio_dma_complete_callback;
    s_audio_dma_channel.XferErrorCallback = NULL;
    s_audio_dma_channel.XferAbortCallback = NULL;

    // link dma to sai
    s_audio_hsai.hdmarx = &s_audio_dma_channel;
    s_audio_dma_channel.Parent = &s_audio_hsai;

    ret |= HAL_DMA_ConfigChannelAttributes(&s_audio_dma_channel, DMA_CHANNEL_NPRIV);

    return ret;
}

/// @brief starts serial audio interface link-lisk DMA
/// @param  
/// @return 
HAL_StatusTypeDef acq_audio_sai_dma_start(void) {
    HAL_StatusTypeDef status = HAL_OK;

    if (HAL_SAI_STATE_READY != s_audio_hsai.State) {
        return HAL_BUSY;
    }

    __HAL_LOCK(&s_audio_hsai);

    s_audio_hsai.pBuffPtr = s_audio_buffer.block[0];
    s_audio_hsai.XferSize = AUDIO_BLOCK_SIZE;
    s_audio_hsai.XferCount = AUDIO_BLOCK_SIZE;
    s_audio_hsai.ErrorCode = HAL_SAI_ERROR_NONE;
    s_audio_hsai.State = HAL_SAI_STATE_BUSY_RX;

    status = HAL_DMAEx_List_Start_IT(s_audio_hsai.hdmarx);
    if (HAL_OK != status) {
        __HAL_UNLOCK(&s_audio_hsai);
        return HAL_ERROR;
    }

    __HAL_SAI_ENABLE_IT(&s_audio_hsai, SAI_IT_OVRUDR | SAI_IT_AFSDET | SAI_IT_LFSDET);

    /* Enable SAI Rx DMA Request */
    s_audio_hsai.Instance->CR1 |= SAI_xCR1_DMAEN;

    /* Check if the SAI is already enabled */
    if ((s_audio_hsai.Instance->CR1 & SAI_xCR1_SAIEN) == 0U)
    {
      /* Enable SAI peripheral */
      __HAL_SAI_ENABLE(&s_audio_hsai);
    }

    return HAL_OK;
    /* Process Locked */

}

void acq_audio_configure_gpios(void) {

}

int acq_audio_init(void) {
  if (s_audio_enabled) {
    // nothing to do
    return 0;
  }

  // configure MCU hardware for serial audio interface
  acq_audio_sai_init();
  acq_audio_sai_dma_init();
  MX_SPI1_Init();

  /* turn on power to audio front-end */
  HAL_GPIO_WritePin(Audio_VN_NEN_GPIO_Output_GPIO_Port, Audio_VN_NEN_GPIO_Output_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(AUDIO_VP_EN_GPIO_Output_GPIO_Port, AUDIO_VP_EN_GPIO_Output_Pin, GPIO_PIN_RESET);
  HAL_Delay(1000);
  HAL_GPIO_WritePin(Audio_VN_NEN_GPIO_Output_GPIO_Port, Audio_VN_NEN_GPIO_Output_Pin, GPIO_PIN_RESET);
  HAL_Delay(200);
  HAL_GPIO_WritePin(AUDIO_VP_EN_GPIO_Output_GPIO_Port, AUDIO_VP_EN_GPIO_Output_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(AUDIO_NRST_GPIO_Output_GPIO_Port, AUDIO_NRST_GPIO_Output_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(AUDIO_NCS_GPIO_Output_GPIO_Port, AUDIO_NCS_GPIO_Output_Pin, GPIO_PIN_RESET);

  // ToDo: setup MCU to handle audio
  // Configure external hardware (ADC)
  int result = ad7768_setup(&audio_adc);

  // after ad7768 is configured we no longer need the spi peripheral until we
  // shut it down
  HAL_SPI_DeInit(&AUDIO_hspi);
  __HAL_RCC_SPI3_CLK_DISABLE();

  // ToDo: recofigure spi_gpio as analog to conserve power

  // Dummy delay
  HAL_Delay(1000);

  s_audio_enabled = 1;
  return result;
}

void acq_audio_disable(void) {
  // Deregister callbacks

  // ToDo: Deconfigure audio hardware

  // ToDo: Disable unused MCU resources
  HAL_SAI_DeInit(&s_audio_hsai);

  /* turn off power to audio front-end */
  HAL_GPIO_WritePin(AUDIO_VP_EN_GPIO_Output_GPIO_Port,
                    AUDIO_VP_EN_GPIO_Output_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(Audio_VN_NEN_GPIO_Output_GPIO_Port,
                    Audio_VN_NEN_GPIO_Output_Pin, GPIO_PIN_SET);

  s_audio_enabled = 0;
}

void acq_audio_start(void) {
    if(s_audio_running) {
        return;
    }
    // initiate transfers
    acq_audio_sai_dma_start();
    s_audio_running = 1;

}

void acq_audio_stop(void) {
    HAL_SAI_DMAStop(&s_audio_hsai);
    s_audio_running = 0;
}

void acq_audio_set_log_callback(AcqAudioLogCallback cb) {
  s_acq_audio_log_callback = cb;
}

/// @note Expected SD card write intervals:
///     Min: 32 * 56.9 mS = 1.82 S
///     Max: 32 * 28.444 mS = 910 mS
void acq_audio_task(void) {
    uint8_t nv_read_half = s_audio_buffer_read_half;
    if (nv_read_half != s_audio_buffer_write_half) {
        if (s_acq_audio_log_callback != NULL) {
            uint8_t *p_data;
            size_t data_size;
            p_data = s_audio_buffer.half[nv_read_half];
            data_size = AUDIO_BLOCK_SIZE * AUDIO_BUFFER_SIZE_BLOCKS / 2;
            s_acq_audio_log_callback(p_data, data_size);
        }
        sd_card_writing = 0;
        s_audio_buffer_read_half = s_audio_buffer_write_half;
    }
}

// (MSH) as there is not instance in which audio is captured, but not logged it is fine to combine them into a single audio API
