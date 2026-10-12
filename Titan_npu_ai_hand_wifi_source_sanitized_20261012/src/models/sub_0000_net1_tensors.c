#include "sub_0000_net1_tensors.h"

const TensorInfo sub_0000_net1_tensors[] = {
  { "_split_1_command_stream", 2, 10496, "COMMAND_STREAM", 0xffffffff },
  { "_split_1_flash", 3, 443456, "MODEL", 0xffffffff },
  { "_split_1_scratch", 4, 442368, "ARENA", 0x0 },
  { "_split_1_scratch_fast", 5, 442368, "FAST_SCRATCH", 0x0 },
  { "serving_default_images_0", 6, 110592, "INPUT_TENSOR", 0x24000 },
  { "PartitionedCall_0_70286", 0, 9504, "OUTPUT_TENSOR", 0xd80 },
  { "PartitionedCall_1_70275", 1, 2376, "OUTPUT_TENSOR", 0x0 },
};

const size_t sub_0000_net1_tensors_count = sizeof(sub_0000_net1_tensors) / sizeof(sub_0000_net1_tensors[0]);

// Addresses for each input and output buffer inside of the arena
const uint32_t sub_0000_net1_address_serving_default_images_0 = 0x24000;
const uint32_t sub_0000_net1_address_PartitionedCall_0_70286 = 0xd80;
const uint32_t sub_0000_net1_address_PartitionedCall_1_70275 = 0x0;

