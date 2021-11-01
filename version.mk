EXTRA_CPPFLAGS  ?=
ifeq ("$(CONFIG_CHIP_NAME)", "BL602")
EXTRA_CPPFLAGS  += -D BL_SDK_VER=\"release_bl_iot_sdk_1.6.32-68-geab3854c5\"
EXTRA_CPPFLAGS  += -D BL_SDK_PHY_VER=\"a0_final-74-g478a1d5\"
EXTRA_CPPFLAGS  += -D BL_SDK_RF_VER=\"0a5bc1d\"
EXTRA_CPPFLAGS  += -D BL_SDK_STDDRV_VER=\"dbb5408\"
endif
ifeq ("$(CONFIG_CHIP_NAME)", "BL702")
EXTRA_CPPFLAGS  += -D BL_SDK_VER=\"release_bl_iot_sdk_1.6.32-68-geab3854c5\"
EXTRA_CPPFLAGS  += -D BL_SDK_STDDRV_VER=\"2fa0580\"
EXTRA_CPPFLAGS  += -D BL_SDK_STDCOM_VER=\"8cf52ec\"
EXTRA_CPPFLAGS  += -D BL_SDK_RF_VER=\"f8ef9db\"
endif
