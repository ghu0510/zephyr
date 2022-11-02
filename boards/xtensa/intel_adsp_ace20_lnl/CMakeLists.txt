add_custom_target(
  gen_signed_image
  COMMAND
  ${RIMAGE}
  -k ${BOARD_DIR}/support/dummy-key.pem
  -c ${BOARD_DIR}/support/ace20.toml
  -i 3
  ${PROJECT_BINARY_DIR}/boot.mod
  ${PROJECT_BINARY_DIR}/main.mod
  -o ${PROJECT_BINARY_DIR}/zephyr.ri
  DEPENDS gen_modules
)
