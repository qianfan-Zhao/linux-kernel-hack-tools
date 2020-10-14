/*
 * extract symbol version from linux Image bin file.
 * qianfan Zhao 2020-10-13
 * based on:
 * https://github.com/glandium/extract-symvers/blob/master/extract-symvers.py
 *
 * 原理: linux内核中symbol version存在如下几个sections中, 根据常量@arr的特征值,
 * 从固件中寻找@arr, 之后逐个导出struct kernel_symbol.
 *
 * struct kernel_symbol {
 * 	unsigned long value;
 * 	const char *name;
 * 	const char *namespace;
 * };
 *
 * struct symsearch {
 * 	const struct kernel_symbol *start, *stop;
 * 	const int32_t *crcs;
 * 	enum mod_license {
 * 		NOT_GPL_ONLY,
 * 		GPL_ONLY,
 * 		WILL_BE_GPL_ONLY,
 * 	} license;
 * 	uint8_t unused;
 * };
 *
 * static bool each_symbol_section(bool (*fn)(const struct symsearch *arr,
 * 				   struct module *owner,
 * 				   void *data),
 * 				   void *data)
 * {
 * 	 struct module *mod;
 * 	 static const struct symsearch arr[] = {
 * 		 { __start___ksymtab, __stop___ksymtab, __start___kcrctab,
 * 		   NOT_GPL_ONLY, false },
 * 		 { __start___ksymtab_gpl, __stop___ksymtab_gpl,
 * 		   __start___kcrctab_gpl,
 * 		   GPL_ONLY, false },
 * 		 { __start___ksymtab_gpl_future, __stop___ksymtab_gpl_future,
 * 		   __start___kcrctab_gpl_future,
 * 		   WILL_BE_GPL_ONLY, false },
 * #ifdef CONFIG_UNUSED_SYMBOLS
 * 		 { __start___ksymtab_unused, __stop___ksymtab_unused,
 * 		   __start___kcrctab_unused,
 * 		   NOT_GPL_ONLY, true },
 * 		 { __start___ksymtab_unused_gpl, __stop___ksymtab_unused_gpl,
 * 		   __start___kcrctab_unused_gpl,
 * 		   GPL_ONLY, true },
 * #endif
 * };
 *
 * .rodata:C090350C arr.45530       DCD __start___ksymtab
 * .rodata:C0903510                 DCD __start___ksymtab_gpl
 * .rodata:C0903514                 DCD __start___kcrctab_unused
 * .rodata:C0903518                 ALIGN 0x10
 * .rodata:C0903520                 DCD __start___ksymtab_gpl
 * .rodata:C0903524                 DCD __start___kcrctab_unused
 * .rodata:C0903528                 DCD __start___kcrctab_unused
 * .rodata:C090352C                 DCB    1
 * .rodata:C090352D                 DCB    0
 * .rodata:C090352E                 DCB    0
 * .rodata:C090352F                 DCB    0
 * .rodata:C0903530                 DCB    0
 * .rodata:C0903531                 DCB    0
 * .rodata:C0903532                 DCB    0
 * .rodata:C0903533                 DCB    0
 * .rodata:C0903534                 DCD __start___kcrctab_unused
 * .rodata:C0903538                 DCD __start___kcrctab_unused
 * .rodata:C090353C                 DCD __start___kcrctab_unused
 * .rodata:C0903540                 DCB    2
 * .rodata:C0903541                 DCB    0
 * .rodata:C0903542                 DCB    0
 * .rodata:C0903543                 DCB    0
 * .rodata:C0903544                 DCB    0
 * .rodata:C0903545                 DCB    0
 * .rodata:C0903546                 DCB    0
 * .rodata:C0903547                 DCB    0
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>

static uint64_t text = 0xc0008000; /* dmesg | grep .text */
static int le = 1; /* big endian or little endian */
static int bits = 32; /* 32 bits or 64 bits */
#define ptr_bytes() ((bits) / 8)

/* 导出的信息中带函数地址 */
static int with_address = 0;

/*
 * linux 5.x从commit 8651ec01daedad26290f76beeb4736f9d2da4b87开始, 对
 * struct kernel_symbol增加了namespace定义.
 */
static int linux5 = 0;

struct Image {
	uint8_t			*buffer;
	size_t			size;
	size_t			offset;
};

static uint64_t peek_number(struct Image *img, size_t offset, int size, int *error)
{
	uint64_t number = 0;

	if (offset + size > img->size) {
		fprintf(stderr, "Error: %s: size limited\n", __func__);
		fprintf(stderr, "offset = %lx, size = %d, filesize = %lx\n",
			offset, size, img->size);
		*error = 1;
		return 0;
	}

	if (le) {
		uint8_t *p = img->buffer + offset;

		for (int s = 0; s < size; s++) {
			uint8_t n = *p++;

			number |= (n << (s * 8));
		}
	} else {
		uint8_t *p = img->buffer + offset + size - 1;

		for (int s = size; s > 0; s--) {
			uint8_t n = *p--;

			number |= (n << (s * 8));
		}
	}

	if (bits == 32)
		number &= 0xffffffff;

	return number;
}

#define get_function_defines(name, type, size)					\
static type peek_##name(struct Image *img, int *error)				\
{										\
	return (type)peek_number(img, img->offset, size, error);		\
}										\
static type peek_##name##_at(struct Image *img, size_t offset, int *error)	\
{										\
	return (type)peek_number(img, offset, size, error);			\
}										\
static type get_##name(struct Image *img, int *error)				\
{										\
	type a = peek_##name(img, error);					\
	if (*error == 0)							\
		img->offset += size;						\
	return a;								\
}

get_function_defines(ptr, uint64_t, bits / 8);
get_function_defines(uint32, uint32_t, 4);

#define KERNEL_FUNCTION_NAME_STRING_LENGTH_LIMIT		128

static const char *peek_string_at(struct Image *img, size_t offset)
{
	const char *str = (const char *)img->buffer + offset;

	for (int i = 0; i < KERNEL_FUNCTION_NAME_STRING_LENGTH_LIMIT; i++) {
		int c = str[i];

		if (c == '\0')
			return str;
		if (!isascii(c))
			return NULL;
	}

	return NULL;
}

enum mod_license {
	NOT_GPL_ONLY,
	GPL_ONLY,
	WILL_BE_GPL_ONLY,
} license;

static size_t sizeof_struct_kernel_symbol(void)
{
	size_t s = 4 + ptr_bytes(); /* value, name */

	if (linux5)
		s += ptr_bytes(); /* namespace */

	return s;
}

static int peek_kernel_symbol(struct Image *img, size_t offt, uint32_t *p_value,
			      const char **p_name)
{
	uint64_t ptr_name;
	const char *name;
	uint32_t v;
	int error = 0;

	v = peek_uint32_at(img, offt, &error);
	offt += sizeof(v);
	if (error)
		return -1;

	ptr_name = peek_ptr_at(img, offt, &error);
	offt += ptr_bytes();
	if (error)
		return -1;

	name = peek_string_at(img, (size_t)(ptr_name - text));
	if (!name)
		return -1;

	*p_name = name;
	*p_value = v;
	return 0;
}

static const char *print_kernel_symbol_format(void)
{
	if (with_address)
		return "0x%08x\t0x%08x\t%s\tvmlinux\t%s\n";
	return "0x%08x\t%s\tvmlinux\t%s\n";
}

static void print_kernel_symbol(uint32_t addr, uint32_t crc, const char *name,
				enum mod_license license)
{
	static const char *string_licenses[] = {
		"EXPORT_SYMBOL",
		"EXPORT_SYMBOL_GPL",
		"EXPORT_SYMBOL_GPL_FUTURE"
	};
	const char *sl = string_licenses[license];

	if (with_address)
		printf(print_kernel_symbol_format(), addr, crc, name, sl);
	else
		printf(print_kernel_symbol_format(), crc, name, sl);
}

static int check_print_kernel_symbols(int check_only, struct Image *img,
				      enum mod_license license,
				      size_t ptr_start_offt,
				      size_t ptr_stop_offt,
				      size_t ptr_crcs_offt)
{
	size_t crc_offt = ptr_crcs_offt;
	size_t sym_offt = ptr_start_offt;
	int ret = 0;

	while (sym_offt < ptr_stop_offt) {
		const char *name = NULL;
		uint32_t value, crc;
		int error = 0;

		ret = peek_kernel_symbol(img, sym_offt, &value, &name);
		if (ret < 0)
			return ret;

		crc = peek_uint32_at(img, crc_offt, &error);
		if (error)
			return error;

		if (!check_only)
			print_kernel_symbol(value, crc, name, license);

		sym_offt += sizeof_struct_kernel_symbol();
		crc_offt += sizeof(crc);
	}

	return ret;
}

static int check_print_all_kernel_symbols(int check_only, struct Image *img,
					  size_t symsearch_arr)
{
	size_t offt = symsearch_arr;
	int ret = 0;

	for (enum mod_license l = NOT_GPL_ONLY; l <= WILL_BE_GPL_ONLY; l++) {
		uint64_t start, stop, crcs;
		uint32_t license, unused;
		int error = 0;

		start = peek_ptr_at(img, offt, &error);
		offt += ptr_bytes();
		stop =  peek_ptr_at(img, offt, &error);
		offt += ptr_bytes();
		crcs =  peek_ptr_at(img, offt, &error);
		offt += ptr_bytes();
		license = peek_uint32_at(img, offt, &error);
		offt += sizeof(license);
		unused  = peek_uint32_at(img, offt, &error);
		offt += sizeof(unused);

		ret = check_print_kernel_symbols(check_only, img, l,
						 (size_t)(start - text),
						 (size_t)(stop - text),
						 (size_t)(crcs - text));
		if (ret < 0)
			return ret;
	}

	return ret;
}

static int ptr_is_valid(struct Image *img, uint64_t ptr)
{
	uint64_t max_ptr = text + img->size;

	/* Image中的任何一个指针的范围应该在文件大小以内 */
	return ptr >= text && ptr < max_ptr;
}

static int check_symsearch_array(struct Image *img, enum mod_license l)
{
	uint64_t start, stop, crcs;
	uint32_t license, unused;
	int error = 0;

	start = get_ptr(img, &error);
	if (!ptr_is_valid(img, start) || error)
		return -1;

	stop = get_ptr(img, &error);
	if (!ptr_is_valid(img, stop) || error)
		return -1;

	crcs = get_ptr(img, &error);
	if (!ptr_is_valid(img, crcs) || error)
		return -1;

	license = get_uint32(img, &error);
	if (license != l || error)
		return -1;

	unused = get_uint32(img, &error);
	if (unused != 0 || error)
		return -1;

	if ((stop - start) % sizeof_struct_kernel_symbol() != 0)
		return -1;

	return 0;
}

/*
 * 从Image中扫描static const struct symsearch arr中的第一个元素ksymtab.
 * 并返回位于Image中的offset.
 */
static size_t scan_symsearch_array_ksymtab(struct Image *img, int *found)
{
	size_t offset;

	while (img->offset < img->size) {
		offset = img->offset;

		if (!check_symsearch_array(img, NOT_GPL_ONLY)) {
			*found = 1;
			return offset;
		}
	}

	return 0;
}

static int check_symsearch_array_ksymtab_gpl(struct Image *img)
{
	return check_symsearch_array(img, GPL_ONLY);
}

static int check_symsearch_array_ksymtab_gpl_future(struct Image *img)
{
	return check_symsearch_array(img, WILL_BE_GPL_ONLY);
}

/*
 * 从Image中扫描static const struct symsearch arr并返回指针.
 */
static int scan_dump_symsearch_array(struct Image *img)
{
	int ret = -1;

	while (img->offset < img->size) {
		int found = 0;
		size_t arr;

		arr = scan_symsearch_array_ksymtab(img, &found);
		if (!found)
			continue;

		if (!check_symsearch_array_ksymtab_gpl(img) &&
		    !check_symsearch_array_ksymtab_gpl_future(img)) {
			ret = check_print_all_kernel_symbols(1, img, arr);
			if (!ret) { /* check pass */
				check_print_all_kernel_symbols(0, img, arr);
				break;
			}
		}
	}

	return ret;
}

static int load_image(struct Image *img, const char *filename)
{
	FILE *fp = fopen(filename, "rb");
	long filesize;

	if (!fp) {
		fprintf(stderr, "load image %s failed: %m\n", filename);
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	filesize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	img->buffer = malloc(filesize);
	if (!img->buffer) {
		fprintf(stderr, "memory limited\n");
		fclose(fp);
		return -1;
	}

	img->size = fread(img->buffer, 1, filesize, fp);
	fclose(fp);

	return 0;
}

static void free_image(struct Image *img)
{
	free(img->buffer);
}

static void usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [OPTION] Image\n", prog);
	fprintf(stderr, "-t, --text	: Set the kernel's text base address, default is 0xC0008000\n");
	fprintf(stderr, "-e, --endian	: Endianness (be|le) ; defaults to le\n");
	fprintf(stderr, "-b, --bits	: Size of pointers in bits ; defaults to 32\n");
	fprintf(stderr, "-5, --linux5	: Kernel version is linux 5.x\n");
	fprintf(stderr, "-a, --address	: With address informations\n");
}

int main(int argc, char **argv)
{
	struct Image img = { .offset = 0 };
	const char *prog = argv[0];
	int ret = -1;

	while (1) {
		int c, option_index = 0;
		static struct option long_options[] = {
			{"text",    required_argument, 0,  't' },
			{"endian",  required_argument, 0,  'e' },
			{"bits",    required_argument, 0,  'b' },
			{"linux5",  no_argument,       0,  '5' },
			{"address", no_argument,       0,  'a' },
			{0,         0,                 0,  0   }
		};

		c = getopt_long(argc, argv, "t:e:b:5ah",
				long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 't':
			text = strtoul(optarg, NULL, 16);
			break;
		case 'e':
			if (!strcmp(optarg, "be"))
				le = 0;
			break;
		case 'b':
			bits = atoi(optarg);
			break;
		case '5':
			linux5 = 1;
			break;
		case 'a':
			with_address = 1;
			break;
		case 'h':
			ret = 0;
			/* fallthrough */
		default:
			usage(prog);
			return ret;
		}
	}

	if (optind >= argc) {
		usage(prog);
		return ret;
	}

	if (!load_image(&img, argv[optind])) {
		ret = scan_dump_symsearch_array(&img);
		free_image(&img);
	}

	return ret;
}
