#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/rtc.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t keep_running = 1;

static void
handle_signal(int signal_number)
{
	(void)signal_number;
	keep_running = 0;
}

static bool
format_time(time_t seconds, char *buffer, size_t buffer_size)
{
	struct tm broken_down;

	if (gmtime_r(&seconds, &broken_down) == NULL)
		return false;

	return strftime(buffer, buffer_size, "%Y-%m-%dT%H:%M:%SZ",
			&broken_down) > 0;
}

static bool
read_rtc(char *buffer, size_t buffer_size)
{
	struct rtc_time rtc_time;
	struct tm broken_down;
	time_t seconds;
	int fd = open("/dev/rtc0", O_RDONLY | O_CLOEXEC);

	if (fd < 0)
		return false;
	if (ioctl(fd, RTC_RD_TIME, &rtc_time) < 0) {
		close(fd);
		return false;
	}
	close(fd);

	memset(&broken_down, 0, sizeof(broken_down));
	broken_down.tm_sec = rtc_time.tm_sec;
	broken_down.tm_min = rtc_time.tm_min;
	broken_down.tm_hour = rtc_time.tm_hour;
	broken_down.tm_mday = rtc_time.tm_mday;
	broken_down.tm_mon = rtc_time.tm_mon;
	broken_down.tm_year = rtc_time.tm_year;
	broken_down.tm_isdst = 0;
	seconds = timegm(&broken_down);

	return format_time(seconds, buffer, buffer_size);
}

static bool
read_uptime(uint64_t *uptime_seconds)
{
	FILE *file = fopen("/proc/uptime", "r");
	double uptime;

	if (file == NULL)
		return false;
	if (fscanf(file, "%lf", &uptime) != 1 || uptime < 0.0) {
		fclose(file);
		return false;
	}
	fclose(file);
	*uptime_seconds = (uint64_t)uptime;
	return true;
}

static bool
read_memory(uint64_t *total_kb, uint64_t *available_kb)
{
	FILE *file = fopen("/proc/meminfo", "r");
	char *line = NULL;
	size_t capacity = 0;
	bool found_total = false;
	bool found_available = false;

	if (file == NULL)
		return false;

	while (getline(&line, &capacity, file) >= 0) {
		uint64_t value;

		if (sscanf(line, "MemTotal: %" SCNu64 " kB", &value) == 1) {
			*total_kb = value;
			found_total = true;
		} else if (sscanf(line, "MemAvailable: %" SCNu64 " kB",
				  &value) == 1) {
			*available_kb = value;
			found_available = true;
		}
	}

	free(line);
	fclose(file);
	return found_total && found_available;
}

static bool
read_filesystem(uint64_t *total_bytes, uint64_t *used_bytes)
{
	struct statvfs filesystem;
	uint64_t blocks;
	uint64_t free_blocks;

	if (statvfs("/data", &filesystem) < 0)
		return false;

	blocks = (uint64_t)filesystem.f_blocks;
	free_blocks = (uint64_t)filesystem.f_bfree;
	*total_bytes = blocks * (uint64_t)filesystem.f_frsize;
	*used_bytes = (blocks - free_blocks) * (uint64_t)filesystem.f_frsize;
	return true;
}

static const char *
read_network_state(void)
{
	DIR *directory = opendir("/sys/class/net");
	struct dirent *entry;
	bool found_interface = false;
	char path[256];
	char state[32];

	if (directory == NULL)
		return "unavailable";

	while ((entry = readdir(directory)) != NULL) {
		FILE *file;

		if (entry->d_name[0] == '.'
		    || strcmp(entry->d_name, "lo") == 0)
			continue;

		if (snprintf(path, sizeof(path), "/sys/class/net/%s/operstate",
			     entry->d_name) >= (int)sizeof(path))
			continue;
		file = fopen(path, "r");
		if (file == NULL)
			continue;

		if (fgets(state, sizeof(state), file) != NULL) {
			found_interface = true;
			if (strncmp(state, "up", 2) == 0) {
				fclose(file);
				closedir(directory);
				return "up";
			}
		}
		fclose(file);
	}

	closedir(directory);
	return found_interface ? "down" : "unavailable";
}

static int
write_record(FILE *output)
{
	struct timespec now;
	char system_time[32] = "unavailable";
	char rtc_time[32] = "unavailable";
	char uptime_field[32];
	char memory_total_field[32];
	char memory_available_field[32];
	char filesystem_total_field[32];
	char filesystem_used_field[32];
	uint64_t uptime = 0;
	uint64_t memory_total = 0;
	uint64_t memory_available = 0;
	uint64_t filesystem_total = 0;
	uint64_t filesystem_used = 0;
	bool have_system_time;
	bool have_uptime;
	bool have_memory;
	bool have_filesystem;

	have_system_time = clock_gettime(CLOCK_REALTIME, &now) == 0
		&& format_time(now.tv_sec, system_time, sizeof(system_time));
	(void)read_rtc(rtc_time, sizeof(rtc_time));
	have_uptime = read_uptime(&uptime);
	have_memory = read_memory(&memory_total, &memory_available);
	have_filesystem = read_filesystem(&filesystem_total, &filesystem_used);

	if (have_uptime)
		(void)snprintf(uptime_field, sizeof(uptime_field), "%" PRIu64,
			       uptime);
	else
		(void)snprintf(uptime_field, sizeof(uptime_field), "unavailable");
	if (have_memory) {
		(void)snprintf(memory_total_field, sizeof(memory_total_field),
			       "%" PRIu64, memory_total);
		(void)snprintf(memory_available_field,
			       sizeof(memory_available_field), "%" PRIu64,
			       memory_available);
	} else {
		(void)snprintf(memory_total_field, sizeof(memory_total_field),
			       "unavailable");
		(void)snprintf(memory_available_field,
			       sizeof(memory_available_field), "unavailable");
	}
	if (have_filesystem) {
		(void)snprintf(filesystem_total_field,
			       sizeof(filesystem_total_field), "%" PRIu64,
			       filesystem_total);
		(void)snprintf(filesystem_used_field,
			       sizeof(filesystem_used_field), "%" PRIu64,
			       filesystem_used);
	} else {
		(void)snprintf(filesystem_total_field,
			       sizeof(filesystem_total_field), "unavailable");
		(void)snprintf(filesystem_used_field,
			       sizeof(filesystem_used_field), "unavailable");
	}

	if (fprintf(output,
		    "system_time=%s rtc_time=%s uptime_sec=%s"
		    " memory_total_kb=%s memory_available_kb=%s"
		    " data_total_bytes=%s data_used_bytes=%s"
		    " network=%s\n",
		    have_system_time ? system_time : "unavailable",
		    rtc_time,
		    uptime_field,
		    memory_total_field,
		    memory_available_field,
		    filesystem_total_field,
		    filesystem_used_field,
		    read_network_state()) < 0)
		return -1;

	return fflush(output);
}

static int
ensure_log_directory(const char *log_path)
{
	char *directory = strdup(log_path);
	char *separator;
	int result = 0;

	if (directory == NULL)
		return -1;
	separator = strrchr(directory, '/');
	if (separator != NULL && separator != directory) {
		*separator = '\0';
		if (mkdir(directory, 0755) < 0 && errno != EEXIST)
			result = -1;
	}
	free(directory);
	return result;
}

static bool
parse_interval(const char *value, unsigned int *interval)
{
	char *end;
	unsigned long parsed;

	errno = 0;
	parsed = strtoul(value, &end, 10);
	if (errno != 0 || end == value || *end != '\0' || parsed == 0
	    || parsed > 86400)
		return false;
	*interval = (unsigned int)parsed;
	return true;
}

int
main(int argc, char **argv)
{
	const char *log_path = NULL;
	unsigned int interval = 60;
	bool once = false;
	FILE *output = stdout;
	struct sigaction action;
	int index;

	for (index = 1; index < argc; index++) {
		if (strcmp(argv[index], "--once") == 0) {
			once = true;
		} else if (strcmp(argv[index], "--interval") == 0
			   && index + 1 < argc) {
			if (!parse_interval(argv[++index], &interval)) {
				fprintf(stderr, "Invalid interval\n");
				return EXIT_FAILURE;
			}
		} else if (strcmp(argv[index], "--log") == 0
			   && index + 1 < argc) {
			log_path = argv[++index];
		} else {
			fprintf(stderr,
				"Usage: %s [--once] [--interval seconds] "
				"[--log path]\n", argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (log_path != NULL) {
		if (ensure_log_directory(log_path) < 0) {
			fprintf(stderr, "Cannot create log directory for %s: %s\n",
				log_path, strerror(errno));
			return EXIT_FAILURE;
		}
		output = fopen(log_path, "a");
		if (output == NULL) {
			fprintf(stderr, "Cannot open %s: %s\n",
				log_path, strerror(errno));
			return EXIT_FAILURE;
		}
	}

	memset(&action, 0, sizeof(action));
	action.sa_handler = handle_signal;
	sigemptyset(&action.sa_mask);
	if (sigaction(SIGTERM, &action, NULL) < 0
	    || sigaction(SIGINT, &action, NULL) < 0) {
		fprintf(stderr, "Cannot install signal handlers: %s\n",
			strerror(errno));
		if (output != stdout)
			fclose(output);
		return EXIT_FAILURE;
	}

	do {
		if (write_record(output) < 0) {
			fprintf(stderr, "Cannot write health record: %s\n",
				strerror(errno));
			if (output != stdout)
				fclose(output);
			return EXIT_FAILURE;
		}
		if (once)
			break;
		sleep(interval);
	} while (keep_running);

	if (output != stdout && fclose(output) != 0) {
		fprintf(stderr, "Cannot close log: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
