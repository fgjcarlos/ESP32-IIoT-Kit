# Same test file the host runner compiles (tools/lcd-smoke/run-host.sh). It is
# kept in the component layout of the repository so a device test project can
# pick it up without duplicating the assertions.
idf_component_register(SRCS "test_lcd_smoke_pattern.c"
                       INCLUDE_DIRS "."
                       REQUIRES lcd_smoke_pattern
                       PRIV_REQUIRES unity)
