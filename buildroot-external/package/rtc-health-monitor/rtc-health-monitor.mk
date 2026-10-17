RTC_HEALTH_MONITOR_VERSION = 1.0
RTC_HEALTH_MONITOR_SITE = $(BR2_EXTERNAL_RTC_MONITORING_PATH)/package/rtc-health-monitor/src
RTC_HEALTH_MONITOR_SITE_METHOD = local
RTC_HEALTH_MONITOR_LICENSE = MIT
RTC_HEALTH_MONITOR_LICENSE_FILES = LICENSE

define RTC_HEALTH_MONITOR_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-o $(@D)/rtc-health-monitor $(@D)/rtc-health-monitor.c
endef

define RTC_HEALTH_MONITOR_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/rtc-health-monitor \
		$(TARGET_DIR)/usr/bin/rtc-health-monitor
	$(INSTALL) -D -m 0755 \
		$(BR2_EXTERNAL_RTC_MONITORING_PATH)/package/rtc-health-monitor/S99rtc-health-monitor \
		$(TARGET_DIR)/etc/init.d/S99rtc-health-monitor
endef

$(eval $(generic-package))
