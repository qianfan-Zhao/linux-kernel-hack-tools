extract_symvers_from_Image
==========================

从linux的Image镜像中提取内核符号表及CRC校验. 支持linux 5.x及之前的版本.

### 编译

```console
$ gcc extract_symvers_from_Image.c -Wall -o extract_symvers_from_Image
```

### 提取linux 5.x版本

```console
$ ./extract_symvers_from_Image ~/linux/arch/arm/boot/Image --linux5 | head
0x8204c177      I_BDEV  vmlinux EXPORT_SYMBOL
0x4c416eb9      LZ4_decompress_fast     vmlinux EXPORT_SYMBOL
0xe99b7111      LZ4_decompress_fast_continue    vmlinux EXPORT_SYMBOL
0xb78debe3      LZ4_decompress_fast_usingDict   vmlinux EXPORT_SYMBOL
0xc7c1107a      LZ4_decompress_safe     vmlinux EXPORT_SYMBOL
0xca813ce6      LZ4_decompress_safe_continue    vmlinux EXPORT_SYMBOL
0x15bed7a5      LZ4_decompress_safe_partial     vmlinux EXPORT_SYMBOL
0x8b0088d1      LZ4_decompress_safe_usingDict   vmlinux EXPORT_SYMBOL
0xadd22e70      LZ4_setStreamDecode     vmlinux EXPORT_SYMBOL
0x13e25147      PDE_DATA        vmlinux EXPORT_SYMBOL
```

### 提取linux 5.x版本并打印内核API地址

```console
./extract_symvers_from_Image ~/linux/arch/arm/boot/Image --linux5 --address | head
0xc02a8954      0x8204c177      I_BDEV  vmlinux EXPORT_SYMBOL
0xc042b480      0x4c416eb9      LZ4_decompress_fast     vmlinux EXPORT_SYMBOL
0xc042cab0      0xe99b7111      LZ4_decompress_fast_continue    vmlinux EXPORT_SYMBOL
0xc042ca68      0xb78debe3      LZ4_decompress_fast_usingDict   vmlinux EXPORT_SYMBOL
0xc042a788      0xc7c1107a      LZ4_decompress_safe     vmlinux EXPORT_SYMBOL
0xc042e5d4      0xca813ce6      LZ4_decompress_safe_continue    vmlinux EXPORT_SYMBOL
0xc042ae00      0x15bed7a5      LZ4_decompress_safe_partial     vmlinux EXPORT_SYMBOL
0xc042f138      0x8b0088d1      LZ4_decompress_safe_usingDict   vmlinux EXPORT_SYMBOL
0xc042a764      0xadd22e70      LZ4_setStreamDecode     vmlinux EXPORT_SYMBOL
0xc02eaed0      0x13e25147      PDE_DATA        vmlinux EXPORT_SYMBOL
```

### 提取linux 5.x之前版本

```console
# Nuvoton 3.10 version
./extract_symvers_from_Image ~/NUC970_Linux_Kernel/arch/arm/boot/Image | head
0x706f6f6c      I_BDEV  vmlinux EXPORT_SYMBOL
0x65705f73      PDE_DATA        vmlinux EXPORT_SYMBOL
0x696a5f72      ___pskb_trim    vmlinux EXPORT_SYMBOL
0x00796666      ___ratelimit    vmlinux EXPORT_SYMBOL
0x65736572      __aeabi_idiv    vmlinux EXPORT_SYMBOL
0x65645f74      __aeabi_idivmod vmlinux EXPORT_SYMBOL
0x65636976      __aeabi_lasr    vmlinux EXPORT_SYMBOL
0x79730073      __aeabi_llsl    vmlinux EXPORT_SYMBOL
0x6d657473      __aeabi_llsr    vmlinux EXPORT_SYMBOL
0x6174735f      __aeabi_lmul    vmlinux EXPORT_SYMBOL
```

