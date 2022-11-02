# SPDX-License-Identifier: Apache-2.0

set(SUPPORTED_EMU_PLATFORMS acesim)

# Use mtl target for now ...
board_set_rimage_target(mtl)

set(RIMAGE_SIGN_KEY otc_private_key.pem)
