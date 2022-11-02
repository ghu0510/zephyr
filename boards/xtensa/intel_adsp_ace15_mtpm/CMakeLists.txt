add_custom_target(
  gen_signed_image
  COMMAND
  ${RIMAGE}
  -k ${BOARD_DIR}/support/dummy-key.pem
  -c ${ZEPHYR_SOF_MODULE_DIR}/rimage/config/${RIMAGE_TARGET}.toml
  -i 3
  ${PROJECT_BINARY_DIR}/boot.mod
  ${PROJECT_BINARY_DIR}/main.mod
  -o ${PROJECT_BINARY_DIR}/zephyr.ri
  DEPENDS gen_modules
)
