#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <dirent.h>

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#else
#include <sys/sysinfo.h>
#endif

/* Helpers */

static void
chomp(char *s)
{
	size_t n = strlen(s);
	while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' '))
		s[--n] = '\0';
}

/* OS/kernel */

static void
get_os(char *buf, size_t sz)
{
#ifdef __APPLE__
	char ver[64] = {0};
	size_t len = sizeof(ver);
	if (sysctlbyname("kern.osproductversion", ver, &len, NULL, 0) == 0)
		snprintf(buf, sz, "macOS %s", ver);
	else
		snprintf(buf, sz, "macOS");
#else
	FILE *f;
	char line[256];
	char name[128] = {0};

	f = fopen("/etc/os-release", "r");
	if (!f) f = fopen("/usr/lib/os-release", "r");
	if (f) {
		while (fgets(line, sizeof(line), f)) {
			if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
				char *val = line + 12;
				if (*val == '"') val++;
				chomp(val);
				size_t l = strlen(val);
				if (l && val[l-1] == '"') val[l-1] = '\0';
				snprintf(name, sizeof(name), "%s", val);
				break;
			}
		}
		fclose(f);
	}

	if (!name[0])
		snprintf(name, sizeof(name), "Linux");

	snprintf(buf, sz, "%s", name);
#endif
}

static void
get_kernel(char *buf, size_t sz)
{
	struct utsname u;
	if (uname(&u) == 0)
		snprintf(buf, sz, "%s", u.release);
	else
		snprintf(buf, sz, "unknown");
}

/* CPU */

static void
shorten_cpu(char *s)
{
	char *p;
	while ((p = strstr(s, "(R)"))  != NULL) memmove(p, p+3, strlen(p+3)+1);
	while ((p = strstr(s, "(TM)")) != NULL) memmove(p, p+4, strlen(p+4)+1);
	if   ((p = strstr(s, " CPU"))  != NULL) memmove(p, p+4, strlen(p+4)+1);
	if   ((p = strstr(s, " @ "))   != NULL) *p = '\0';

	if   ((p = strstr(s, "-Core")) != NULL) *p = '\0';

	while ((p = strstr(s, "  ")) != NULL) memmove(p, p+1, strlen(p+1)+1);

	size_t n = strlen(s);
	while (n > 0 && s[n-1] == ' ') s[--n] = '\0';
}

static void
get_cpu(char *buf, size_t sz)
{
#ifdef __APPLE__
	size_t len = sz;
	if (sysctlbyname("machdep.cpu.brand_string", buf, &len, NULL, 0) != 0) {
		snprintf(buf, sz, "unknown");
		return;
	}
	shorten_cpu(buf);
#else
	FILE *f;
	char line[256];

	f = fopen("/proc/cpuinfo", "r");
	if (!f) {
		snprintf(buf, sz, "unknown");
		return;
	}
	while (fgets(line, sizeof(line), f)) {
		if (strncmp(line, "model name", 10) == 0) {
			char *colon = strchr(line, ':');
			if (colon) {
				colon++;
				while (*colon == ' ') colon++;
				chomp(colon);
				snprintf(buf, sz, "%s", colon);
				shorten_cpu(buf);
				fclose(f);
				return;
			}
		}
	}
	fclose(f);
	snprintf(buf, sz, "unknown");
#endif
}

/* RAM */

static void
get_ram(char *used_buf, size_t used_sz, char *total_buf, size_t total_sz)
{
#ifdef __APPLE__
	int64_t total = 0;
	size_t len = sizeof(total);
	if (sysctlbyname("hw.memsize", &total, &len, NULL, 0) != 0) {
		snprintf(used_buf,  used_sz,  "?");
		snprintf(total_buf, total_sz, "?");
		return;
	}

	vm_size_t page_size = 0;
	mach_port_t host = mach_host_self();
	host_page_size(host, &page_size);

	vm_statistics64_data_t vmstat;
	mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
	if (host_statistics64(host, HOST_VM_INFO64,
	                       (host_info64_t)&vmstat, &count) != KERN_SUCCESS) {
		snprintf(used_buf,  used_sz,  "?");
		snprintf(total_buf, total_sz, "?");
		return;
	}

	uint64_t used_bytes = (uint64_t)(vmstat.active_count +
	                                  vmstat.wire_count +
	                                  vmstat.compressor_page_count) *
	                                  (uint64_t)page_size;

	double total_gib = total      / (1024.0 * 1024.0 * 1024.0);
	double used_gib  = used_bytes / (1024.0 * 1024.0 * 1024.0);
	snprintf(used_buf,  used_sz,  "%.2f", used_gib);
	snprintf(total_buf, total_sz, "%.2f", total_gib);
#else
	FILE *f;
	char line[256];
	unsigned long long total = 0, memfree = 0, buffers = 0;
	unsigned long long cached = 0, sreclaimable = 0, shmem = 0;

	f = fopen("/proc/meminfo", "r");
	if (!f) {
		snprintf(used_buf,  used_sz,  "?");
		snprintf(total_buf, total_sz, "?");
		return;
	}
	while (fgets(line, sizeof(line), f)) {
		if      (strncmp(line, "MemTotal:",     9)  == 0) sscanf(line + 9,  "%llu", &total);
		else if (strncmp(line, "MemFree:",      8)  == 0) sscanf(line + 8,  "%llu", &memfree);
		else if (strncmp(line, "Buffers:",      8)  == 0) sscanf(line + 8,  "%llu", &buffers);
		else if (strncmp(line, "Cached:",       7)  == 0 &&
		         strncmp(line, "SwapCached:",  11)  != 0) sscanf(line + 7,  "%llu", &cached);
		else if (strncmp(line, "SReclaimable:", 13) == 0) sscanf(line + 13, "%llu", &sreclaimable);
		else if (strncmp(line, "Shmem:",        6)  == 0) sscanf(line + 6,  "%llu", &shmem);
	}
	fclose(f);

	unsigned long long usedDiff = memfree + cached + sreclaimable + buffers;
	unsigned long long used_kb  = (total >= usedDiff) ? total - usedDiff : total - memfree;
	used_kb += shmem;
	double total_gib = total   / (1024.0 * 1024.0);
	double used_gib  = used_kb / (1024.0 * 1024.0);
	snprintf(used_buf,  used_sz,  "%.2f", used_gib);
	snprintf(total_buf, total_sz, "%.2f", total_gib);
#endif
}

/* Uptime */

static void
get_uptime(char *buf, size_t sz)
{
	long up;
#ifdef __APPLE__
	struct timeval boottime;
	size_t len = sizeof(boottime);
	int mib[2] = { CTL_KERN, KERN_BOOTTIME };
	if (sysctl(mib, 2, &boottime, &len, NULL, 0) != 0) {
		snprintf(buf, sz, "unknown");
		return;
	}
	up = time(NULL) - boottime.tv_sec;
#else
	struct sysinfo si;
	if (sysinfo(&si) != 0) {
		snprintf(buf, sz, "unknown");
		return;
	}
	up = si.uptime;
#endif
	int days  = up / 86400;
	int hours = (up % 86400) / 3600;
	int mins  = (up % 3600)  / 60;

	if (days > 0)
		snprintf(buf, sz, "%dd %dh %dm", days, hours, mins);
	else if (hours > 0)
		snprintf(buf, sz, "%dh %dm", hours, mins);
	else
		snprintf(buf, sz, "%dm", mins);
}

/* Shell */

static void
get_shell(char *buf, size_t sz)
{
	const char *s = getenv("SHELL");
	if (s) {
		const char *b = strrchr(s, '/');
		snprintf(buf, sz, "%s", b ? b + 1 : s);
	} else {
		snprintf(buf, sz, "unknown");
	}
}


/* Disk */

static void
get_disk(char *buf, size_t sz)
{
	struct statvfs st;
	if (statvfs("/", &st) != 0) {
		snprintf(buf, sz, "unknown");
		return;
	}
	unsigned long long total = (unsigned long long)st.f_blocks * st.f_frsize;
	unsigned long long free  = (unsigned long long)st.f_bfree  * st.f_frsize;
	unsigned long long used  = total - free;

	double used_gib  = used  / (1024.0 * 1024.0 * 1024.0);
	double total_gib = total / (1024.0 * 1024.0 * 1024.0);
	snprintf(buf, sz, "%.1f / %.1f GiB", used_gib, total_gib);
}

/*
 * GPU
 */

static void
get_gpu(char *buf, size_t sz)
{
#ifdef __APPLE__
	/* No /sys/class/drm on Darwin. Real detection needs IOKit; on Apple
	 * Silicon the GPU is part of the SoC, so the CPU brand string is a
	 * reasonable stand-in without pulling in IOKit/CoreFoundation. */
	char brand[128] = {0};
	size_t len = sizeof(brand);
	if (sysctlbyname("machdep.cpu.brand_string", brand, &len, NULL, 0) == 0)
		snprintf(buf, sz, "%s (integrated)", brand);
	else
		snprintf(buf, sz, "unknown");
#else
	FILE *f;
	char line[256];
	char driver[64] = {0};
	char pci_id[16] = {0};

	char uevent_path[64];
	for (int card = 0; card <= 1; card++) {
		snprintf(uevent_path, sizeof(uevent_path),
		         "/sys/class/drm/card%d/device/uevent", card);
		f = fopen(uevent_path, "r");
		if (f) break;
	}
	if (!f) {
		snprintf(buf, sz, "unknown");
		return;
	}
	while (fgets(line, sizeof(line), f)) {
		chomp(line);
		if (strncmp(line, "DRIVER=", 7) == 0) {
			snprintf(driver, sizeof(driver), "%.63s", line + 7);
		} else if (strncmp(line, "PCI_ID=", 7) == 0) {
			snprintf(pci_id, sizeof(pci_id), "%.9s", line + 7);
		}
	}
	fclose(f);

	if (!pci_id[0] && !driver[0]) {
		snprintf(buf, sz, "unknown");
		return;
	}

	const char *vendor = "";
	if      (strncasecmp(pci_id, "8086", 4) == 0) vendor = "Intel";
	else if (strncasecmp(pci_id, "1002", 4) == 0) vendor = "AMD";
	else if (strncasecmp(pci_id, "10de", 4) == 0) vendor = "NVIDIA";

	char model[128] = {0};
	char label_path[64];
	snprintf(label_path, sizeof(label_path),
	         "/sys/class/drm/card%d/device/label",
	         strstr(uevent_path, "card1") ? 1 : 0);
	f = fopen(label_path, "r");
	if (f) {
		if (fgets(model, sizeof(model), f))
			chomp(model);
		fclose(f);
	}

	if (model[0])
		snprintf(buf, sz, "%s", model);
	else if (vendor[0] && driver[0])
		snprintf(buf, sz, "%s (%s)", vendor, driver);
	else if (vendor[0])
		snprintf(buf, sz, "%s", vendor);
	else
		snprintf(buf, sz, "%s", driver);
#endif
}

/* ASCII art + labels (macOS Terminal-style small logo) */

static const char *rabbit[] = {
	"        .:'          %s@%s",
	"    __ :'__          -----------",
	" .'`  `-'  ``.       os: %s",
	":          .-'       kernel: %s",
	":         :          shell: %s",
	" :         `-;       uptime: %s",
	"  `.__.-.__.'        cpu: %s",
	"                     ram: %s / %s GiB",
	"                     disk: %s",
	"                     gpu: %s",
	NULL
};

/* main */

int
main(void)
{
	char os[128], kernel[128], cpu[256];
	char shell[64], uptime[64];
	char ram_used[32], ram_total[32];
	char disk[64], gpu[128];
	char hostname[64] = {0};
	const char *user;

	get_os(os, sizeof(os));
	get_kernel(kernel, sizeof(kernel));
	get_cpu(cpu, sizeof(cpu));
	get_shell(shell, sizeof(shell));
	get_uptime(uptime, sizeof(uptime));
	get_ram(ram_used, sizeof(ram_used), ram_total, sizeof(ram_total));
	get_disk(disk, sizeof(disk));
	get_gpu(gpu, sizeof(gpu));

	gethostname(hostname, sizeof(hostname) - 1);
	char *dot = strchr(hostname, '.');
	if (dot) *dot = '\0';

	user = getenv("USER");
	if (!user) user = getenv("LOGNAME");
	if (!user) user = "unknown";

	printf(rabbit[0], user, hostname);      putchar('\n');
	printf("%s\n", rabbit[1]);
	printf(rabbit[2], os); putchar('\n');
	printf(rabbit[3], kernel); putchar('\n');
	printf(rabbit[4], shell); putchar('\n');
	printf(rabbit[5], uptime); putchar('\n');
	printf(rabbit[6], cpu); putchar('\n');
	printf(rabbit[7], ram_used, ram_total); putchar('\n');
	printf(rabbit[8], disk); putchar('\n');
	printf(rabbit[9], gpu); putchar('\n');

	return 0;
}
