add_custom_target(
  gen_signed_image
  COMMAND
  ${RIMAGE}
  -k ${ZEPHYR_SOF_MODULE_DIR}/keys/${RIMAGE_SIGN_KEY}
  -c ${ZEPHYR_SOF_MODULE_DIR}/rimage/config/${RIMAGE_TARGET}.toml
  -i 3
  ${PROJECT_BINARY_DIR}/boot.mod
  ${PROJECT_BINARY_DIR}/main.mod
  -o ${PROJECT_BINARY_DIR}/zephyr.ri
  DEPENDS gen_modules
)
