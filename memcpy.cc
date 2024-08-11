#include "memcpy.h"

#include "hardware/dma.h"
#include "hardware/irq.h"

static uint8_t dma_fill_chan;
static dma_channel_config chan_config;

void
memcpy_dma_init()
{
    /* 4-byte DMA transfer width */
    dma_fill_chan = dma_claim_unused_channel(true);
    // dma_channel_config cfg32 = dma_channel_get_default_config(chan16);
    // channel_config_set_write_increment(&cfg32, true);
    // channel_config_set_transfer_data_size(&cfg32, DMA_SIZE_16);
    // dma_channel_set_config(chan16, &cfg32, false);

    chan_config = dma_channel_get_default_config(dma_fill_chan);
    channel_config_set_transfer_data_size(&chan_config, DMA_SIZE_32);
    channel_config_set_read_increment(&chan_config, false);
    channel_config_set_write_increment(&chan_config, true);
}

// void memcpy_32(uint32_t *dst, const uint32_t *src, size_t cnt) {
//   dma_channel_set_read_addr(chan32, src, false);
//   dma_channel_set_write_addr(chan32, dst, false);
//   dma_channel_set_trans_count(chan32, cnt >> 2, false);
//   dma_channel_start(chan32);
// }

void
__not_in_flash_func(memset32_dma)(uint32_t* dst, uint32_t val, size_t cnt)
{

    dma_channel_configure(dma_fill_chan,
                          &chan_config,
                          dst,  // destination to set
                          &val, // values to use
                          cnt,  // cnt to set
                          true  // start immediately
    );

    dma_channel_wait_for_finish_blocking(dma_fill_chan);
}

void
__not_in_flash_func(memset_dma_ready)()
{
    // wait for dma to finish
    dma_channel_wait_for_finish_blocking(dma_fill_chan);
}