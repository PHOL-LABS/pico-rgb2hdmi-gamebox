if (TARGET tinyusb_device)
    pico_sdk_init()

    add_executable(hdmiTestVR2
        hdmiTestVR2.c
    )

    target_compile_options(hdmiTestVR2 PRIVATE -Wall)

    target_compile_definitions(hdmiTestVR2 PRIVATE
        DVI_VERTICAL_REPEAT=2
        DVI_N_TMDS_BUFFERS=5
    )

    target_link_libraries(hdmiTestVR2 PRIVATE
        libdvi
        pico_multicore
        pico_stdlib
    )

    pico_enable_stdio_usb(hdmiTestVR2 1)
    pico_enable_stdio_uart(hdmiTestVR2 0)
    pico_add_extra_outputs(hdmiTestVR2)

elseif(PICO_ON_DEVICE)
    message(WARNING "not building hdmiTestVR2 because TinyUSB submodule is not initialized in the SDK")
endif()
